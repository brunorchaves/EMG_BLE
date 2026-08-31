"""Relatorio de caracterizacao de consumo em PDF.

Inclui o grafico no estilo da Fig. 7 do artigo (corrente vs tempo com faixas
sombreadas por estado) e a analise: adequacao ao protocolo experimental, o
custo do I2C continuo, o efeito de reduzir o ADC para 1 kHz em consumo e em
qualidade de sinal, e a natureza dos picos de corrente.

Uso:
    python report_pdf.py --out relatorio.pdf \
        --run "5V=<dir>" --run "3V3=<dir>" --run "3V3_1kSPS=<dir>"
"""

from __future__ import annotations

import argparse
import json
import textwrap
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages
from matplotlib.patches import Patch

import config
import dsp
import report_style as S
from analyze import _range_switch_mask, analyze_run
from ppk2_decode import decode_words, load_raw
from timeline import EventLog, build_bands, fit_stream_clock


# ----------------------------------------------------------------- carregamento

def load_run(run_dir: Path) -> dict:
    meta = json.loads((run_dir / "run_meta.json").read_text(encoding="utf-8"))
    source_v = meta["stream_config"]["source_mv"] / 1000.0
    w = np.asarray(load_raw(run_dir / "current_raw.u32")).astype(np.uint32)
    blk = decode_words(w, meta["ppk2_calibration"], source_v)
    fit = fit_stream_clock(np.load(run_dir / "chunks.npy"), config.PPK2_FS_NOMINAL)
    log = EventLog.from_jsonl(run_dir / "events.jsonl")
    fs_acq = None
    fwc = run_dir / "fw_counters.json"
    if fwc.exists():
        fs_acq = json.loads(fwc.read_text(encoding="utf-8")).get("fs_acquisition_sps")
    rep = analyze_run(run_dir)
    return {
        "dir": run_dir, "source_v": source_v, "blk": blk, "fit": fit, "log": log,
        "bands": build_bands(log, [s.value for s in config.State], fit),
        "report": rep, "by_state": {s["state"]: s for s in rep.state_stats},
        "clean": ~_range_switch_mask(blk.range_idx), "fs_acq": fs_acq,
    }


def spike_stats(run: dict, state: str = "ADVERTISING", excess_uA: float = 4000.0) -> dict:
    """Taxa, largura e duty dos picos de corrente numa banda."""
    fs = run["fit"].fs_effective
    b = next((x for x in run["bands"] if x.state == state), None)
    if b is None:
        return {}
    lo, hi = b.guarded_indices(fs)
    seg = np.where(run["clean"][lo:hi], run["blk"].current_uA[lo:hi].astype(np.float64), np.nan)
    seg = np.nan_to_num(seg, nan=0.0)
    base = float(np.percentile(seg, 20))
    above = seg > base + excess_uA
    d = np.diff(above.astype(np.int8))
    starts, ends = np.flatnonzero(d == 1) + 1, np.flatnonzero(d == -1) + 1
    n = min(len(starts), len(ends))
    dur = (hi - lo) / fs
    widths_us = (ends[:n] - starts[:n]) / fs * 1e6 if n else np.array([0.0])
    return {
        "base_mA": base / 1000.0, "rate_hz": n / dur if dur else 0.0,
        "width_med_us": float(np.median(widths_us)), "duty_pct": float(above.mean() * 100),
        "peak_mA": float(seg.max() / 1000.0),
    }


# ----------------------------------------------------------------------- figuras

def _text_blocks(fig, blocks: list[tuple[str, str]], y0: float,
                 width: int = 134, size: float = 8.4) -> float:
    """Empilha blocos (subtitulo, paragrafo) numa pagina, avancando y pelo
    numero REAL de linhas apos a quebra. Centralizado numa funcao porque
    calcular esse avanco a olho em cada figura foi o que fez o texto da pagina
    do I2C invadir o rodape."""
    y = y0
    for title, para in blocks:
        fig.text(0.075, y, title, fontsize=10.0, color=S.ACCENT, fontweight="bold")
        y -= 0.024
        wrapped = textwrap.wrap(para, width)
        fig.text(0.075, y, "\n".join(wrapped), fontsize=size, color=S.INK_MID,
                 va="top", linespacing=1.5)
        y -= 0.0205 * len(wrapped) + 0.020
    return y

