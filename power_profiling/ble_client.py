"""Central BLE (bleak) para o sensor EMG_BLE, com timestamps de recepção do
próprio stack Bluetooth do Windows quando disponível.

bleak, no backend WinRT, marca a chegada de cada notificação via
``loop.call_soon_threadsafe`` antes de entregar pro callback assíncrono
(bleak/backends/winrt/client.py) - isso descarta o timestamp que o próprio
Windows já tinha (``GattValueChangedEventArgs.timestamp``). Registramos um
handler bruto direto no objeto WinRT por baixo do bleak para recuperar esse
timestamp, com fallback gracioso pro callback normal do bleak se essa via
não estiver disponível (troca de versão da API, ou rodando fora do Windows).
"""

from __future__ import annotations

import asyncio
import sys
import time
from collections import deque
from dataclasses import dataclass, field
from typing import Callable

import numpy as np
from bleak import BleakClient, BleakScanner
from bleak.backends.device import BLEDevice

from config import DEVICE_NAME, EMG_SAMPLES_PER_PACKET, EMG_UUID, GAIN_UUID, MIN_REQUIRED_MTU
from timeline import Clock


@dataclass(frozen=True)
class Packet:
    t_os: float | None  # tempo (na timeline do run) da chegada, via WinRT args.timestamp
    t_host: float  # tempo do callback asyncio - só como sinal de vida
    seq_rx: int  # índice de recepção no host (o dispositivo não manda sequência)
    n_bytes: int
    gain_level: int
    samples: np.ndarray  # int16, até 60 amostras


@dataclass
class BleStats:
    n_packets: int = 0
    n_bad_length: int = 0
    n_duplicate_payload: int = 0
    mtu: int = 0
    connect_latency_s: float = 0.0
    t_os_minus_t_host_p50_ms: float = float("nan")
    t_os_minus_t_host_p95_ms: float = float("nan")
    raw_winrt_path_active: bool = False


