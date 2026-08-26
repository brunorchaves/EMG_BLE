"""Análise pós-run: estatísticas por estado, energia por evento, decomposição
de corrente, projeção de autonomia e comparação antes/depois de otimizações.

Tudo lido dos artefatos gravados por run_bench.py (current_raw.u32,
chunks.npy, events.jsonl, emg_packets.npz, run_meta.json) - nenhuma decisão
de decodificação acontece durante a captura, só aqui.
"""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass, field
from pathlib import Path

import numpy as np

import config
import dsp
import emg_validate
from ppk2_decode import CounterReport, DecodedBlock, check_counter, decode_words, digital_bit, load_raw
from timeline import Band, ClockFit, EventLog, build_bands, fit_stream_clock, refine_event_by_changepoint


@dataclass
class StateStats:
    state: str
    n_samples: int
    t_start: float
    t_end: float
    duration_s: float
    mean_uA: float
    rms_uA: float
    median_uA: float
    p05_uA: float
    p95_uA: float
    min_uA: float
    max_uA: float
    std_uA: float
    mean_mW: float
    rms_mW: float
    charge_uC: float
    energy_uJ: float
    baseline_uA: float
    radio_excess_uA: float
    radio_duty: float
    n_range_switches: int
    has_gap: bool
    # Estatisticas EXCLUINDO a vizinhanca das trocas de faixa de medicao.
    # Necessario porque a PPK2 produz artefatos grandes nessas transicoes:
    # medido nesta bancada, 100% das amostras acima de 30 mA estavam a +-3
    # amostras de uma troca de faixa, e o "maximo" bruto chegava a 563 mA -
    # fisicamente impossivel nesta placa a 5 V. A MEDIA quase nao e afetada
    # (6,648 vs 6,707 mA), mas qualquer numero de PICO tem de ser o limpo,
    # senao o relatorio mente por um fator de ~20x.
    mean_clean_uA: float = float("nan")
    p99_9_clean_uA: float = float("nan")
    max_clean_uA: float = float("nan")
    n_samples_clean: int = 0
    flags: list[str] = field(default_factory=list)


@dataclass
class EventEnergy:
    kind: str
    n_reps: int
    t: float
    t_refined: float
    refine_correction_ms: float
    baseline_uA: float
    peak_uA: float
    excess_charge_uC: float
    excess_energy_uJ: float


@dataclass
class Autonomy:
    state_mix: dict[str, float]
    avg_current_mA: float
    avg_power_mW: float
    battery_mAh: float
    battery_v: float
    boost_eta: float
    hours_energy_based: float
    hours_charge_based: float
    assumptions: list[str]


@dataclass
class RunReport:
    run_dir: str
    clock_fit: dict
    counter_report: dict
    state_stats: list[dict]
    emg_validation: dict | None
    gain_response: dict | None
    autonomy: dict | None
    warnings: list[str]


def _range_switch_mask(range_idx: np.ndarray, halfwidth: int = 3) -> np.ndarray:
    """Marca as amostras a +-halfwidth de uma troca de faixa de medicao, onde a
    PPK2 gera artefatos grandes de corrente."""
    switches = np.flatnonzero(np.diff(range_idx.astype(np.int16)) != 0) + 1
    mask = np.zeros(len(range_idx), dtype=bool)
    for off in range(-halfwidth, halfwidth + 1):
        idx = switches + off
        idx = idx[(idx >= 0) & (idx < len(range_idx))]
        mask[idx] = True
    return mask


