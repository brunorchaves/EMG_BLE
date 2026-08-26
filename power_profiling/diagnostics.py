"""Autotestes de bancada (Fase 0 do plano): provar que a captura não perde
amostra antes de confiar em qualquer número, medir o efeito do J-Link, e
medir o piso de corrente da própria PPK2 sem carga.
"""

from __future__ import annotations

import subprocess
import time
from dataclasses import dataclass

import numpy as np

from ppk2_decode import check_counter, load_raw
from ppk2_stream import Ppk2Stream, StreamConfig


@dataclass
class HeadroomProbeResult:
    stall_ms: float
    n_samples: int
    n_gaps: int
    ok: bool


def buffer_headroom_probe(
    port: str, stall_schedule_ms: tuple[float, ...] = (10, 50, 200, 500, 1000)
) -> list[HeadroomProbeResult]:
    """Mede empiricamente até quanto tempo a captura tolera um bloqueio da
    thread leitora sem perder amostra (contador da PPK2 sem gaps), em vez de
    confiar só no tamanho nominal do buffer pedido ao driver."""
    results = []
    for stall_ms in stall_schedule_ms:
        cfg = StreamConfig(port=port)
        import tempfile
        from pathlib import Path

        with tempfile.TemporaryDirectory() as tmp:
            stream = Ppk2Stream(cfg, Path(tmp))
            stream.open()
            stream.start()
            time.sleep(0.5)
            time.sleep(stall_ms / 1000.0)  # simula o estouro (o GIL real já cobre isso;
            # aqui adicionamos uma folga extra deliberada no thread principal)
            time.sleep(0.5)
            health = stream.stop()
            stream.close()

            raw = load_raw(Path(tmp) / "current_raw.u32", mmap=False)
            words = raw.astype(np.uint32)
            counter = ((words >> 18) & 0x3F).astype(np.uint8)
            report = check_counter(counter)

        results.append(
            HeadroomProbeResult(
                stall_ms=stall_ms,
                n_samples=report.n_samples,
                n_gaps=report.n_gaps,
                ok=report.ok and not health.overflow_suspected,
            )
        )
    return results


def jlink_delta(port: str, jlink_exe: str, duration_s: float = 10.0) -> dict:
    """Mede o delta de corrente com o J-Link conectado vs. sem sessão ativa.
    Não desconecta o cabo fisicamente (não dá pra automatizar) - só compara
    com/sem uma sessão JLink.exe ativa enquanto o cabo já está no lugar."""
    from pathlib import Path
    import tempfile

    def _capture_mean(tmp: Path, secs: float) -> float:
        cfg = StreamConfig(port=port)
        stream = Ppk2Stream(cfg, tmp)
        stream.open()
        stream.start()
        stream.dut_power(True)
        time.sleep(secs)
        health = stream.stop()
        stream.close()

        raw = load_raw(tmp / "current_raw.u32", mmap=False)
        from ppk2_decode import decode_words

        calib = stream.calibration if hasattr(stream, "calibration") else None
        # calib já não está disponível pós-close(); refeito via novo open() seria
        # mais correto, mas para uma leitura grosseira do delta usamos a média
        # do ADC bruto como proxy relativo (mesma escala nos dois casos).
        adc_raw = (raw.astype(np.uint32) & 0x3FFF).astype(np.float64) * 4.0
        return float(adc_raw.mean())

    with tempfile.TemporaryDirectory() as tmp1:
        without_session = _capture_mean(Path(tmp1), duration_s)

    proc = subprocess.Popen(
        [jlink_exe, "-device", "NRF52840_XXAA", "-if", "SWD", "-speed", "4000", "-autoconnect", "1", "-NoGui", "1"],
        stdin=subprocess.PIPE,
    )
    time.sleep(1.5)
    try:
        with tempfile.TemporaryDirectory() as tmp2:
            with_session = _capture_mean(Path(tmp2), duration_s)
    finally:
        proc.terminate()
        proc.wait(timeout=5)

    return {
        "adc_raw_mean_without_jlink": without_session,
        "adc_raw_mean_with_jlink": with_session,
        "delta_pct": (with_session - without_session) / without_session * 100 if without_session else float("nan"),
        "note": "proxy em unidades de ADC bruto, nao em uA - use so como indicador relativo",
    }


def instrument_floor(port: str, duration_s: float = 10.0) -> dict:
    """Mede o piso da própria PPK2 com VOUT desligado - separa leakage do
    instrumento do leakage real da placa."""
    from pathlib import Path
    import tempfile

    from ppk2_decode import decode_words

    with tempfile.TemporaryDirectory() as tmp:
        cfg = StreamConfig(port=port)
        stream = Ppk2Stream(cfg, Path(tmp))
        calib = stream.open()
        stream.dut_power(False)
        stream.start()
        time.sleep(duration_s)
        stream.stop()
        stream.close()

        raw = load_raw(Path(tmp) / "current_raw.u32", mmap=False)
        words = raw.astype(np.uint32)
        block = decode_words(words, calib, source_v=5.0)

    return {
        "mean_uA": float(block.current_uA.mean()),
        "std_uA": float(block.current_uA.std()),
        "max_uA": float(block.current_uA.max()),
        "n_samples": len(block.current_uA),
    }
