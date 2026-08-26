"""Geração de figuras a partir dos artefatos de um run - a versão "Fig. 7 e
muito mais" pedida: mesma ideia (corrente vs tempo com faixas por estado),
só que com muito mais detalhamento (zooms, distribuições, energia por
evento, validação, autonomia, antes/depois).
"""

from __future__ import annotations

import json
from pathlib import Path

import matplotlib
import matplotlib.pyplot as plt
import numpy as np

matplotlib.use("Agg")

import config
from ppk2_decode import decode_words, load_raw
from timeline import EventLog, build_bands, fit_stream_clock

_STATE_COLORS = {
    config.State.OFF.value: "#4b4b4b",
    config.State.BOOT.value: "#8e44ad",
    config.State.ADVERTISING.value: "#3498db",
    config.State.CONNECTING.value: "#95a5a6",
    config.State.CONNECTED_IDLE.value: "#e67e22",
    config.State.STREAMING.value: "#e74c3c",
    config.State.CONNECTED_IDLE_2.value: "#e67e22",
    config.State.DISCONNECT.value: "#95a5a6",
    config.State.RE_ADVERTISING.value: "#3498db",
    config.State.OFF_FINAL.value: "#4b4b4b",
}


def _load_common(run_dir: Path):
    meta = json.loads((run_dir / "run_meta.json").read_text(encoding="utf-8"))
    report = json.loads((run_dir / "report.json").read_text(encoding="utf-8"))
    chunks = np.load(run_dir / "chunks.npy")
    fit = fit_stream_clock(chunks, fs_nominal=config.PPK2_FS_NOMINAL)
    log = EventLog.from_jsonl(run_dir / "events.jsonl")
    bands = build_bands(log, [s.value for s in config.State], fit)
    return meta, report, fit, log, bands


def fig_a_full_trace(run_dir: Path, out_path: Path) -> None:
    """Fig A: traço completo com faixas por estado - o sucessor direto da
    Fig. 7 do artigo, sem corte no eixo Y e com média/RMS anotados."""
    meta, report, fit, log, bands = _load_common(run_dir)
    mmx = np.load(run_dir / "current_100.npy")  # (min, max, mean) @ ~100 Hz
    t = np.arange(len(mmx)) / 100.0

    fig, ax = plt.subplots(figsize=(14, 5))
    for b in bands:
        i0 = int(b.i_start / (fit.fs_effective / 100))
        i1 = int(b.i_end / (fit.fs_effective / 100))
        ax.axvspan(t[min(i0, len(t) - 1)], t[min(i1, len(t) - 1)], color=_STATE_COLORS.get(b.state, "#ccc"), alpha=0.15)

    ax.fill_between(t, mmx[:, 0] / 1000, mmx[:, 1] / 1000, color="#2c3e50", alpha=0.4, label="min-max")
    ax.plot(t, mmx[:, 2] / 1000, color="#2c3e50", lw=0.8, label="média")
    ax.set_xlabel("Tempo (s)")
    ax.set_ylabel("Corrente (mA)")
    ax.set_title(f"Corrente ao longo do run - {run_dir.name}")
    ax.legend(loc="upper right", fontsize=8)
    fig.tight_layout()
    fig.savefig(out_path, dpi=300)
    plt.close(fig)


def fig_b_zooms(run_dir: Path, out_path: Path, window_ms: float = 500.0) -> None:
    """Fig B: zooms em microssegundos - conexão, advertising, notificação -
    resolução que a Fig. 7 (janela de 44s inteira) não conseguia mostrar."""
    meta, report, fit, log, bands = _load_common(run_dir)
    words = load_raw(run_dir / "current_raw.u32")
    calib = meta["ppk2_calibration"]
    source_v = meta["stream_config"]["source_mv"] / 1000.0
    block = decode_words(words.astype(np.uint32), calib, source_v)

    kinds = ["ble.connected", "ble.cccd_on", "ble.mtu"]
    events = [e for e in log.events if e.kind in kinds]
    n = len(events) or 1
    fig, axes = plt.subplots(1, n, figsize=(5 * n, 4), squeeze=False)
    for ax, ev in zip(axes[0], events or [None]):
        if ev is None:
            ax.axis("off")
            continue
        center = int((ev.t - fit.t_offset) * fit.fs_effective)
        half = int((window_ms / 1000.0) * fit.fs_effective / 2)
        lo, hi = max(0, center - half), min(len(block.current_uA), center + half)
        seg = block.current_uA[lo:hi]
        tt = (np.arange(len(seg)) - half) / fit.fs_effective * 1000
        ax.plot(tt, seg / 1000.0, lw=0.6)
        ax.axvline(0, color="red", ls="--", lw=0.8)
        ax.set_title(ev.kind)
        ax.set_xlabel("ms")
        ax.set_ylabel("mA")
    fig.tight_layout()
    fig.savefig(out_path, dpi=300)
    plt.close(fig)