def per_state_stats(
    uA: np.ndarray,
    bands: list[Band],
    source_v: float,
    fs: float,
    logic: np.ndarray | None = None,
    counter_report: CounterReport | None = None,
    range_idx: np.ndarray | None = None,
) -> list[StateStats]:
    switch_mask = _range_switch_mask(range_idx) if range_idx is not None else None
    out = []
    for b in bands:
        lo, hi = b.guarded_indices(fs)
        lo, hi = max(0, lo), min(len(uA), hi)
        if hi <= lo:
            continue
        seg = uA[lo:hi].astype(np.float64)
        dt = 1.0 / fs

        mean_uA = float(seg.mean())
        rms_uA = float(np.sqrt(np.mean(seg**2)))
        baseline_uA = float(np.percentile(seg, 10))
        mad = float(np.median(np.abs(seg - np.median(seg))))
        radio_excess = mean_uA - baseline_uA
        radio_duty = float(np.mean(seg > baseline_uA + 3 * mad)) if mad > 0 else 0.0

        charge_uC = float(seg.sum() * dt)  # uA * s = uC
        energy_uJ = charge_uC * source_v  # uC * V = uJ

        has_gap = False
        if counter_report is not None and counter_report.n_gaps:
            has_gap = bool(np.any((counter_report.gap_index >= lo) & (counter_report.gap_index < hi)))

        n_range_switches = 0
        mean_clean = p999_clean = max_clean = float("nan")
        n_clean = 0
        if switch_mask is not None:
            band_switch = switch_mask[lo:hi]
            n_range_switches = int(np.count_nonzero(np.diff(range_idx[lo:hi].astype(np.int16)) != 0))
            clean = seg[~band_switch]
            n_clean = len(clean)
            if n_clean:
                mean_clean = float(clean.mean())
                p999_clean = float(np.percentile(clean, 99.9))
                max_clean = float(clean.max())

        flags = []
        if has_gap:
            flags.append("descontinuidade_do_contador_na_banda")
        if b.state == config.State.CONNECTED_IDLE.value:
            flags.append("firmware_bug_suspect: packet_index sem bound-check quando CCCD desabilitado (main.c:743)")

        out.append(
            StateStats(
                state=b.state,
                n_samples=hi - lo,
                t_start=b.t_start,
                t_end=b.t_end,
                duration_s=b.duration_s,
                mean_uA=mean_uA,
                rms_uA=rms_uA,
                median_uA=float(np.median(seg)),
                p05_uA=float(np.percentile(seg, 5)),
                p95_uA=float(np.percentile(seg, 95)),
                min_uA=float(seg.min()),
                max_uA=float(seg.max()),
                std_uA=float(seg.std()),
                mean_mW=mean_uA * source_v / 1000.0,
                rms_mW=rms_uA * source_v / 1000.0,
                charge_uC=charge_uC,
                energy_uJ=energy_uJ,
                baseline_uA=baseline_uA,
                radio_excess_uA=radio_excess,
                radio_duty=radio_duty,
                n_range_switches=n_range_switches,
                has_gap=has_gap,
                mean_clean_uA=mean_clean,
                p99_9_clean_uA=p999_clean,
                max_clean_uA=max_clean,
                n_samples_clean=n_clean,
                flags=flags,
            )
        )
    return out


def event_energies(
    uA: np.ndarray,
    fs: float,
    log: EventLog,
    kinds: list[str],
    fit: ClockFit,
    window_ms: float = 500.0,
) -> list[EventEnergy]:
    from timeline import time_to_sample_index

    out = []
    for kind in kinds:
        matches = [e for e in log.events if e.kind == kind]
        for ev in matches:
            t_refined, conf = refine_event_by_changepoint(uA, fs, ev.t, search_ms=window_ms / 2)
            half_n = int(round((window_ms / 1000.0) * fs / 2))
            center = int(time_to_sample_index(t_refined, fit))
            lo, hi = max(0, center - half_n), min(len(uA), center + half_n)
            if hi <= lo:
                continue
            seg = uA[lo:hi].astype(np.float64)
            baseline = float(np.percentile(seg, 10))
            peak = float(seg.max())
            excess = seg - baseline
            excess_charge_uC = float(np.clip(excess, 0, None).sum() / fs)
            out.append(
                EventEnergy(
                    kind=kind,
                    n_reps=1,
                    t=ev.t,
                    t_refined=t_refined,
                    refine_correction_ms=(t_refined - ev.t) * 1000.0,
                    baseline_uA=baseline,
                    peak_uA=peak,
                    excess_charge_uC=excess_charge_uC,
                    excess_energy_uJ=excess_charge_uC * 5.0,
                )
            )
    return out


