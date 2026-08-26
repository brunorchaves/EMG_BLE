"""Gera o relatorio de consumo em PDF, com o grafico no estilo da Fig. 7 do
artigo (corrente vs tempo com faixas sombreadas por estado) e as analises.

Uso:
    python report_pdf.py <run_5V> [<run_3V3>] [-o saida.pdf]
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages
from matplotlib.patches import Patch

import config
import dsp
from analyze import _range_switch_mask, analyze_run
from ppk2_decode import decode_words, load_raw
from timeline import EventLog, build_bands, fit_stream_clock

# Cores das faixas por estado. As quatro do artigo seguem as cores que ele usa
# (OFF roxo, IDLE azul-claro, CONNECTED laranja, TRANSMITTING rosa); os estados
# novos ganham tons proximos do estado a que mais se parecem.
STATE_COLORS = {
    "OFF":              "#b0a7c6",
    "BOOT":             "#8e6fb0",
    "ADVERTISING":      "#9ecae1",
    "CONNECTING":       "#c9c9c9",
    "CONNECTED_IDLE":   "#f6b26b",
    "STREAMING":        "#f08fa8",
    "CONNECTED_IDLE_2": "#f6b26b",
    "DISCONNECT":       "#c9c9c9",
    "RE_ADVERTISING":   "#9ecae1",
    "OFF_FINAL":        "#b0a7c6",
}

ARTICLE_TABLE2 = {"IDLE": 2.922, "CONNECTED": 8.497, "TRANSMITTING": 9.063}


def load_run(run_dir: Path) -> dict:
    meta = json.loads((run_dir / "run_meta.json").read_text(encoding="utf-8"))
    source_v = meta["stream_config"]["source_mv"] / 1000.0
    w = np.asarray(load_raw(run_dir / "current_raw.u32")).astype(np.uint32)
    blk = decode_words(w, meta["ppk2_calibration"], source_v)
    fit = fit_stream_clock(np.load(run_dir / "chunks.npy"), config.PPK2_FS_NOMINAL)
    log = EventLog.from_jsonl(run_dir / "events.jsonl")
    bands = build_bands(log, [s.value for s in config.State], fit)
    return {
        "dir": run_dir, "meta": meta, "source_v": source_v, "blk": blk,
        "fit": fit, "log": log, "bands": bands,
        "report": analyze_run(run_dir),
        "clean_mask": ~_range_switch_mask(blk.range_idx),
    }


def fig_trace(run: dict, title: str) -> plt.Figure:
    """Fig. 7 do artigo, com muito mais detalhe: traco completo de corrente com
    faixas sombreadas por estado, media anotada por faixa, e sem corte no eixo Y.

    Usa decimacao min/max (estilo osciloscopio) em vez de subamostragem: cada
    pixel horizontal mostra o minimo e o maximo reais daquele intervalo, entao
    nenhum transiente e perdido - ao contrario de pegar 1 amostra a cada N.
    """
    blk, fit, bands = run["blk"], run["fit"], run["bands"]
    uA = blk.current_uA.astype(np.float64)
    # zera artefatos de troca de faixa para nao poluir a envoltoria
    uA = np.where(run["clean_mask"], uA, np.nan)
    fs = fit.fs_effective

    n_px = 2200
    step = max(1, len(uA) // n_px)
    n = len(uA) // step
    m = uA[: n * step].reshape(n, step)
    with np.errstate(all="ignore"):
        lo_env = np.nanmin(m, axis=1) / 1000.0
        hi_env = np.nanmax(m, axis=1) / 1000.0
        mean_env = np.nanmean(m, axis=1) / 1000.0
    t = np.arange(n) * step / fs

    fig, ax = plt.subplots(figsize=(11, 4.4))
    seen = []
    for b in bands:
        t0, t1 = (b.i_start / fs), (b.i_end / fs)
        if t1 <= t0:
            continue
        c = STATE_COLORS.get(b.state, "#dddddd")
        # alpha baixo e zorder abaixo do traco: a faixa identifica o estado sem
        # competir com o sinal, que e o dado
        ax.axvspan(t0, t1, color=c, alpha=0.30, lw=0, zorder=0)
        if b.state not in seen:
            seen.append(b.state)
        st = next((s for s in run["report"].state_stats if s["state"] == b.state), None)
        if st and b.duration_s > 5:
            ax.text((t0 + t1) / 2, ax.get_ylim()[1], "", ha="center")
            ax.annotate(
                f"{st['mean_uA']/1000:.2f} mA",
                xy=((t0 + t1) / 2, hi_env[np.isfinite(hi_env)].max() * 0.97),
                ha="center", va="top", fontsize=7.5, color="#333333",
            )

    ax.fill_between(t, lo_env, hi_env, color="#1f3d5c", alpha=0.42, lw=0, label="min-max", zorder=2)
    ax.plot(t, mean_env, color="#0b1a28", lw=0.8, label="corrente (média)", zorder=3)

    ax.set_xlabel("Tempo (s)")
    ax.set_ylabel("Corrente (mA)")
    ax.set_title(title, fontsize=11, loc="left")
    ax.set_xlim(0, t[-1])
    ax.margins(y=0.06)
    ax.grid(alpha=0.18, lw=0.5)

    handles = [Patch(facecolor=STATE_COLORS.get(s, "#ddd"), alpha=0.55, label=s) for s in seen]
    handles.append(plt.Line2D([], [], color="#12283d", lw=1.2, label="corrente"))
    ax.legend(handles=handles, fontsize=6.6, ncol=5, loc="lower center",
              bbox_to_anchor=(0.5, -0.42), frameon=False)
    fig.tight_layout()
    return fig


def fig_compare(run5: dict, run33: dict | None) -> plt.Figure:
    """Barras por estado: 5,0 V vs 3,3 V vs Tabela 2 do artigo."""
    fig, ax = plt.subplots(figsize=(11, 4.2))
    states = ["BOOT", "ADVERTISING", "CONNECTED_IDLE", "STREAMING", "RE_ADVERTISING"]
    A = {s["state"]: s for s in run5["report"].state_stats}
    B = {s["state"]: s for s in run33["report"].state_stats} if run33 else {}

    x = np.arange(len(states))
    w = 0.36 if run33 else 0.6
    v5 = [A[s]["mean_uA"] / 1000 if s in A else np.nan for s in states]
    ax.bar(x - (w / 2 if run33 else 0), v5, w, label="5,0 V", color="#c47f2a")
    for xi, v in zip(x - (w / 2 if run33 else 0), v5):
        ax.text(xi, v + 0.08, f"{v:.2f}", ha="center", fontsize=7.5)
    if run33:
        v3 = [B[s]["mean_uA"] / 1000 if s in B else np.nan for s in states]
        ax.bar(x + w / 2, v3, w, label="3,3 V", color="#0d6f78")
        for xi, v, v0 in zip(x + w / 2, v3, v5):
            ax.text(xi, v + 0.08, f"{v:.2f}", ha="center", fontsize=7.5)
            ax.text(xi, v / 2, f"{(v-v0)/v0*100:+.0f}%", ha="center", fontsize=7,
                    color="white", fontweight="bold")

    # referencia do artigo, onde ha estado equivalente
    art = {"ADVERTISING": ARTICLE_TABLE2["IDLE"],
           "CONNECTED_IDLE": ARTICLE_TABLE2["CONNECTED"],
           "STREAMING": ARTICLE_TABLE2["TRANSMITTING"]}
    for i, s in enumerate(states):
        if s in art:
            ax.hlines(art[s], i - 0.45, i + 0.45, color="#333333", ls="--", lw=1.3, zorder=5)
            # rotulo ACIMA da linha e com fundo branco: antes ficava a direita e
            # caia sobre a barra vizinha, ilegivel contra o preenchimento
            ax.text(i, art[s], f"artigo {art[s]:.2f}", ha="center", va="bottom",
                    fontsize=7, color="#333333", zorder=6,
                    bbox=dict(boxstyle="round,pad=0.18", fc="white", ec="none", alpha=0.85))

    ax.set_xticks(x)
    ax.set_xticklabels(states, fontsize=8)
    ax.set_ylabel("Corrente média (mA)")
    ax.set_title("Consumo por estado: 5,0 V vs 3,3 V, contra a Tabela 2 do artigo",
                 fontsize=11, loc="left")
    ax.grid(axis="y", alpha=0.18, lw=0.5)
    ax.legend(fontsize=8, frameon=False)
    fig.tight_layout()
    return fig


def fig_boot(run: dict) -> plt.Figure:
    """Zoom no transiente de partida - o dado que dimensiona supercapacitor e
    que o artigo nao tem."""
    blk, fit, bands = run["blk"], run["fit"], run["bands"]
    uA = np.where(run["clean_mask"], blk.current_uA.astype(np.float64), np.nan)
    fs = fit.fs_effective
    boot = next((b for b in bands if b.state == "BOOT"), None)
    fig, ax = plt.subplots(figsize=(11, 3.6))
    if boot is None:
        ax.text(.5, .5, "sem banda BOOT", ha="center"); return fig
    i0 = boot.i_start
    win = int(1.2 * fs)
    seg = uA[max(0, i0 - int(0.05 * fs)) : i0 + win]
    t = (np.arange(len(seg)) / fs - 0.05) * 1000
    ax.plot(t, seg / 1000.0, lw=0.5, color="#8e6fb0")
    ax.axvline(0, color="#333", ls=":", lw=1)
    ax.set_xlabel("Tempo desde a energização (ms)")
    ax.set_ylabel("Corrente (mA)")
    ax.set_title(f"Transiente de partida a {run['source_v']:.1f} V "
                 f"(zoom que a janela de 44 s da Fig. 7 não resolve)", fontsize=11, loc="left")
    ax.grid(alpha=0.18, lw=0.5)
    fig.tight_layout()
    return fig


def fig_emg(run: dict) -> plt.Figure:
    """Validacao do sinal: forma de onda recebida e espectro, com a taxa de
    AQUISICAO (nao a de entrega) no eixo de frequencia."""
    fig, axes = plt.subplots(1, 2, figsize=(11, 3.6))
    p = run["dir"] / "emg_packets.npz"
    fwc = run["dir"] / "fw_counters.json"
    fs_acq = None
    if fwc.exists():
        fs_acq = json.loads(fwc.read_text(encoding="utf-8")).get("fs_acquisition_sps")
    if not p.exists():
        for a in axes: a.axis("off")
        axes[0].text(.5, .5, "sem pacotes EMG", ha="center"); return fig

    npz = np.load(p)
    s = npz["samples"].astype(float).ravel()
    fs = fs_acq or 2000.0
    t = np.arange(min(len(s), int(fs * 1.5))) / fs
    axes[0].plot(t, s[: len(t)], lw=0.5, color="#12283d")
    axes[0].set_xlabel("Tempo (s)"); axes[0].set_ylabel("ADC (int16)")
    axes[0].set_title(f"Sinal recebido via BLE ({run['source_v']:.1f} V)", fontsize=10, loc="left")
    axes[0].grid(alpha=0.18, lw=0.5)

    f, psd = dsp.welch_psd(s - s.mean(), fs, nperseg=1024)
    axes[1].semilogy(f, psd, lw=0.8, color="#0d6f78")
    for h, lbl in ((60, "60 Hz"), (180, "3º harm.")):
        axes[1].axvline(h, color="#c0392b", ls="--", lw=0.8)
        axes[1].text(h, psd.max(), f" {lbl}", fontsize=7, color="#c0392b", va="top")
    axes[1].set_xlabel("Frequência (Hz)"); axes[1].set_ylabel("PSD")
    axes[1].set_title(f"Espectro (fs de aquisição = {fs:.0f} S/s)", fontsize=10, loc="left")
    axes[1].grid(alpha=0.18, lw=0.5)
    fig.tight_layout()
    return fig


def fig_text(title: str, lines: list[str]) -> plt.Figure:
    fig = plt.figure(figsize=(11, 8.5))
    fig.text(0.06, 0.94, title, fontsize=15, fontweight="bold", va="top")
    fig.text(0.06, 0.885, "\n".join(lines), fontsize=8.6, va="top", family="monospace",
             linespacing=1.62)
    return fig


def summary_lines(run5: dict, run33: dict | None) -> list[str]:
    A = {s["state"]: s for s in run5["report"].state_stats}
    B = {s["state"]: s for s in run33["report"].state_stats} if run33 else {}
    au5, au3 = run5["report"].autonomy, (run33["report"].autonomy if run33 else None)
    cr = run5["report"].counter_report
    L = []
    L.append("CONSUMO POR ESTADO")
    L.append("")
    hdr = f"{'estado':<18}{'5,0V mA':>10}{'5,0V mW':>10}"
    if run33: hdr += f"{'3,3V mA':>10}{'3,3V mW':>10}{'delta':>9}"
    L.append(hdr)
    L.append("-" * len(hdr))
    for s in ("OFF", "BOOT", "ADVERTISING", "CONNECTING", "CONNECTED_IDLE",
              "STREAMING", "CONNECTED_IDLE_2", "RE_ADVERTISING", "OFF_FINAL"):
        if s not in A: continue
        r = f"{s:<18}{A[s]['mean_uA']/1000:>10.3f}{A[s]['mean_mW']:>10.2f}"
        if run33 and s in B:
            r += f"{B[s]['mean_uA']/1000:>10.3f}{B[s]['mean_mW']:>10.2f}"
            # Delta so faz sentido acima do piso de ruido: nos estados OFF as
            # duas medidas sao ~1 uA e a razao entre elas e ruido dividido por
            # ruido, que sairia como um "-3,6%" sem nenhum significado.
            if A[s]["mean_uA"] > 50:
                d = (B[s]["mean_uA"] - A[s]["mean_uA"]) / A[s]["mean_uA"] * 100
                r += f"{d:>8.1f}%"
            else:
                r += f"{'--':>9}"
        L.append(r)
    L += ["", "PICOS (excluindo artefato de troca de faixa da PPK2)", ""]
    for s in ("ADVERTISING", "STREAMING"):
        if s in A:
            r = f"{s:<18}p99,9 {A[s]['p99_9_clean_uA']/1000:>7.2f} mA   max {A[s]['max_clean_uA']/1000:>7.2f} mA"
            if run33 and s in B:
                r += f"   | 3,3V: p99,9 {B[s]['p99_9_clean_uA']/1000:.2f}  max {B[s]['max_clean_uA']/1000:.2f} mA"
            L.append(r)
    L += ["", "AUTONOMIA PROJETADA (bateria 400 mAh @ 3,7 V, conversor a 85%)", ""]
    L.append(f"  5,0 V : {au5['avg_current_mA']:.3f} mA   {au5['avg_power_mW']:.2f} mW   {au5['hours_energy_based']:.1f} h")
    if au3:
        L.append(f"  3,3 V : {au3['avg_current_mA']:.3f} mA   {au3['avg_power_mW']:.2f} mW   {au3['hours_energy_based']:.1f} h"
                 f"   ({au3['hours_energy_based']/au5['hours_energy_based']:.2f}x)")
    L += ["", "INTEGRIDADE DA MEDICAO", ""]
    L.append(f"  amostras                : {cr['n_samples']:,}")
    L.append(f"  gaps nao explicados     : {cr['n_gaps_unexplained']}  (captura sem perda: {cr['capture_lossless']})")
    L.append(f"  descarte do instrumento : {cr['n_gaps_at_range_switch']} (todos em troca de faixa)")
    L.append(f"  deriva do relogio       : {run5['report'].clock_fit['drift_ppm']:.1f} ppm")
    v = run5["report"].emg_validation
    if v:
        L += ["", "VALIDACAO DO SINAL", ""]
        L.append(f"  veredito                : {v['verdict']}")
        L.append(f"  pacotes / continuidade  : {v['n_packets']} / {v['frac_continuous_joins']:.3f}")
        L.append(f"  potencia em 50/60 Hz    : {v['line_50_60_frac']*100:.0f}%")
        import textwrap

        for r in v["reasons"]:
            # quebra por palavra em vez de cortar em 96 caracteres, que cortava
            # no meio da palavra ("caminho analogi", "blocos desc")
            wrapped = textwrap.wrap(r, width=94)
            for j, line in enumerate(wrapped):
                L.append(("    - " if j == 0 else "      ") + line)
    return L


def build(run5_dir: Path, run33_dir: Path | None, out_pdf: Path) -> Path:
    run5 = load_run(run5_dir)
    run33 = load_run(run33_dir) if run33_dir else None

    with PdfPages(out_pdf) as pdf:
        cover = [
            "Sensor sEMG vestivel BLE - nRF52840 + ADS112C04",
            "",
            f"Instrumento     : Nordic Power Profiler Kit II (PCA63100), 100 kS/s, source meter",
            f"Trilhos medidos : 5,0 V" + (f" e 3,3 V" if run33 else ""),
            f"Condicao        : entradas abertas (eletrodos nao conectados)",
            f"Aquisicao       : 2042 S/s, 0% de conversoes do ADC perdidas",
            "",
            "",
            "CONTEXTO",
            "",
            "  Esta medicao refaz o estudo de energia do artigo (Tabela 2 e Fig. 7) com a",
            "  PPK2 em vez de shunt + osciloscopio, sobre o firmware corrigido.",
            "",
            "  A correcao mais importante nao foi de consumo: a aquisicao do ADC nunca",
            "  funcionou. O loop principal so era acordado pelo timer do LED de 1 Hz, entao",
            "  o ADC era lido 1 vez por segundo - os 2 kS/s do artigo nao estavam",
            "  acontecendo. Agora sao 2042 S/s com zero conversoes perdidas.",
            "",
            "  Ressalva declarada: as entradas estavam abertas, entao o espectro e dominado",
            "  por rede eletrica (~55%) e a validacao fecha em 'suspect', nao 'real'. Isso",
            "  prova que a cadeia analogica esta viva e com ganho, mas nao permite chamar o",
            "  sinal de EMG real nem medir a banda do filtro.",
        ]
        pdf.savefig(fig_text("Caracterizacao de consumo com PPK2", cover)); plt.close("all")

        pdf.savefig(fig_trace(run5, f"Corrente ao longo do ciclo de operacao - trilho de 5,0 V")); plt.close("all")
        if run33:
            pdf.savefig(fig_trace(run33, "Corrente ao longo do ciclo de operacao - trilho de 3,3 V")); plt.close("all")
        pdf.savefig(fig_compare(run5, run33)); plt.close("all")
        pdf.savefig(fig_boot(run5)); plt.close("all")
        pdf.savefig(fig_emg(run5)); plt.close("all")
        if run33:
            pdf.savefig(fig_emg(run33)); plt.close("all")
        pdf.savefig(fig_text("Resultados", summary_lines(run5, run33))); plt.close("all")

        d = pdf.infodict()
        d["Title"] = "Caracterizacao de consumo - sensor sEMG BLE"
        d["Subject"] = "Medicao com Nordic PPK2 a 5,0 V e 3,3 V"
    return out_pdf


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("run5")
    ap.add_argument("run33", nargs="?")
    ap.add_argument("-o", "--out", default=None)
    a = ap.parse_args()
    out = Path(a.out) if a.out else Path("power_profiling/relatorio_consumo.pdf")
    out.parent.mkdir(parents=True, exist_ok=True)
    p = build(Path(a.run5), Path(a.run33) if a.run33 else None, out)
    print(f"PDF gravado em {p}  ({p.stat().st_size/1024:.0f} kB)")


if __name__ == "__main__":
    main()