def fig_trace(run: dict, title: str, subtitle: str) -> plt.Figure:
    """Fig. 7 do artigo, com muito mais detalhe.

    Decimacao min/max (estilo osciloscopio): cada coluna de pixels mostra o
    minimo e o maximo REAIS daquele intervalo, entao nenhum transiente e
    perdido - ao contrario de pegar 1 amostra a cada N.
    """
    fs = run["fit"].fs_effective
    uA = np.where(run["clean"], run["blk"].current_uA.astype(np.float64), np.nan)

    n_px, = (2400,)
    step = max(1, len(uA) // n_px)
    n = len(uA) // step
    m = uA[: n * step].reshape(n, step)
    with np.errstate(all="ignore"):
        lo_e, hi_e, mu_e = np.nanmin(m, 1) / 1e3, np.nanmax(m, 1) / 1e3, np.nanmean(m, 1) / 1e3
    t = np.arange(n) * step / fs

    fig = plt.figure(figsize=(11.7, 8.3))
    S.page_header(fig, "corrente vs tempo", title, subtitle)
    ax = fig.add_axes([0.075, 0.40, 0.865, 0.42])

    top = np.nanmax(hi_e) * 1.10
    seen = []
    for b in run["bands"]:
        t0, t1 = b.i_start / fs, b.i_end / fs
        if t1 <= t0:
            continue
        ax.axvspan(t0, t1, color=S.STATE_COLORS.get(b.state, "#e5e5e5"),
                   alpha=0.42, lw=0, zorder=0)
        if b.state not in seen:
            seen.append(b.state)
        st = run["by_state"].get(b.state)
        if st and b.duration_s > 5:
            ax.annotate(f"{st['mean_uA']/1e3:.2f}", xy=((t0 + t1) / 2, top * 0.965),
                        ha="center", va="top", fontsize=7.8, color=S.INK,
                        fontweight="bold")

    ax.fill_between(t, lo_e, hi_e, color=S.ACCENT, alpha=0.30, lw=0, zorder=2)
    ax.plot(t, mu_e, color="#0a3b40", lw=0.85, zorder=3)
    ax.axhline(S.TARGET_MEAN_MA, color=S.ALERT, ls=(0, (5, 3)), lw=1.1, zorder=4)
    ax.text(t[-1], S.TARGET_MEAN_MA, f" meta {S.TARGET_MEAN_MA:.0f} mA", color=S.ALERT,
            fontsize=7.6, va="bottom", ha="right")

    ax.set_xlabel("Tempo (s)"); ax.set_ylabel("Corrente (mA)")
    ax.set_xlim(0, t[-1]); ax.set_ylim(-top * 0.03, top)

    h = [Patch(facecolor=S.STATE_COLORS.get(s, "#e5e5e5"), alpha=0.42, label=s) for s in seen]
    h += [Patch(facecolor=S.ACCENT, alpha=0.30, label="envelope min–max"),
          plt.Line2D([], [], color="#0a3b40", lw=1.2, label="corrente média")]
    ax.legend(handles=h, ncol=4, fontsize=7.2, loc="upper center",
              bbox_to_anchor=(0.5, -0.20))

    note = (
        "Envelope min–max por coluna de pixel, não subamostragem: nenhum transiente é perdido.\n"
        "Artefatos de troca de faixa da PPK2 tratados pelo spike filter do fabricante, que\n"
        "SUBSTITUI as 3 amostras seguintes a cada troca por uma média móvel causal em vez de\n"
        "descartá-las — descartar removia 14 a 27% do registro e enviesava a média em −27%."
    )
    fig.text(0.075, 0.15, note, fontsize=8, color=S.INK_SOFT, va="top", linespacing=1.6)
    S.page_footer(fig, run["dir"].name, f"{run['source_v']:.1f} V · {run['fs_acq'] or 0:.0f} S/s")
    return fig


def fig_progression(runs: dict) -> plt.Figure:
    """Progressao do consumo ao longo das otimizacoes, com a meta marcada."""
    fig = plt.figure(figsize=(11.7, 8.3))
    S.page_header(fig, "progressão", "De 8,15 mA para 1,60 mA",
                  "Cada barra é uma medição completa, não uma estimativa.")
    ax = fig.add_axes([0.10, 0.44, 0.84, 0.40])

    # Da segunda barra em diante os valores vem do spike filter da Nordic. A
    # primeira e de uma medicao anterior a essa correcao e por isso fica ~3,5%
    # alta: a comparacao segue valida (o vies e muito menor que os passos), mas
    # nao e homogenea.
    steps = [
        ("Original\n5 V, LED aceso\naquisição quebrada", 8.148, 40.74, S.BEFORE),
        ("LED e UART off\n5 V", 6.575, 32.87, S.BEFORE),
        ("Trilho 3,3 V", 3.940, 13.00, S.ACCENT_LT),
        ("3,3 V + ADC 1 kSPS", 2.531, 8.35, S.ACCENT_LT),
        ("+ power-down do ADC\nquando desconectado", 1.601, 5.28, S.ACCENT),
    ]
    x = np.arange(len(steps))
    vals = [s[1] for s in steps]
    ax.bar(x, vals, 0.56, color=[s[3] for s in steps], zorder=3)
    for xi, (lbl, mA, mW, _c) in zip(x, steps):
        ax.text(xi, mA + 0.16, f"{mA:.2f} mA", ha="center", fontsize=9.5,
                fontweight="bold", color=S.INK)
        ax.text(xi, mA / 2, f"{mW:.1f} mW", ha="center", fontsize=8.4, color="white",
                fontweight="bold")
    for xi in range(1, len(steps)):
        d = (vals[xi] - vals[xi - 1]) / vals[xi - 1] * 100
        ax.annotate(f"{d:+.0f}%", xy=(xi - 0.5, max(vals) * 0.92), ha="center",
                    fontsize=8.6, color=S.INK_MID)

    ax.axhline(S.TARGET_MEAN_MA, color=S.ALERT, ls=(0, (5, 3)), lw=1.2, zorder=4)
    ax.text(len(steps) - 0.4, S.TARGET_MEAN_MA, f" meta {S.TARGET_MEAN_MA:.0f} mA",
            color=S.ALERT, fontsize=8.4, va="bottom", ha="right")

    ax.set_xticks(x); ax.set_xticklabels([s[0] for s in steps], fontsize=8.2)
    ax.set_ylabel("Corrente média em ADVERTISING (mA)")
    ax.set_ylim(0, max(vals) * 1.15)

    txt = (
        f"A queda total é de {(1 - steps[-1][1] / steps[0][1]) * 100:.0f}% na corrente e "
        f"{(1 - steps[-1][2] / steps[0][2]) * 100:.0f}% na potência. O passo do ADC a 1 kSPS\n"
        "tem custo de qualidade de sinal, descrito na página seguinte — não é ganho gratuito.\n"
        "O power-down do ADC não tem: ele só age quando não há ninguém consumindo as amostras."
    )
    fig.text(0.10, 0.30, txt, fontsize=8.6, color=S.INK_MID, va="top", linespacing=1.7)
    S.page_footer(fig, "medições a 3,3 V e 5,0 V", "PPK2 · 100 kS/s")
    return fig


def fig_i2c(runs: dict) -> plt.Figure:
    """O custo do I2C continuo e a natureza dos picos."""
    r2k = runs.get("3V3"); r1k = runs.get("3V3_1kSPS")
    fig = plt.figure(figsize=(11.7, 8.3))
    S.page_header(fig, "picos de corrente",
                  "Os picos são o I²C do ADC, não o rádio",
                  "Um pico por conversão — a taxa acompanha o ADC, a amplitude não muda.")

    ax = fig.add_axes([0.075, 0.505, 0.40, 0.265])
    ax2 = fig.add_axes([0.565, 0.505, 0.375, 0.265])

    # zoom de 6 ms mostrando os pulsos individuais
    if r2k:
        fs = r2k["fit"].fs_effective
        b = next(x for x in r2k["bands"] if x.state == "ADVERTISING")
        lo, _ = b.guarded_indices(fs)
        lo += int(2 * fs)
        seg = np.where(r2k["clean"][lo:lo + int(0.006 * fs)],
                       r2k["blk"].current_uA[lo:lo + int(0.006 * fs)].astype(float), np.nan)
        t = np.arange(len(seg)) / fs * 1000
        ax.plot(t, seg / 1e3, lw=0.9, color=S.ACCENT)
        ax.set_xlabel("Tempo (ms)"); ax.set_ylabel("Corrente (mA)")
        ax.set_title("Zoom de 6 ms · 2 kSPS", fontsize=10)

    # taxa de picos vs taxa do ADC
    labels, rates, peaks, cols = [], [], [], []
    for key, lbl, col in (("3V3", "2 kSPS", S.ACCENT_LT), ("3V3_1kSPS", "1 kSPS", S.ACCENT)):
        if key in runs:
            sp = spike_stats(runs[key])
            labels.append(lbl); rates.append(sp["rate_hz"]); peaks.append(sp["peak_mA"]); cols.append(col)
    if labels:
        xx = np.arange(len(labels))
        ax2.bar(xx - 0.19, rates, 0.36, color=cols, zorder=3, label="picos/s")
        ax2b = ax2.twinx(); ax2b.grid(False)
        ax2b.bar(xx + 0.19, peaks, 0.36, color=S.BEFORE, zorder=3, label="pico (mA)")
        for xi, v in zip(xx - 0.19, rates):
            ax2.text(xi, v + 30, f"{v:.0f}/s", ha="center", fontsize=8.2, color=S.INK)
        for xi, v in zip(xx + 0.19, peaks):
            ax2b.text(xi, v + 0.5, f"{v:.1f}", ha="center", fontsize=8.2, color=S.INK)
        ax2.set_xticks(xx); ax2.set_xticklabels(labels)
        ax2.set_ylabel("Picos por segundo"); ax2b.set_ylabel("Amplitude do pico (mA)")
        ax2.set_ylim(0, max(rates) * 1.3); ax2b.set_ylim(0, max(peaks) * 1.35)
        ax2.set_title("Taxa acompanha o ADC; amplitude não", fontsize=10)

    body = [
        ("Por que o I²C é o gargalo",
         "O ADS112C04 está em conversão contínua. Cada conversão gera uma borda de DRDY# que acorda a "
         "CPU para uma leitura I²C bloqueante: escreve o comando RDATA, lê 2 bytes. A 400 kHz isso são "
         "~112 µs por amostra, e a 2042 S/s ocupa ~23% do tempo de CPU em espera ativa. A 100 kHz "
         "(configuração original) eram ~450 µs, ou seja ~92% — e por isso 11% das conversões nunca "
         "eram lidas."),
        ("O que os picos realmente são",
         "Medido: 1950 picos/s a 2 kSPS e 1004 picos/s a 1 kSPS — um por conversão, largura mediana de "
         "20 µs. A amplitude não muda (20,0 → 19,8 mA). Não é o rádio: entre ADVERTISING e STREAMING a "
         "diferença de média é de apenas 0,25 mA a 1 kSPS (0,46 mA a 2 kSPS), com o link entregando 17 e 32 pacotes/s respectivamente."),
        ("Como resolver",
         "Cada pico move ~0,4 µC. Para manter o trilho dentro de 10 mV basta C = Q/ΔV ≈ 44 µF de "
         "capacitância local — um bulk de 47–100 µF junto à carga absorve os pulsos, e a fonte "
         "(supercapacitor ou PMU) só vê a média. A solução estrutural é usar TWIM com EasyDMA "
         "disparado por PPI a partir do DRDY#: a transferência acontece sem CPU, que só acorda a cada "
         "N amostras. Hoje o projeto usa o driver legado nrfx_twi, sem DMA, com patch manual no SDK."),
    ]
    _text_blocks(fig, body, y0=0.415)
    S.page_footer(fig, "picos medidos em ADVERTISING, 3,3 V", "limiar: base + 4 mA")
    return fig


def fig_1khz(runs: dict) -> plt.Figure:
    """1 kSPS: o que se ganha em consumo e o que se perde em sinal."""
    r2k, r1k = runs.get("3V3"), runs.get("3V3_1kSPS")
    fig = plt.figure(figsize=(11.7, 8.3))
    S.page_header(fig, "adc a 1 kSPS",
                  "1 kSPS corta 37% do consumo, e a banda foi restaurada",
                  "Coeficientes recalculados para fs=1000; o filtro analógico fica como está.")

    ax = fig.add_axes([0.075, 0.525, 0.38, 0.255])
    ax2 = fig.add_axes([0.555, 0.525, 0.385, 0.255])

    if r2k and r1k:
        sts = ["ADVERTISING", "CONNECTED_IDLE", "STREAMING"]
        xx = np.arange(len(sts))
        v2 = [r2k["by_state"][s]["mean_uA"] / 1e3 for s in sts]
        v1 = [r1k["by_state"][s]["mean_uA"] / 1e3 for s in sts]
        ax.bar(xx - 0.19, v2, 0.36, color=S.ACCENT_LT, label="2 kSPS", zorder=3)
        ax.bar(xx + 0.19, v1, 0.36, color=S.ACCENT, label="1 kSPS", zorder=3)
        for xi, a, b_ in zip(xx, v2, v1):
            ax.text(xi - 0.19, a + 0.08, f"{a:.2f}", ha="center", fontsize=8)
            ax.text(xi + 0.19, b_ + 0.08, f"{b_:.2f}", ha="center", fontsize=8)
            ax.text(xi + 0.19, b_ / 2, f"{(b_-a)/a*100:+.0f}%", ha="center", fontsize=7.6,
                    color="white", fontweight="bold")
        ax.axhline(S.TARGET_MEAN_MA, color=S.ALERT, ls=(0, (5, 3)), lw=1.1)
        ax.set_xticks(xx); ax.set_xticklabels([s.replace("_", "\n") for s in sts], fontsize=7.8)
        ax.set_ylabel("Corrente média (mA)")
        ax.legend(fontsize=7.6, ncol=2, loc="upper left", bbox_to_anchor=(0, -0.13))
        ax.set_title("Consumo a 3,3 V", fontsize=10)

        # espectros sobrepostos
        for run, lbl, col in ((r2k, "2 kSPS", S.ACCENT_LT), (r1k, "1 kSPS", S.ACCENT)):
            p = run["dir"] / "emg_packets.npz"
            if not p.exists():
                continue
            s = np.load(p)["samples"].astype(float).ravel()
            fs = run["fs_acq"] or 2000.0
            f, psd = dsp.welch_psd(s - s.mean(), fs, nperseg=1024)
            ax2.semilogy(f, psd / psd.max(), lw=1.0, color=col, label=lbl)
        for hz, lbl in ((60, "60 Hz"), (180, "180 Hz")):
            ax2.axvline(hz, color=S.ALERT, ls=":", lw=0.9)
            ax2.text(hz + 4, 0.5, lbl, fontsize=7.4, color=S.ALERT, rotation=90, va="center")
        ax2.set_xlim(0, 450); ax2.set_ylim(1e-8, 3)
        ax2.set_xlabel("Frequência (Hz)"); ax2.set_ylabel("PSD normalizada")
        ax2.legend(fontsize=7.8)
        ax2.set_title("Espectro com os coeficientes de 2 kHz a 1 kSPS", fontsize=10)

    body = [
        ("O que se ganha",
         "Medido a 3,3 V: ADVERTISING 3,92 → 2,52 mA (−35,7%), STREAMING 4,43 → 2,77 mA (−37,5%). "
         "Potência média 13,00 → 8,35 mW; autonomia projetada 97 → 151 h. Vem de três fontes: o ADC "
         "sai do modo turbo (que dobra o clock interno do modulador), as transações I²C caem pela "
         "metade, e o rádio transmite metade dos blocos."),
        ("O que se perdia, e já foi corrigido",
         "Os coeficientes do Butterworth dependem da taxa, e só existia o conjunto de fs = 2000 Hz: a "
         "1000 SPS a banda de 20–400 Hz virava 10–200 Hz em silêncio (energia em 200–400 Hz caindo de "
         "2,1% para 0,2%). Os de 1 kHz foram gerados com mkfilter e verificados: −3 dB em exatamente "
         "20,0 e 400,0 Hz, energia de volta a 2,4%. Os dois conjuntos são escolhidos pelo mesmo flag "
         "que define a taxa, e não podem dessincronizar."),
        ("Correção de uma análise anterior: o anti-aliasing não é problema",
         "Uma versão anterior deste relatório tratava o Sallen-Key de 2ª ordem em 482 Hz como "
         "anti-aliasing insuficiente. Está errado: o ADS112C04 é delta-sigma e seu filtro digital "
         "interno atenua de fDR/2 até fMOD, então o analógico só precisa bloquear perto de fMOD, onde "
         "há ~109 dB de folga. Mantê-lo como está é correto nas duas taxas."),
        ("A decisão que sobra",
         "A 1 kSPS a banda de 30–400 Hz é preservada; muda só a taxa, de 2 para 1 kS/s — Nyquist "
         "de 500 Hz para banda de 400 Hz, margem de 25%: apertada mas legítima. O default do "
         "firmware passou a ser 1 kSPS (ADS_TURBO_MODE 0)."),
    ]
    _text_blocks(fig, body, y0=0.442, size=8.2)
    S.page_footer(fig, "ADS_TURBO_MODE em ADS112C04.h", "coeficientes gerados com mkfilter")
    return fig


def fig_voltage(sweep_csv: Path | None) -> plt.Figure:
    """Varredura de tensao de alimentacao e tensao minima de operacao."""
    fig = plt.figure(figsize=(11.7, 8.3))
    S.page_header(fig, "trilho de alimentação",
                  "3,3 V corta 58% da potência; o piso útil é 2,7 V",
                  "Placa real medida em cada tensão, com o firmware confirmado vivo em todas.")
    ax = fig.add_axes([0.075, 0.50, 0.40, 0.33])
    ax2 = ax.twinx(); ax2.grid(False)

    mv = np.array([5000, 4700, 4300, 4000, 3600, 3300, 3000, 2700, 2400, 2200, 2000])
    mA = np.array([6.791, 6.510, 5.388, 5.253, 5.111, 4.371, 4.041, np.nan, np.nan, np.nan, np.nan])
    # segunda serie, medida com o ADC a 1 kSPS
    mA1k = np.array([np.nan]*5 + [2.758, 2.479, 2.388, 2.543, 2.515, 2.463])
    mW = mA * mv / 1000.0
    mW1k = mA1k * mv / 1000.0

    ax.plot(mv / 1000, mA, "o-", color=S.ACCENT_LT, lw=1.6, ms=4.5, label="corrente · 2 kSPS")
    ax.plot(mv / 1000, mA1k, "s-", color=S.ACCENT, lw=1.6, ms=4.5, label="corrente · 1 kSPS")
    ax2.plot(mv / 1000, mW, "o--", color=S.BEFORE, lw=1.2, ms=3.5, alpha=0.85, label="potência · 2 kSPS")
    ax2.plot(mv / 1000, mW1k, "s--", color="#8a5a12", lw=1.2, ms=3.5, alpha=0.85, label="potência · 1 kSPS")

    ax.axvspan(2.0, 2.7, color=S.ALERT, alpha=0.10, lw=0)
    ax.axvline(2.7, color=S.ALERT, ls="-", lw=1.3)
    ax.text(2.72, ax.get_ylim()[1] * 0.96, "2,7 V\nlimite do DS3502", fontsize=7.6,
            color=S.ALERT, va="top")
    ax.set_xlabel("Tensão de alimentação (V)"); ax.set_ylabel("Corrente média (mA)")
    ax2.set_ylabel("Potência (mW)")
    h1, l1 = ax.get_legend_handles_labels(); h2, l2 = ax2.get_legend_handles_labels()
    ax.legend(h1 + h2, l1 + l2, fontsize=7.4, loc="lower right")
    ax.set_title("Corrente e potência vs tensão", fontsize=10)

    rows = [
        ("Tensão", "Corrente", "Potência", "Situação"),
        ("5,0 V", "6,79 mA", "33,96 mW", "referência do artigo"),
        ("3,6 V", "5,11 mA", "18,40 mW", "tudo em especificação"),
        ("3,3 V", "4,37 mA", "14,41 mW", "recomendado"),
        ("3,0 V", "2,48 mA", "7,44 mW", "regulador do módulo próximo do dropout"),
        ("2,7 V", "2,39 mA", "6,45 mW", "piso: limite inferior do DS3502"),
        ("2,4 V", "2,54 mA", "6,10 mW", "MCP609 e DS3502 fora de spec"),
        ("2,0 V", "2,46 mA", "4,93 mW", "firmware roda, mas fora de spec"),
    ]
    tb = fig.add_axes([0.545, 0.50, 0.395, 0.33]); tb.axis("off")
    tbl = tb.table(cellText=[r for r in rows[1:]], colLabels=rows[0],
                   cellLoc="left", loc="upper left",
                   colWidths=[0.15, 0.18, 0.18, 0.49])
    tbl.auto_set_font_size(False); tbl.set_fontsize(7.6); tbl.scale(1, 1.42)
    for (r, c), cell in tbl.get_celld().items():
        cell.set_edgecolor(S.LINE); cell.set_linewidth(0.5)
        if r == 0:
            cell.set_facecolor("#eef2f3"); cell.set_text_props(color=S.INK, fontweight="bold")
        elif rows[r][0] == "3,3 V":
            cell.set_facecolor("#e2f0f1")
        elif r >= 6:
            cell.set_text_props(color=S.INK_SOFT)

    # Esta varredura e de uma medicao anterior a duas mudancas: o spike filter
    # (que baixa as medias ~3,5%) e o power-down do ADC (que baixa o
    # ADVERTISING ~39%). Os numeros seguem validos para a pergunta desta
    # pagina - onde o ganho de tensao satura -, mas nao sao comparaveis com a
    # pagina de progressao. Dizer isso e mais honesto que renumerar sem
    # remedir.
    tb.text(0.0, -0.10,
            "Varredura medida sem o spike filter (~3,5% alta) e com o firmware\n"
            "anterior ao power-down do ADC. Serve para localizar onde o ganho de\n"
            "tensão satura, não para comparar com a página de progressão.",
            transform=tb.transAxes, fontsize=7.0, color=S.INK_SOFT,
            va="top", linespacing=1.5)

    body = (
        "A corrente para de cair abaixo de ~3,0 V e chega a subir: é o regulador do módulo entrando em "
        "dropout, com o trilho do nRF já não regulado. Ou seja, o ganho de potência satura em torno de "
        "2,7–3,0 V e não há motivo para ir mais fundo.\n\n"
        "Para o supercapacitor isso define a janela útil de descarga. Com máximo em 3,3 V e mínimo em "
        "2,7 V, a energia extraível é E = ½·C·(3,3² − 2,7²) = ½·C·3,60 — atenção: o protocolo "
        "experimental usa E = C·V² e E_disp = C·(V²máx − V²mín), sem o fator ½. As fórmulas corretas "
        "são E = ½CV² e E_disp = ½C(V²máx − V²mín); do jeito escrito a energia disponível fica "
        "superestimada em exatamente 2×, o que dobraria a autonomia prevista."
    )
    fig.text(0.075, 0.40, "\n".join(textwrap.wrap(body, 128).__iter__()) if False else body,
             fontsize=8.7, color=S.INK_MID, va="top", linespacing=1.62, wrap=True)
    S.page_footer(fig, "voltage_sweep.py", "firmware confirmado vivo pelos contadores em cada ponto")
    return fig


def fig_protocol(runs: dict) -> plt.Figure:
    """Adequacao das medicoes ao protocolo experimental do Robert."""
    fig = plt.figure(figsize=(11.7, 8.3))
    S.page_header(fig, "protocolo experimental",
                  "Adequação ao Ensaio 1 do protocolo",
                  "O que já está coberto, o que falta, e o que precisa de correção.")

    rows = [
        ("Requisito do protocolo", "Situação", "Observação"),
        ("Estado Standby", "coberto",
         "ADVERTISING com o ADC em power-down: 1,54 mA, validado por contador"),
        ("Estado Medição (sem BLE)", "faltando",
         "Exige um build com o rádio desligado; não medido isoladamente"),
        ("Estado Transmissão (sem aquisição)", "faltando",
         "Sem aquisição não há o que transmitir; o power-down cobre o inverso"),
        ("Estado Medição + Bluetooth", "coberto", "= nosso STREAMING"),
        ("Corrente média / mín / máx", "coberto",
         "Artefato de faixa tratado pelo spike filter; pico pelo p99,9"),
        ("Tensão de alimentação", "coberto", "5,0 V, 3,3 V e varredura de 5,0 a 2,0 V"),
        ("Frequência de aquisição", "coberto",
         "1015 S/s a 1 kSPS, 2042 S/s em turbo, por contador no firmware"),
        ("Resolução do ADC", "coberto", "16 bits"),
        ("Taxa de transmissão / pacotes", "coberto",
         "17 pacotes/s de 120 B a 1 kSPS (32/s em turbo), sem blocos perdidos"),
        ("Energia consumida", "coberto", "Carga e energia por estado, em µC e µJ"),
        ("Tensão mínima de operação", "coberto", "2,7 V em spec; roda até 2,0 V fora de spec"),
        ("Autonomia estimada", "coberto", "Bateria; falta a curva de descarga do supercapacitor"),
        ("Comportamento da regulação", "faltando", "Depende da placa BQ25570, ainda não integrada"),
        ("Comparação com clínico", "fora de escopo", "Ensaio 5, não é medição de consumo"),
    ]
    ax = fig.add_axes([0.062, 0.30, 0.876, 0.53]); ax.axis("off")
    tbl = ax.table(cellText=[r for r in rows[1:]], colLabels=rows[0], cellLoc="left",
                   loc="upper left", colWidths=[0.27, 0.12, 0.61])
    tbl.auto_set_font_size(False); tbl.set_fontsize(8.0); tbl.scale(1, 1.55)
    status_col = {"coberto": S.GOOD, "parcial": S.BEFORE, "faltando": S.ALERT,
                  "fora de escopo": S.INK_SOFT}
    for (r, c), cell in tbl.get_celld().items():
        cell.set_edgecolor(S.LINE); cell.set_linewidth(0.5)
        if r == 0:
            cell.set_facecolor("#eef2f3"); cell.set_text_props(color=S.INK, fontweight="bold")
        else:
            st = rows[r][1]
            if c == 1:
                cell.set_text_props(color=status_col.get(st, S.INK), fontweight="bold")

    note = (
        "A lacuna estrutural que restou é a de isolar o rádio: o firmware não tem um build sem BLE, "
        "então não há como medir o ADC sem o rádio ligado. O protocolo pede essa separação para "
        "atribuir consumo por subsistema (Nível 1 — Subsistemas). "
        "Para o ADC essa lacuna FOI FECHADA: com o power-down implementado, ADVERTISING (ADC dormindo) e "
        "CONNECTED (ADC convertendo) diferem apenas pela aquisição — 1,536 contra 2,487 mA a 3,3 V e "
        "1 kSPS, ou seja 0,95 mA de aquisição, medido e não estimado. Antes havia só a aproximação pelo "
        "transiente de partida (2,69 mA antes de configurar o ADC contra 6,81 mA em regime, a 5 V, "
        "atribuindo ~4,1 mA). Falta o build sem rádio para isolar o outro subsistema."
    )
    fig.text(0.062, 0.255, "\n".join(textwrap.wrap(note, 132)), fontsize=8.7,
             color=S.INK_MID, va="top", linespacing=1.66)
    S.page_footer(fig, "Protocolo_Experimental_sEMG_Supercapacitor.pdf", "Ensaio 1 e Ensaio 4")
    return fig


def fig_results(runs: dict) -> plt.Figure:
    fig = plt.figure(figsize=(11.7, 8.3))
    S.page_header(fig, "resultados", "Tabela consolidada",
                  "Quatro configurações medidas, mesma sequência de nove estados.")
    keys = [k for k in ("5V", "3V3", "3V3_1kSPS", "3V3_1kSPS_pd") if k in runs]
    names = {"5V": "5,0 V · 2 kSPS", "3V3": "3,3 V · 2 kSPS",
             "3V3_1kSPS": "3,3 V · 1 kSPS", "3V3_1kSPS_pd": "3,3 V · 1 kSPS\n+ power-down"}
    states = ["OFF", "BOOT", "ADVERTISING", "CONNECTING", "CONNECTED_IDLE",
              "STREAMING", "CONNECTED_IDLE_2", "RE_ADVERTISING", "OFF_FINAL"]

    header = ["Estado"] + [f"{names[k]}\nmA / mW" for k in keys]
    body = []
    for st in states:
        row = [st]
        for k in keys:
            s = runs[k]["by_state"].get(st)
            row.append(f"{s['mean_uA']/1e3:.3f} / {s['mean_mW']:.2f}" if s else "—")
        body.append(row)

    ax = fig.add_axes([0.062, 0.50, 0.876, 0.33]); ax.axis("off")
    tbl = ax.table(cellText=body, colLabels=header, cellLoc="right", loc="upper left",
                   colWidths=[0.22] + [0.195] * len(keys))
    tbl.auto_set_font_size(False); tbl.set_fontsize(8.2); tbl.scale(1, 1.5)
    for (r, c), cell in tbl.get_celld().items():
        cell.set_edgecolor(S.LINE); cell.set_linewidth(0.5)
        if r == 0:
            cell.set_facecolor("#eef2f3"); cell.set_text_props(color=S.INK, fontweight="bold")
        if c == 0 and r > 0:
            cell.set_text_props(ha="left")
        if r > 0 and body[r - 1][0] == "STREAMING":
            cell.set_facecolor("#e2f0f1")
    # o rotulo da config com power-down tem tres linhas e nao caberia na altura
    # padrao do cabecalho, que e dimensionada para uma so
    for c in range(len(header)):
        h0 = tbl[(0, c)].get_height()
        tbl[(0, c)].set_height(h0 * 1.9)

    lines = []
    for k in keys:
        au = runs[k]["report"].autonomy
        cr = runs[k]["report"].counter_report
        v = runs[k]["report"].emg_validation or {}
        lines.append(f"{names[k]}")
        lines.append(f"   autonomia projetada   {au['hours_energy_based']:.1f} h "
                     f"({au['avg_current_mA']:.3f} mA · {au['avg_power_mW']:.2f} mW)")
        lines.append(f"   captura sem perda     {cr['capture_lossless']}  "
                     f"({cr['n_gaps_unexplained']} gaps não explicados)")
        lines.append(f"   pacotes / continuidade {v.get('n_packets','—')} / "
                     f"{v.get('frac_continuous_joins', float('nan')):.3f}")
        lines.append("")
    # em duas colunas: com quatro configuracoes uma coluna unica nao cabe
    half = (len(lines) + 1) // 2
    # nao cortar no meio de um bloco de 5 linhas (titulo + 3 dados + branco)
    while half < len(lines) and lines[half - 1] != "":
        half += 1
    for x, chunk in ((0.062, lines[:half]), (0.52, lines[half:])):
        fig.text(x, 0.455, "\n".join(chunk), fontsize=7.8, color=S.INK_MID,
                 va="top", family="DejaVu Sans", linespacing=1.66)

    caveat = (
        "Ressalvas declaradas. As entradas estavam abertas, então o espectro é dominado por rede "
        "elétrica (~55%) e a validação fecha em 'suspect', não 'real': isso prova que a cadeia "
        "analógica está viva e com ganho, mas não permite chamar o sinal de EMG real nem medir a banda "
        "do filtro. O cabo SWD ficou conectado durante os runs — sem sessão de debug ativa na janela "
        "medida, mas o delta do cabo em si não foi quantificado. Os artefatos de troca de faixa da "
        "PPK2 são tratados pelo spike filter do fabricante; ainda assim sobram ~0,001% de amostras "
        "acima de 25 mA, todas na mesma faixa, então o pico representativo é o p99,9 e não o máximo."
    )
    fig.text(0.062, 0.235, "\n".join(textwrap.wrap(caveat, 132)), fontsize=8.5,
             color=S.INK_MID, va="top", linespacing=1.66)
    S.page_footer(fig, "power_profiling/runs/", "PPK2 · source meter · 100 kS/s")
    return fig


def fig_cover(runs: dict) -> plt.Figure:
    fig = plt.figure(figsize=(11.7, 8.3))
    fig.text(0.062, 0.88, "CARACTERIZAÇÃO DE CONSUMO", fontsize=8.4, color=S.ACCENT,
             fontweight="bold")
    fig.text(0.062, 0.80, "Sensor sEMG vestível\nalimentado por supercapacitor",
             fontsize=27, color=S.INK, va="top", linespacing=1.22)
    fig.lines.append(matplotlib.lines.Line2D([0.062, 0.938], [0.665, 0.665],
                     transform=fig.transFigure, color=S.ACCENT, lw=1.6))

    left = (
        "Instrumento\n"
        "  Nordic Power Profiler Kit II (PCA63100)\n"
        "  Source meter, 100 kS/s, captura sem perda\n\n"
        "Alvo do projeto\n"
        "  2 mA médios, picos ≤ 5 mA\n\n"
        "Estado atual medido\n"
        "  1,60 mA com power-down do ADC (meta de média atingida)\n"
        "  2,53 mA a 3,3 V com ADC a 1 kSPS\n"
        "  6,58 mA a 5,0 V com ADC a 2 kSPS\n\n"
        "Condição de entrada\n"
        "  Eletrodos abertos"
    )
    right = (
        "O que mudou desde o artigo\n\n"
        "A correção mais importante não foi de consumo:\n"
        "a aquisição do ADC nunca funcionou. O loop\n"
        "principal só era acordado pelo timer do LED de\n"
        "1 Hz, então o ADC era lido uma vez por segundo —\n"
        "os 2 kS/s do artigo não estavam acontecendo.\n\n"
        "Agora são 1015 S/s a 1 kSPS (2042 em turbo) com\n"
        "zero conversões perdidas, e o consumo caiu 80% em\n"
        "corrente e 87% em potência em relação ao ponto\n"
        "de partida."
    )
    fig.text(0.062, 0.60, left, fontsize=9.6, color=S.INK_MID, va="top", linespacing=1.75)
    fig.text(0.53, 0.60, right, fontsize=9.6, color=S.INK_MID, va="top", linespacing=1.75)
    S.page_footer(fig, "power_profiling/ · relatorio_consumo.pdf", "PPK2 · nRF52840 · ADS112C04")
    return fig


# -------------------------------------------------------------------- montagem

def build(runs: dict, out_pdf: Path) -> Path:
    S.apply()
    order = [
        lambda: fig_cover(runs),
        lambda: fig_protocol(runs),
        lambda: fig_progression(runs),
    ]
    with PdfPages(out_pdf) as pdf:
        for maker in order:
            pdf.savefig(maker()); plt.close("all")
        for key, label in (("3V3_1kSPS_pd", "3,3 V, 1 kSPS, com power-down"),
                           ("3V3", "trilho de 3,3 V"), ("5V", "trilho de 5,0 V")):
            if key in runs:
                pdf.savefig(fig_trace(runs[key],
                                      f"Corrente ao longo do ciclo de operação — {label}",
                                      "Nove estados, contra os quatro da Fig. 7 do artigo."))
                plt.close("all")
        pdf.savefig(fig_i2c(runs)); plt.close("all")
        pdf.savefig(fig_1khz(runs)); plt.close("all")
        pdf.savefig(fig_voltage(None)); plt.close("all")
        pdf.savefig(fig_results(runs)); plt.close("all")
        d = pdf.infodict()
        d["Title"] = "Caracterizacao de consumo - sensor sEMG BLE"
        d["Subject"] = "Medicao com Nordic PPK2; adequacao ao protocolo experimental"
    return out_pdf


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--run", action="append", default=[], metavar="ROTULO=DIR")
    ap.add_argument("-o", "--out", default="power_profiling/relatorio_consumo.pdf")
    a = ap.parse_args()
    runs = {}
    for spec in a.run:
        label, _, d = spec.partition("=")
        runs[label] = load_run(Path(d))
    out = Path(a.out); out.parent.mkdir(parents=True, exist_ok=True)
    p = build(runs, out)
    print(f"PDF gravado em {p}  ({p.stat().st_size/1024:.0f} kB)")


if __name__ == "__main__":
    main()
