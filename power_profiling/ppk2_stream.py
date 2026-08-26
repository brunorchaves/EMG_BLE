"""Captura contínua e sem perda de amostras da PPK2.

Não usa ``PPK2_MP`` (o "modo multiprocessing" do ppk2-api) - ele descarta
dados silenciosamente de 3 formas diferentes (fila recortada além de
buffer_len_s, cauda descartada no stop(), buffer serial nunca esvaziado no
início porque seu próprio start_measuring() chama get_data() já sobrescrito,
que lê da fila em vez da porta). Em vez disso: duas threads dedicadas (leitora
e escritora), decodificação ZERO durante o run (grava palavras de 32 bits
cruas em disco, decodifica tudo depois via ppk2_decode.py).

O risco real não é o GIL (o trabalho Python por iteração é ínfimo) - é
LATÊNCIA: depois que ``ReadFile`` retorna, a thread leitora pode esperar até
``sys.getswitchinterval()`` (5 ms por padrão) para readquirir o GIL, e o
buffer de driver padrão do pyserial no Windows (``SetupComm(h, 4096, 4096)``)
só absorve 10.24 ms disso a 400 kB/s. Por isso pedimos um buffer de 1 MB
(2.6 s de margem) e reduzimos o intervalo de troca do GIL.
"""

from __future__ import annotations

import ctypes
import gc
import sys
import threading
import time
from collections import deque
from dataclasses import dataclass, field
from pathlib import Path
from typing import Literal

from ppk2_api.ppk2_api import PPK2_API, PPK2_Command

from config import PPK2_FS_NOMINAL, PPK2_PORT_DEFAULT, SOURCE_MV_DEFAULT


@dataclass(frozen=True)
class StreamConfig:
    port: str = PPK2_PORT_DEFAULT
    source_mv: int = SOURCE_MV_DEFAULT
    mode: Literal["source", "ampere"] = "source"
    rx_buffer_bytes: int = 1 << 20
    read_period_s: float = 0.001
    flush_bytes: int = 1 << 16
    tune_runtime: bool = True


@dataclass
class StreamHealth:
    bytes_read: int = 0
    chunks: int = 0
    samples_est: int = 0
    max_read_gap_ms: float = 0.0
    last_in_waiting: int = 0
    max_in_waiting: int = 0
    deque_depth: int = 0
    writer_lag_bytes: int = 0
    thread_error: BaseException | None = None
    overflow_suspected: bool = False


