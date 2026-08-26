"""Uma timeline só, comum à PPK2 e ao BLE.

A PPK2 não tem timestamp de hardware - o tempo de cada amostra vem de
índice/fs_efetivo, com fs_efetivo estimado a partir dos timestamps de
chegada de cada chunk lido da serial (ver fit_stream_clock). Eventos do lado
BLE/host entram na mesma timeline via time.monotonic(), cada um com sua
própria incerteza declarada (Event.unc_ms) - nunca fingimos uma precisão que
não temos.

Quando o firmware tiver marcadores GPIO nos canais digitais da PPK2 (Fase 2),
reanchor_from_digital() re-ancora a timeline inteira usando bordas exatas do
mesmo stream de amostras, e o erro de alinhamento desaparece.
"""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Literal

import numpy as np


@dataclass
class Clock:
    t0_mono: float
    t0_wall: float
    t0_iso: str
    _heartbeats: list[tuple[float, float]] = field(default_factory=list)

    @classmethod
    def start_now(cls) -> "Clock":
        import time

        t_mono = time.monotonic()
        t_wall = time.time()
        iso = datetime.fromtimestamp(t_wall, tz=timezone.utc).isoformat()
        return cls(t0_mono=t_mono, t0_wall=t_wall, t0_iso=iso)

    def now(self) -> float:
        """Retorna time.monotonic() ABSOLUTO (não relativo a t0_mono).

        Importante: ppk2_stream.py grava timestamps absolutos de
        time.monotonic() nos chunks (sem subtrair nenhum offset), e
        fit_stream_clock/build_bands operam nesse mesmo referencial
        absoluto. Se Event.t fosse relativo a t0_mono, o casamento entre
        índice de amostra e evento ficaria deslocado por exatamente
        t0_mono. t0_mono continua disponível como metadado (para exibir
        duração "desde o início do run" em relatórios), mas a timeline
        interna é toda em monotonic() absoluto.
        """
        import time

        return time.monotonic()

    def wall_to_run(self, epoch_s: float) -> float:
        """Converte um timestamp de relógio de parede (time.time()) para
        time.monotonic() ABSOLUTO (mesmo referencial de ``now()``), usando o
        heartbeat mais próximo para compensar qualquer salto de NTP entre o
        início do run e agora."""
        if not self._heartbeats:
            return self.t0_mono + (epoch_s - self.t0_wall)
        wall_hb, mono_hb = min(self._heartbeats, key=lambda hb: abs(hb[0] - epoch_s))
        return mono_hb + (epoch_s - wall_hb)

    def winrt_to_run(self, dt: datetime) -> float:
        return self.wall_to_run(dt.timestamp())

    def heartbeat(self) -> None:
        import time

        self._heartbeats.append((time.time(), time.monotonic()))
        if len(self._heartbeats) > 10_000:
            self._heartbeats = self._heartbeats[-5000:]

    def steps(self) -> list[tuple[float, float]]:
        """Detecta saltos de relógio de parede entre heartbeats consecutivos
        (>5ms de discrepância vs. o monotonic) - sinal de ajuste de NTP."""
        out = []
        for (w0, m0), (w1, m1) in zip(self._heartbeats, self._heartbeats[1:]):
            drift = (w1 - w0) - (m1 - m0)
            if abs(drift) > 0.005:
                out.append((w1, drift))
        return out


@dataclass(frozen=True)
class Event:
    t: float
    kind: str
    state: str | None
    source: Literal["host", "winrt", "ppk2_ctl", "ppk2_digital", "derived"]
    unc_ms: float
    detail: dict = field(default_factory=dict)