def decompose_current(
    uA: np.ndarray, band: Band, fs: float, logic: np.ndarray | None = None, radio_ch: int = 2
) -> tuple[float, float, float]:
    """Decompõe a corrente de uma banda em (baseline, excesso_de_radio,
    duty_de_radio). Sem marcadores de hardware (D2 = evento de rádio via
    PPI/GPIOTE, Fase 2), usa o limiar baseline+3*MAD como heurística. Com
    marcadores, integra exatamente sobre as janelas em que D2 está alto -
    o segundo caso VALIDA o primeiro (ver risco: threshold heuristic vs
    ground truth)."""
    lo, hi = band.guarded_indices(fs)
    seg = uA[lo:hi].astype(np.float64)
    if len(seg) == 0:
        return float("nan"), float("nan"), float("nan")

    if logic is not None:
        bit = digital_bit(logic[lo:hi], radio_ch)
        if bit.any():
            baseline = float(seg[bit == 0].mean()) if np.any(bit == 0) else float(seg.min())
            radio_mean = float(seg[bit == 1].mean()) if np.any(bit == 1) else baseline
            duty = float(bit.mean())
            return baseline, radio_mean - baseline, duty

    baseline = float(np.percentile(seg, 10))
    mad = float(np.median(np.abs(seg - np.median(seg))))
    mask = seg > baseline + 3 * mad if mad > 0 else np.zeros_like(seg, dtype=bool)
    radio_mean = float(seg[mask].mean()) if mask.any() else baseline
    duty = float(mask.mean())
    return baseline, radio_mean - baseline, duty


def project_autonomy(
    stats: list[StateStats],
    mix: dict[str, float],
    battery_mah: float = config.BATTERY_MAH,
    battery_v: float = config.BATTERY_V,
    eta: float = config.BOOST_ETA_DEFAULT,
    source_v: float = 5.0,
) -> Autonomy:
    """Projeta autonomia a partir das estatisticas por estado.

    ``source_v`` e a tensao do trilho medido. Precisa ser parametro: a versao
    anterior tinha 5,0 V hardcoded, o que subestimava o ganho de qualquer run
    feito em outra tensao (num run a 3,3 V a potencia saia 21,9 mW em vez dos
    14,5 mW reais, escondendo justamente o beneficio que se queria medir).
    """
    by_state = {s.state: s for s in stats}
    total_weight = sum(mix.values()) or 1.0
    mix_norm = {k: v / total_weight for k, v in mix.items()}

    avg_uA = sum(mix_norm.get(k, 0.0) * by_state[k].mean_uA for k in mix_norm if k in by_state)
    avg_mA = avg_uA / 1000.0
    avg_mW = avg_mA * source_v

    energy_wh_available = (battery_mah / 1000.0) * battery_v * eta
    hours_energy = energy_wh_available / (avg_mW / 1000.0) if avg_mW > 0 else float("nan")

    charge_available_mah = battery_mah * (battery_v / source_v) * eta
    hours_charge = charge_available_mah / avg_mA if avg_mA > 0 else float("nan")

    return Autonomy(
        state_mix=mix_norm,
        avg_current_mA=avg_mA,
        avg_power_mW=avg_mW,
        battery_mAh=battery_mah,
        battery_v=battery_v,
        boost_eta=eta,
        hours_energy_based=hours_energy,
        hours_charge_based=hours_charge,
        assumptions=[
            f"eficiencia do boost assumida em {eta:.0%}",
            "mistura de estados fornecida pelo chamador, nao medida em uso real",
            "bateria nova, sem degradacao de capacidade",
        ],
    )