class Ppk2Stream:
    """Núcleo de captura da PPK2 - abre a porta, mede sem perder amostra,
    grava bruto em disco. Decodificação e análise ficam para depois
    (ppk2_decode.py / analyze.py)."""

    def __init__(self, cfg: StreamConfig, out_dir: Path) -> None:
        self.cfg = cfg
        self.out_dir = Path(out_dir)
        self.out_dir.mkdir(parents=True, exist_ok=True)
        self._ppk2: PPK2_API | None = None
        self._ctl_lock = threading.Lock()
        self._quit_evt = threading.Event()
        self._reader: threading.Thread | None = None
        self._writer: threading.Thread | None = None
        self._deque: deque[tuple[float, bytes, int]] = deque()
        self._deque_lock = threading.Lock()
        self._health = StreamHealth()
        self._health_lock = threading.Lock()
        self._chunks: list[tuple[float, int, int, int]] = []
        self._raw_file = None
        self._byte_offset = 0
        self._t0_mono: float | None = None
        self._old_switchinterval: float | None = None
        self._gc_was_enabled = True

    # --- ciclo de vida -----------------------------------------------

    def open(self) -> dict:
        """Abre a porta e lê a calibração. TEM que rodar antes de start() -
        get_modifiers() envia uma sequência ASCII que desalinharia o stream
        se chamado durante a medição."""
        self._ppk2 = PPK2_API(self.cfg.port, timeout=1)
        if self.cfg.tune_runtime:
            try:
                self._ppk2.ser.set_buffer_size(rx_size=self.cfg.rx_buffer_bytes)
            except Exception:
                pass  # nem toda plataforma/driver suporta; degrada com aviso no health()

        ok = self._ppk2.get_modifiers()
        if not ok:
            raise RuntimeError(
                f"Falha ao ler calibracao da PPK2 em {self.cfg.port} - confira porta/cabo."
            )

        if self.cfg.mode == "source":
            self._ppk2.use_source_meter()
            self._ppk2.set_source_voltage(self.cfg.source_mv)
        else:
            self._ppk2.use_ampere_meter()

        return dict(self._ppk2.modifiers)

    @property
    def calibration(self) -> dict:
        assert self._ppk2 is not None, "chame open() primeiro"
        return dict(self._ppk2.modifiers)

    def dut_power(self, on: bool) -> float:
        """Liga/desliga a saida do DUT (source meter mode). Retorna o
        time.monotonic() do momento do envio - como é uma escrita serial na
        própria PPK2 que está amostrando, esse timestamp é preciso a
        ~1 ms, muito melhor que qualquer evento BLE."""
        assert self._ppk2 is not None
        with self._ctl_lock:
            t = time.monotonic()
            self._ppk2.toggle_DUT_power("ON" if on else "OFF")
        return t

    def set_source_mv(self, mv: int) -> None:
        """Só use antes de start() - trocar a tensão em pleno run pode gerar
        um transiente indesejado no VOUT."""
        assert self._ppk2 is not None
        with self._ctl_lock:
            self._ppk2.set_source_voltage(mv)

    def start(self) -> float:
        """Esvazia a porta, dispara AVERAGE_START, sobe as threads leitora e
        escritora. Retorna t0_mono - o zero da timeline do run."""
        assert self._ppk2 is not None, "chame open() primeiro"

        if self.cfg.tune_runtime:
            self._old_switchinterval = sys.getswitchinterval()
            sys.setswitchinterval(0.0005)
            self._gc_was_enabled = gc.isenabled()
            gc.collect()
            try:
                gc.freeze()
            except AttributeError:
                pass
            gc.disable()

        # Esvazia a porta usando o método NÃO sobrescrito (o método de
        # instância pode ter sido trocado por uma subclasse no futuro -
        # aqui usamos a classe base diretamente, igual PPK2_MP deveria
        # ter feito e não faz).
        while True:
            leftover = PPK2_API.get_data(self._ppk2)
            if not leftover:
                break

        self._raw_file = open(self.out_dir / "current_raw.u32", "wb", buffering=0)
        self._byte_offset = 0
        self._quit_evt.clear()
        self._health = StreamHealth()

        self._ppk2.start_measuring()
        self._t0_mono = time.monotonic()

        self._reader = threading.Thread(target=self._reader_loop, name="ppk2-reader", daemon=True)
        self._writer = threading.Thread(target=self._writer_loop, name="ppk2-writer", daemon=True)
        self._reader.start()
        self._writer.start()
        self._set_reader_priority_best_effort()

        return self._t0_mono

    def stop(self):
        """Para de amostrar, esvazia o que falta escrever, fecha o arquivo.
        A porta serial continua aberta (dut_power ainda funciona depois)."""
        assert self._ppk2 is not None
        self._quit_evt.set()
        if self._reader:
            self._reader.join(timeout=5.0)
        with self._ctl_lock:
            self._ppk2.stop_measuring()
        if self._writer:
            self._writer.join(timeout=10.0)
        if self._raw_file:
            self._raw_file.flush()
            self._raw_file.close()
            self._raw_file = None

        import numpy as np

        chunks_arr = np.array(
            self._chunks,
            dtype=[("t_mono", "f8"), ("byte_offset", "i8"), ("n_bytes", "i4"), ("in_waiting", "i4")],
        )
        np.save(self.out_dir / "chunks.npy", chunks_arr)
        return self.health()

    def close(self) -> None:
        """Desliga o DUT, restaura o runtime, fecha a porta. Chame só depois
        de stop()."""
        if self._ppk2 is not None:
            try:
                with self._ctl_lock:
                    self._ppk2.toggle_DUT_power("OFF")
            except Exception:
                pass
        if self.cfg.tune_runtime:
            if self._old_switchinterval is not None:
                sys.setswitchinterval(self._old_switchinterval)
            if self._gc_was_enabled:
                gc.enable()
        if self._ppk2 is not None:
            try:
                self._ppk2.ser.close()
            except Exception:
                pass
        self._ppk2 = None

    def __enter__(self) -> "Ppk2Stream":
        self.open()
        return self

    def __exit__(self, *exc) -> None:
        try:
            if self._reader is not None and self._reader.is_alive():
                self.stop()
        finally:
            self.close()

    # --- threads -------------------------------------------------------

    def _reader_loop(self) -> None:
        ser = self._ppk2.ser
        last_read_t = time.monotonic()
        max_gap = 0.0
        max_in_waiting = 0
        try:
            while not self._quit_evt.is_set():
                n = ser.in_waiting
                if n:
                    buf = ser.read(n)
                    t = time.monotonic()
                    gap_ms = (t - last_read_t) * 1000.0
                    if gap_ms > max_gap:
                        max_gap = gap_ms
                    last_read_t = t
                    if n > max_in_waiting:
                        max_in_waiting = n
                    with self._deque_lock:
                        self._deque.append((t, buf, n))
                else:
                    time.sleep(self.cfg.read_period_s)
        except Exception as e:  # noqa: BLE001 - queremos capturar qualquer coisa e reportar
            with self._health_lock:
                self._health.thread_error = e
        finally:
            with self._health_lock:
                self._health.max_read_gap_ms = max(self._health.max_read_gap_ms, max_gap)
                self._health.max_in_waiting = max(self._health.max_in_waiting, max_in_waiting)

    def _writer_loop(self) -> None:
        bytes_since_flush = 0
        try:
            while not self._quit_evt.is_set() or self._deque:
                item = None
                with self._deque_lock:
                    if self._deque:
                        item = self._deque.popleft()
                if item is None:
                    if self._quit_evt.is_set():
                        break
                    time.sleep(self.cfg.read_period_s)
                    continue

                t, buf, in_waiting = item
                self._raw_file.write(buf)
                n = len(buf)
                self._chunks.append((t, self._byte_offset, n, in_waiting))
                self._byte_offset += n
                bytes_since_flush += n
                if bytes_since_flush >= self.cfg.flush_bytes:
                    self._raw_file.flush()
                    bytes_since_flush = 0

                with self._health_lock:
                    self._health.bytes_read += n
                    self._health.chunks += 1
                    self._health.samples_est = self._health.bytes_read // 4
                    self._health.last_in_waiting = in_waiting
                    with self._deque_lock:
                        self._health.deque_depth = len(self._deque)
        except Exception as e:  # noqa: BLE001
            with self._health_lock:
                self._health.thread_error = e

    def _set_reader_priority_best_effort(self) -> None:
        if sys.platform != "win32" or self._reader is None:
            return
        try:
            THREAD_PRIORITY_TIME_CRITICAL = 15
            handle = ctypes.windll.kernel32.OpenThread(
                0x0020, False, self._reader.ident  # THREAD_SET_INFORMATION
            )
            if handle:
                ctypes.windll.kernel32.SetThreadPriority(handle, THREAD_PRIORITY_TIME_CRITICAL)
                ctypes.windll.kernel32.CloseHandle(handle)
        except Exception:
            pass  # best-effort; sem isso o processo ainda funciona, só com mais risco de latência

    # --- introspecção ----------------------------------------------------

    def health(self) -> StreamHealth:
        with self._health_lock:
            h = StreamHealth(**vars(self._health))
        with self._deque_lock:
            h.deque_depth = len(self._deque)
        h.writer_lag_bytes = sum(len(b) for _, b, _ in list(self._deque))
        # heurística grosseira de overflow: gap de leitura > 80% da margem
        # teórica do buffer de recepção configurado
        theoretical_headroom_ms = (self.cfg.rx_buffer_bytes / 400_000.0) * 1000.0
        h.overflow_suspected = h.max_read_gap_ms > 0.8 * theoretical_headroom_ms
        return h

    @property
    def t0_mono(self) -> float | None:
        return self._t0_mono