def fig_c_distributions(run_dir: Path, out_path: Path) -> None:
    """Fig C: distribuição de corrente por estado (histograma) - o RMS único
    da Tabela 2 esconde essa dispersão."""
    _, report, _, _, _ = _load_common(run_dir)
    states = [s for s in report["state_stats"] if s["n_samples"] > 0]
    fig, ax = plt.subplots(figsize=(10, 5))
    for s in states:
        ax.bar(
            s["state"],
            s["mean_uA"] / 1000,
            yerr=s["std_uA"] / 1000,
            color=_STATE_COLORS.get(s["state"], "#888"),
            capsize=4,
        )
    ax.set_ylabel("Corrente média ± desvio (mA)")
    ax.set_title("Corrente por estado")
    plt.setp(ax.get_xticklabels(), rotation=30, ha="right")
    fig.tight_layout()
    fig.savefig(out_path, dpi=300)
    plt.close(fig)


def fig_e_autonomy(run_dir: Path, out_path: Path) -> None:
    """Fig E: projeção de autonomia (bateria 400 mAh e dimensionamento de
    supercapacitor) - o artigo não tem nenhum número de autonomia."""
    _, report, _, _, _ = _load_common(run_dir)
    autonomy = report.get("autonomy")
    fig, ax = plt.subplots(figsize=(6, 4))
    ax.axis("off")
    if not autonomy:
        ax.text(0.5, 0.5, "Autonomia não calculada (estados faltando)", ha="center")
    else:
        text = (
            f"Corrente média projetada: {autonomy['avg_current_mA']:.3f} mA\n"
            f"Potência média: {autonomy['avg_power_mW']:.2f} mW\n\n"
            f"Bateria: {autonomy['battery_mAh']:.0f} mAh @ {autonomy['battery_v']:.1f} V\n"
            f"Eficiência do boost assumida: {autonomy['boost_eta']:.0%}\n\n"
            f"Autonomia (base energia): {autonomy['hours_energy_based']:.1f} h\n"
            f"Autonomia (base carga): {autonomy['hours_charge_based']:.1f} h"
        )
        ax.text(0.05, 0.95, text, va="top", family="monospace", fontsize=11)
    fig.tight_layout()
    fig.savefig(out_path, dpi=300)
    plt.close(fig)


def fig_f_emg_validation(run_dir: Path, out_path: Path) -> None:
    """Fig F: forma de onda EMG recebida + espectro + taxa verificada -
    prova visual de que os dados são reais."""
    packets_path = run_dir / "emg_packets.npz"
    fig, axes = plt.subplots(1, 2, figsize=(12, 4))
    if not packets_path.exists():
        for ax in axes:
            ax.axis("off")
        axes[0].text(0.5, 0.5, "Sem pacotes EMG neste run", ha="center")
        fig.savefig(out_path, dpi=300)
        plt.close(fig)
        return

    npz = np.load(packets_path)
    samples = npz["samples"]
    if samples.ndim != 2 or samples.shape[0] == 0:
        for ax in axes:
            ax.axis("off")
        fig.savefig(out_path, dpi=300)
        plt.close(fig)
        return

    flat = samples.flatten().astype(np.float64)
    axes[0].plot(flat[:2000], lw=0.6)
    axes[0].set_title("Forma de onda EMG recebida (amostra)")
    axes[0].set_xlabel("amostra")
    axes[0].set_ylabel("ADC (int16)")

    import dsp

    fs_est = len(flat) / max(1e-6, (npz["t_host"][-1] - npz["t_host"][0])) if len(npz["t_host"]) > 1 else 1000.0
    freqs, psd = dsp.welch_psd(flat - flat.mean(), fs_est)
    axes[1].semilogy(freqs, psd)
    axes[1].set_title("PSD (Welch)")
    axes[1].set_xlabel("Hz")
    fig.tight_layout()
    fig.savefig(out_path, dpi=300)
    plt.close(fig)


def generate_all(run_dir: Path) -> list[Path]:
    run_dir = Path(run_dir)
    out_dir = run_dir / "figures"
    out_dir.mkdir(exist_ok=True)
    made = []
    for name, fn in (
        ("fig_a_full_trace.png", fig_a_full_trace),
        ("fig_b_zooms.png", fig_b_zooms),
        ("fig_c_distributions.png", fig_c_distributions),
        ("fig_e_autonomy.png", fig_e_autonomy),
        ("fig_f_emg_validation.png", fig_f_emg_validation),
    ):
        path = out_dir / name
        try:
            fn(run_dir, path)
            made.append(path)
        except Exception as e:  # noqa: BLE001
            print(f"aviso: falha gerando {name}: {e}")
    return made


if __name__ == "__main__":
    import sys

    generate_all(Path(sys.argv[1]))