def analyze_run(run_dir: Path, spike_filter: bool = False, guard_s: float = 0.25) -> RunReport:
    run_dir = Path(run_dir)
    meta = json.loads((run_dir / "run_meta.json").read_text(encoding="utf-8"))
    calib = meta["ppk2_calibration"]
    source_v = meta["stream_config"]["source_mv"] / 1000.0

    chunks = np.load(run_dir / "chunks.npy")
    fit = fit_stream_clock(chunks, fs_nominal=config.PPK2_FS_NOMINAL)

    words = load_raw(run_dir / "current_raw.u32")
    w = words.astype(np.uint32)
    # passa a faixa de medicao para o check_counter distinguir descarte do
    # instrumento (troca de faixa) de perda real de captura
    counter_report = check_counter(
        ((w >> 18) & 0x3F).astype(np.uint8),
        range_idx=((w >> 14) & 0x07).astype(np.uint8),
    )
    block = decode_words(w, calib, source_v, spike_filter=spike_filter)

    log = EventLog.from_jsonl(run_dir / "events.jsonl")
    seq = [s.value for s in config.State]
    bands = build_bands(log, seq, fit, guard_s=guard_s)

    stats = per_state_stats(
        block.current_uA,
        bands,
        source_v,
        fit.fs_effective,
        block.logic,
        counter_report,
        range_idx=block.range_idx,
    )

    warnings: list[str] = list(counter_report.reasons)

    validation_dict = None
    gain_dict = None
    packets_path = run_dir / "emg_packets.npz"
    if packets_path.exists():
        npz = np.load(packets_path, allow_pickle=True)
        n_pkt = len(npz["t_host"])
        if n_pkt > 0:
            Packet = type("Packet", (), {})
            packets = []
            for i in range(n_pkt):
                p = Packet()
                p.t_os = None if np.isnan(npz["t_os"][i]) else float(npz["t_os"][i])
                p.t_host = float(npz["t_host"][i])
                p.n_bytes = int(npz["n_bytes"][i])
                p.gain_level = int(npz["gain_level"][i])
                p.samples = npz["samples"][i] if npz["samples"].ndim == 2 else np.array([])
                packets.append(p)

            streaming_bands = [b for b in bands if b.state == config.State.STREAMING.value]
            duration_s = sum(b.duration_s for b in streaming_bands) or None

            # taxa de aquisicao REAL medida pelo contador do firmware (não a
            # de entrega) - essencial para a análise espectral não sair com o
            # eixo de frequência escalado. Ver emg_validate.validate_stream.
            fs_acq = None
            fwc_path = run_dir / "fw_counters.json"
            if fwc_path.exists():
                fs_acq = json.loads(fwc_path.read_text(encoding="utf-8")).get("fs_acquisition_sps")

            validation = emg_validate.validate_stream(
                packets,
                uA=block.current_uA,
                fs_ppk2=fit.fs_effective,
                duration_s=duration_s,
                fs_acquisition=fs_acq,
            )
            validation_dict = asdict(validation)
            if validation.verdict != "real":
                warnings.extend(validation.reasons)

            gain_resp = emg_validate.validate_gain_response(packets, meta.get("input_condition", "unknown"))
            gain_dict = asdict(gain_resp)

    autonomy_dict = None
    by_state_names = {s.state for s in stats}
    default_mix = {
        config.State.ADVERTISING.value: 0.94,
        config.State.CONNECTED_IDLE.value: 0.03,
        config.State.STREAMING.value: 0.03,
    }
    if by_state_names.issuperset(default_mix.keys()):
        autonomy = project_autonomy(stats, default_mix, source_v=source_v)
        autonomy_dict = asdict(autonomy)

    report = RunReport(
        run_dir=str(run_dir),
        clock_fit=asdict(fit),
        counter_report={k: v for k, v in vars(counter_report).items() if not isinstance(v, np.ndarray)},
        state_stats=[asdict(s) for s in stats],
        emg_validation=validation_dict,
        gain_response=gain_dict,
        autonomy=autonomy_dict,
        warnings=warnings,
    )

    (run_dir / "report.json").write_text(json.dumps(asdict(report), indent=2, default=str), encoding="utf-8")
    _write_report_md(run_dir, report, meta)

    # versões decimadas para plot rápido
    from ppk2_decode import decimate_minmaxmean

    for factor, name in ((int(fit.fs_effective / 1000) or 1, "current_1k.npy"), (int(fit.fs_effective / 100) or 1, "current_100.npy")):
        mn, mx, mean = decimate_minmaxmean(block.current_uA, max(1, factor))
        np.save(run_dir / name, np.stack([mn, mx, mean], axis=1))

    return report