class EventLog:
    def __init__(self) -> None:
        self._events: list[Event] = []

    def add(
        self,
        kind: str,
        *,
        t: float,
        state: str | None = None,
        source: str = "host",
        unc_ms: float = 25.0,
        **detail,
    ) -> Event:
        ev = Event(t=t, kind=kind, state=state, source=source, unc_ms=unc_ms, detail=detail)
        self._events.append(ev)
        return ev

    @property
    def events(self) -> list[Event]:
        return list(self._events)

    def to_jsonl(self, path: Path) -> None:
        with open(path, "a", encoding="utf-8") as f:
            for ev in self._events:
                f.write(json.dumps(asdict(ev), ensure_ascii=False) + "\n")

    def flush_new_to_jsonl(self, path: Path, written: int) -> int:
        """Escreve só os eventos além de `written` (uso incremental durante
        o run, para o log sobreviver a um crash)."""
        with open(path, "a", encoding="utf-8") as f:
            for ev in self._events[written:]:
                f.write(json.dumps(asdict(ev), ensure_ascii=False) + "\n")
        return len(self._events)

    @classmethod
    def from_jsonl(cls, path: Path) -> "EventLog":
        log = cls()
        with open(path, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                d = json.loads(line)
                log._events.append(Event(**d))
        return log


@dataclass(frozen=True)
class Band:
    state: str
    t_start: float
    t_end: float
    i_start: int
    i_end: int
    guard_s: float
    boundary_source: Literal["host", "digital", "refined"]
    boundary_unc_ms: float
    has_gap: bool = False
    gain_level: int | None = None

    @property
    def duration_s(self) -> float:
        return self.t_end - self.t_start

    def guarded_indices(self, fs: float) -> tuple[int, int]:
        guard_n = int(round(self.guard_s * fs))
        lo = self.i_start + guard_n
        hi = self.i_end - guard_n
        return (lo, hi) if hi > lo else (self.i_start, self.i_end)


@dataclass(frozen=True)
class ClockFit:
    fs_effective: float
    t_offset: float
    drift_ppm: float
    latency_p50_ms: float
    latency_p95_ms: float
    latency_max_ms: float
    n_chunks: int
    method: str
    ok: bool


def fit_stream_clock(chunks: np.ndarray, fs_nominal: float = 100_000.0) -> ClockFit:
    """Estima (offset, fs_efetivo) do stream da PPK2 a partir dos timestamps
    de chegada de cada chunk lido da serial.

    Modelo: t_k = a + n_k/fs + d_k, com d_k >= 0 (latência de
    transporte USB/driver, sempre atrasando, nunca antecipando). Por isso
    NÃO usamos OLS simples (que embutiria a latência MÉDIA no offset,
    enviesando todo timestamp de amostra por alguns ms) - usamos o envelope
    inferior: ajusta, desloca o intercepto para o resíduo mínimo, refina
    iterando só no decil de resíduos mais baixo.
    """
    if len(chunks) < 5:
        t0 = float(chunks["t_mono"][0]) if len(chunks) else 0.0
        return ClockFit(fs_nominal, t0, 0.0, 0.0, 0.0, 0.0, len(chunks), "dados_insuficientes", False)

    t = chunks["t_mono"].astype(np.float64)
    n_bytes_cum = (chunks["byte_offset"] + chunks["n_bytes"]).astype(np.float64)
    x = n_bytes_cum / 4.0  # índice de amostra cumulativo

    def _ols(tt, xx):
        A = np.vstack([np.ones_like(xx), xx]).T
        coef, *_ = np.linalg.lstsq(A, tt, rcond=None)
        return coef  # (a, b) tal que t = a + b*x

    a, b = _ols(t, x)
    resid = t - (a + b * x)
    a += resid.min()
    resid = t - (a + b * x)

    for _ in range(3):
        thresh = np.quantile(resid, 0.10)
        mask = resid <= thresh
        if mask.sum() < 5:
            break
        a, b = _ols(t[mask], x[mask])
        resid_full = t - (a + b * x)
        a += resid_full.min()
        resid = t - (a + b * x)

    if b <= 0:
        return ClockFit(fs_nominal, float(t[0]), 0.0, 0.0, 0.0, 0.0, len(chunks), "fit_invalido", False)

    fs_hat = 1.0 / b
    drift_ppm = 1e6 * (fs_hat / fs_nominal - 1.0)
    lat_ms = resid * 1000.0

    return ClockFit(
        fs_effective=float(fs_hat),
        t_offset=float(a),
        drift_ppm=float(drift_ppm),
        latency_p50_ms=float(np.percentile(lat_ms, 50)),
        latency_p95_ms=float(np.percentile(lat_ms, 95)),
        latency_max_ms=float(lat_ms.max()),
        n_chunks=len(chunks),
        method="lower_envelope_iterativo",
        ok=True,
    )


def sample_index_to_time(idx: np.ndarray | int, fit: ClockFit) -> np.ndarray | float:
    return fit.t_offset + np.asarray(idx) / fit.fs_effective


def time_to_sample_index(t: np.ndarray | float, fit: ClockFit) -> np.ndarray | int:
    return np.round((np.asarray(t) - fit.t_offset) * fit.fs_effective).astype(np.int64)


def build_bands(log: EventLog, seq: list[str], fit: ClockFit, guard_s: float = 0.25) -> list[Band]:
    """Constrói as bandas a partir dos eventos ``state.enter`` do log, na
    ordem esperada (``seq``). Cada banda vai do enter do estado até o
    próximo evento ``state.enter`` QUALQUER (não só os que estão em
    ``seq``) - importante porque o marcador de fim de run ("RUN_END") não
    está em ``seq`` mas precisa fechar a última banda (ex: OFF_FINAL);
    sem isso a última banda sempre ficaria com duração zero."""
    enters = [e for e in log.events if e.kind == "state.enter"]
    bands: list[Band] = []
    for i, ev in enumerate(enters):
        if ev.state not in seq:
            continue
        t_start = ev.t
        t_end = enters[i + 1].t if i + 1 < len(enters) else t_start
        i_start = int(time_to_sample_index(t_start, fit))
        i_end = int(time_to_sample_index(t_end, fit))
        bands.append(
            Band(
                state=ev.state,
                t_start=t_start,
                t_end=t_end,
                i_start=max(0, i_start),
                i_end=max(i_start, i_end),
                guard_s=guard_s,
                boundary_source="host",
                boundary_unc_ms=ev.unc_ms,
                gain_level=ev.detail.get("gain_level"),
            )
        )
    return bands


def refine_event_by_changepoint(
    uA: np.ndarray, fs: float, t_guess: float, search_ms: float = 150.0
) -> tuple[float, float]:
    """Refina o instante de uma transição usando detecção de changepoint
    (CUSUM) numa janela ``t_guess ± search_ms`` sobre a corrente. Retorna
    (t_refinado, confiança em [0,1])."""
    half_n = int(round((search_ms / 1000.0) * fs))
    center = int(round(t_guess * fs))
    lo, hi = max(0, center - half_n), min(len(uA), center + half_n)
    if hi - lo < 4:
        return t_guess, 0.0

    window = uA[lo:hi].astype(np.float64)
    mean = window.mean()
    cusum = np.cumsum(window - mean)
    idx = int(np.argmax(np.abs(cusum)))
    t_refined = (lo + idx) / fs

    noise = window.std() or 1e-9
    magnitude = np.abs(cusum[idx]) / (len(window) * noise)
    confidence = float(np.clip(magnitude, 0.0, 1.0))
    return t_refined, confidence


@dataclass
class ReanchorReport:
    n_markers_found: int
    n_matched: int
    alpha: float
    beta: float
    delta_p50_ms: float
    delta_p95_ms: float
    ok: bool


def reanchor_from_digital(
    logic: np.ndarray,
    fit: ClockFit,
    log: EventLog,
    state_code: dict[str, int],
    marker_channel: int = 0,
) -> tuple[list[Band], ReanchorReport]:
    """Re-ancora a timeline usando os marcadores GPIO decodificados dos
    canais digitais (requer o build instrumentado da Fase 2 - sem firmware
    com marcadores, chame só com dados sintéticos/teste).

    Implementação: decodifica bordas de subida no canal ``marker_channel``,
    casa contra a sequência de eventos ``state.enter`` do host por ordem
    (não por valor - o esquema de codificação de N pulsos por estado é
    decodificado por quem chama e passado via detail['code'] nos eventos,
    se disponível), ajusta uma correção linear (alpha + beta*t) e devolve
    bandas com boundary_source="digital" e incerteza de amostra única.
    """
    from ppk2_decode import digital_bit, find_edges

    bit = digital_bit(logic, marker_channel)
    edges = find_edges(bit, "rising")
    t_markers = sample_index_to_time(edges, fit)

    enters = [e for e in log.events if e.kind == "state.enter"]
    n = min(len(t_markers), len(enters))
    if n == 0:
        return [], ReanchorReport(len(t_markers), 0, 0.0, 1.0, float("nan"), float("nan"), False)

    t_host = np.array([e.t for e in enters[:n]])
    t_mark = np.array(t_markers[:n])
    deltas = t_mark - t_host

    A = np.vstack([np.ones_like(t_host), t_host]).T
    (alpha, beta), *_ = np.linalg.lstsq(A, deltas, rcond=None)

    corrected_bands = []
    for i, ev in enumerate(enters[:n]):
        t_start = t_mark[i]
        t_end = t_mark[i + 1] if i + 1 < n else t_start
        i_start = int(time_to_sample_index(t_start, fit))
        i_end = int(time_to_sample_index(t_end, fit))
        corrected_bands.append(
            Band(
                state=ev.state,
                t_start=float(t_start),
                t_end=float(t_end),
                i_start=max(0, i_start),
                i_end=max(i_start, i_end),
                guard_s=0.0,  # não precisa de guarda - fronteira exata
                boundary_source="digital",
                boundary_unc_ms=0.01,
            )
        )

    report = ReanchorReport(
        n_markers_found=len(t_markers),
        n_matched=n,
        alpha=float(alpha),
        beta=float(beta),
        delta_p50_ms=float(np.percentile(np.abs(deltas), 50) * 1000),
        delta_p95_ms=float(np.percentile(np.abs(deltas), 95) * 1000),
        ok=True,
    )
    return corrected_bands, report