class EmgBleClient:
    def __init__(
        self,
        clock: Clock,
        on_event: Callable[[str, dict], None],
        device_name: str = DEVICE_NAME,
    ) -> None:
        self.clock = clock
        self.on_event = on_event
        self.device_name = device_name

        self._client: BleakClient | None = None
        self._packets: deque[Packet] = deque()
        self._seq_rx = 0
        self._current_gain = 10  # valor default do firmware (main.c: gain_level = 10)
        self._raw_token = None
        self._raw_active = False
        self._raw_ts_queue: deque[float] = deque()
        self._n_bad_length = 0
        self._n_duplicate = 0
        self._last_payload: bytes | None = None
        self._t_os_minus_t_host_ms: list[float] = []

    # --- ciclo de conexão ------------------------------------------------

    async def ensure_radio_on(self) -> bool:
        """Garante que o radio Bluetooth do Windows esta ligado.

        Necessario porque durante esta bancada o adaptador Intel desligou
        sozinho repetidas vezes (provavelmente o driver se reinicializando
        depois de muitos ciclos connect/disconnect, ou gerenciamento de
        energia do Windows), fazendo o bleak falhar com
        BleakBluetoothNotAvailableError no meio de um run. Religar na mao a
        cada vez inviabilizaria qualquer captura longa.
        """
        if sys.platform != "win32":
            return True
        try:
            from winrt.windows.devices.radios import Radio, RadioKind, RadioState
        except ImportError:
            return True

        try:
            await Radio.request_access_async()
            radios = await Radio.get_radios_async()
            for r in radios:
                if r.kind == RadioKind.BLUETOOTH:
                    if r.state == RadioState.ON:
                        return True
                    self.on_event("radio_was_off", {"state": int(r.state)})
                    await r.set_state_async(RadioState.ON)
                    for _ in range(20):  # espera ate ~4s o radio subir
                        await asyncio.sleep(0.2)
                        for r2 in await Radio.get_radios_async():
                            if r2.kind == RadioKind.BLUETOOTH and r2.state == RadioState.ON:
                                self.on_event("radio_turned_on", {})
                                return True
                    return False
        except Exception as e:  # noqa: BLE001
            self.on_event("warning", {"where": "ensure_radio_on", "error": str(e)})
        return True

    async def scan(self, timeout: float = 15.0) -> BLEDevice:
        await self.ensure_radio_on()
        self.on_event("scan_start", {})

        def _match(d, adv):
            return d.name == self.device_name

        dev = await BleakScanner.find_device_by_filter(_match, timeout=timeout)
        if dev is None:
            self.on_event("scan_timeout", {"timeout": timeout})
            raise TimeoutError(f"'{self.device_name}' nao encontrado em {timeout}s")
        self.on_event("scan_found", {"address": dev.address})
        return dev

    async def connect(self, dev: BLEDevice, *, use_cached_services: bool = False, timeout: float = 20.0) -> None:
        self.on_event("connect_begin", {"address": dev.address})
        t0 = time.monotonic()
        self._client = BleakClient(dev, timeout=timeout, disconnected_callback=self._on_disconnected)
        # use_cached_services=False: bleak recomenda para periféricos DIY cuja
        # tabela GATT pode mudar entre firmwares - evita handles obsoletos.
        connect_kwargs = {}
        try:
            await self._client.connect(dangerous_use_bleak_cache=use_cached_services, **connect_kwargs)
        except TypeError:
            # versões de bleak sem esse kwarg - segue sem ele
            await self._client.connect(**connect_kwargs)
        self.on_event("connected", {"latency_s": time.monotonic() - t0})

    async def read_mtu(self, *, poll_s: float = 2.0) -> int:
        """No Windows o MTU pode aparecer como 23 até a troca de MTU
        completar - espera até poll_s por um valor maior antes de desistir."""
        assert self._client is not None
        deadline = time.monotonic() + poll_s
        mtu = self._client.mtu_size
        while mtu < MIN_REQUIRED_MTU and time.monotonic() < deadline:
            await asyncio.sleep(0.1)
            mtu = self._client.mtu_size
        self.on_event("mtu", {"mtu": mtu, "required": MIN_REQUIRED_MTU})
        return mtu

    async def subscribe(self) -> tuple[float, float]:
        assert self._client is not None
        t_before = self.clock.now()
        self._attach_raw_winrt_handler()

        async def _noop(_sender, _data):  # bleak exige um callback; a captura real é o handler bruto
            pass

        await self._client.start_notify(EMG_UUID, self._on_notify_bleak)
        t_after = self.clock.now()
        self.on_event("cccd_on", {"t_before": t_before, "t_after": t_after})
        return t_before, t_after

    async def unsubscribe(self) -> tuple[float, float]:
        assert self._client is not None
        t_before = self.clock.now()
        try:
            await self._client.stop_notify(EMG_UUID)
        finally:
            self._detach_raw_winrt_handler()
        t_after = self.clock.now()
        self.on_event("cccd_off", {"t_before": t_before, "t_after": t_after})
        return t_before, t_after

    async def write_gain(self, level: int, *, response: bool = True) -> tuple[float, float]:
        assert self._client is not None
        t_before = self.clock.now()
        await self._client.write_gatt_char(GAIN_UUID, bytes([level]), response=response)
        self._current_gain = level
        t_after = self.clock.now()
        self.on_event("gain_write", {"level": level, "t_before": t_before, "t_after": t_after})
        return t_before, t_after

    async def disconnect(self) -> None:
        if self._client is None:
            return
        self.on_event("disconnect_begin", {})
        try:
            await self._client.disconnect()
        except Exception as e:  # noqa: BLE001
            self.on_event("error", {"where": "disconnect", "error": str(e)})

    def _on_disconnected(self, _client) -> None:
        self.on_event("disconnected", {"t": self.clock.now()})

    # --- recepção de dados -----------------------------------------------

    def _attach_raw_winrt_handler(self) -> None:
        """Tenta anexar um SEGUNDO handler direto no objeto WinRT subjacente
        só para capturar ``args.timestamp`` (o instante de recepção do
        próprio stack Bluetooth do Windows). O callback padrão do bleak
        (``_on_notify_bleak``) SEMPRE processa o pacote de qualquer forma -
        esse handler bruto é estritamente um bônus de enriquecimento de
        timestamp, nunca um caminho exclusivo. Isso importa porque a API
        WinRT por baixo do bleak pode mudar entre versões (``char.obj`` não
        é uma API pública documentada) - se esse handler não disparar por
        qualquer motivo, os pacotes continuam chegando normalmente, só sem
        o t_os de bônus."""
        if sys.platform != "win32":
            return
        try:
            char = self._client.services.get_characteristic(EMG_UUID)
            winrt_char = getattr(char, "obj", None)
            if winrt_char is None:
                return

            def _raw_handler(sender, args):
                try:
                    t_os = self.clock.winrt_to_run(args.timestamp)
                    self._raw_ts_queue.append(t_os)
                except Exception:
                    pass

            self._raw_token = winrt_char.add_value_changed(_raw_handler)
            self._raw_active = True
        except Exception as e:  # noqa: BLE001
            self.on_event("warning", {"where": "raw_winrt_handler", "error": str(e)})
            self._raw_active = False

    def _detach_raw_winrt_handler(self) -> None:
        if self._raw_token is None or self._client is None:
            return
        try:
            char = self._client.services.get_characteristic(EMG_UUID)
            winrt_char = getattr(char, "obj", None)
            if winrt_char is not None:
                winrt_char.remove_value_changed(self._raw_token)
        except Exception:
            pass
        self._raw_token = None

    def _on_notify_bleak(self, _sender, data: bytearray) -> None:
        """Callback padrão do bleak - SEMPRE processa o pacote (é o caminho
        garantido, já testado contra o hardware real). Se o handler bruto
        WinRT tiver disparado para esta mesma notificação um pouco antes,
        usamos o timestamp dele; senão t_os fica None e ficamos só com
        t_host (ainda preciso a poucos ms, via callback do proprio bleak)."""
        t_os = self._raw_ts_queue.popleft() if self._raw_ts_queue else None
        self._enqueue_packet(bytes(data), t_os)

    def _enqueue_packet(self, value: bytes, t_os: float | None) -> None:
        t_host = self.clock.now()
        if t_os is not None:
            self._t_os_minus_t_host_ms.append((t_os - t_host) * 1000.0)

        n = len(value)
        if n != EMG_SAMPLES_PER_PACKET * 2:
            self._n_bad_length += 1

        if value == self._last_payload:
            self._n_duplicate += 1
        self._last_payload = value

        n_samples = n // 2
        samples = np.frombuffer(value[: n_samples * 2], dtype="<i2")

        pkt = Packet(
            t_os=t_os,
            t_host=t_host,
            seq_rx=self._seq_rx,
            n_bytes=n,
            gain_level=self._current_gain,
            samples=samples,
        )
        self._seq_rx += 1
        self._packets.append(pkt)

    def drain_packets(self) -> list[Packet]:
        out = list(self._packets)
        self._packets.clear()
        return out

    def stats(self) -> BleStats:
        p50 = float(np.percentile(self._t_os_minus_t_host_ms, 50)) if self._t_os_minus_t_host_ms else float("nan")
        p95 = float(np.percentile(self._t_os_minus_t_host_ms, 95)) if self._t_os_minus_t_host_ms else float("nan")
        mtu = 0
        if self._client is not None:
            try:
                mtu = self._client.mtu_size
            except Exception:
                pass  # sessao ja fechada (pos-disconnect) - mtu so importa durante a conexao
        return BleStats(
            n_packets=self._seq_rx,
            n_bad_length=self._n_bad_length,
            n_duplicate_payload=self._n_duplicate,
            mtu=mtu,
            t_os_minus_t_host_p50_ms=p50,
            t_os_minus_t_host_p95_ms=p95,
            raw_winrt_path_active=self._raw_active,
        )