def _write_report_md(run_dir: Path, report: RunReport, meta: dict) -> None:
    lines = [f"# Relatório - {run_dir.name}", ""]
    lines.append(f"Condição de entrada: `{meta.get('input_condition')}`")
    lines.append(f"J-Link conectado durante o run: `{meta.get('jlink_attached')}`")
    lines.append("")
    lines.append("## Clock fit")
    lines.append(f"- fs_efetivo: {report.clock_fit['fs_effective']:.3f} Hz")
    lines.append(f"- drift: {report.clock_fit['drift_ppm']:.1f} ppm")
    lines.append(f"- latência p50/p95/max: {report.clock_fit['latency_p50_ms']:.2f} / {report.clock_fit['latency_p95_ms']:.2f} / {report.clock_fit['latency_max_ms']:.2f} ms")
    lines.append("")
    lines.append("## Por estado")
    lines.append(
        "Picos (p99.9 / máx) **excluem** a vizinhança das trocas de faixa da PPK2, "
        "onde o instrumento gera artefatos de até ~20× o valor real. A média é "
        "praticamente insensível a isso (<1%)."
    )
    lines.append("")
    lines.append(
        "| Estado | Dur (s) | Média (mA) | RMS (mA) | p95 (mA) | p99.9 limpo (mA) | Máx limpo (mA) | Pot. média (mW) |"
    )
    lines.append("|---|---|---|---|---|---|---|---|")
    for s in report.state_stats:
        lines.append(
            f"| {s['state']} | {s['duration_s']:.1f} | {s['mean_uA']/1000:.3f} | "
            f"{s['rms_uA']/1000:.3f} | {s['p95_uA']/1000:.3f} | "
            f"{s.get('p99_9_clean_uA', float('nan'))/1000:.3f} | "
            f"{s.get('max_clean_uA', float('nan'))/1000:.3f} | {s['mean_mW']:.2f} |"
        )
    if report.emg_validation:
        lines.append("")
        lines.append("## Validação EMG")
        lines.append(f"- Veredito: **{report.emg_validation['verdict']}**")
        for r in report.emg_validation.get("reasons", []):
            lines.append(f"  - {r}")
    if report.warnings:
        lines.append("")
        lines.append("## Avisos")
        for w in report.warnings:
            lines.append(f"- {w}")
    (run_dir / "report.md").write_text("\n".join(lines), encoding="utf-8")


@dataclass
class ComparisonReport:
    deltas: dict[str, dict]
    firmware_diff: dict


def compare_runs(before: Path, after: Path) -> ComparisonReport:
    before_report = json.loads((Path(before) / "report.json").read_text(encoding="utf-8"))
    after_report = json.loads((Path(after) / "report.json").read_text(encoding="utf-8"))
    before_meta = json.loads((Path(before) / "run_meta.json").read_text(encoding="utf-8"))
    after_meta = json.loads((Path(after) / "run_meta.json").read_text(encoding="utf-8"))

    before_by_state = {s["state"]: s for s in before_report["state_stats"]}
    after_by_state = {s["state"]: s for s in after_report["state_stats"]}

    deltas = {}
    for state in set(before_by_state) & set(after_by_state):
        b, a = before_by_state[state], after_by_state[state]
        pct = (a["mean_uA"] - b["mean_uA"]) / b["mean_uA"] * 100 if b["mean_uA"] else float("nan")
        deltas[state] = {
            "before_mA": b["mean_uA"] / 1000,
            "after_mA": a["mean_uA"] / 1000,
            "delta_pct": pct,
        }

    fw_diff = {}
    bf, af = before_meta.get("firmware_config", {}), after_meta.get("firmware_config", {})
    for key in set(bf) | set(af):
        if bf.get(key) != af.get(key):
            fw_diff[key] = {"before": bf.get(key), "after": af.get(key)}

    return ComparisonReport(deltas=deltas, firmware_diff=fw_diff)
