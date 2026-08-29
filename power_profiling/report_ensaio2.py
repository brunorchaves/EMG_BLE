"""Relatorio do Ensaio 2: RMS na janela de operacao + entrega dos dados brutos.

Responde a dois pedidos concretos:
  1. O valor de corrente reportado antes era RMS ou media? E sobre qual janela?
     A preocupacao e legitima: media sobre todo o tempo de operacao inclui as
     fases desligada e desconectada, que MASCARAM o consumo real de uso.
  2. Entregar os pontos de corrente vs tempo, em duas janelas (todas as etapas
     e so conectado+streaming), para plotar e recalcular por conta propria.

Uso:
    python report_ensaio2.py --run <run_dir> -o relatorio_ensaio_2.pdf
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
import report_style as S
from analyze import analyze_run
from export_data import WINDOWS, _load, _slices

WINDOW_LABEL = {
    "todas_etapas": "Todas as etapas",
    "conectado_streaming": "Conectado + streaming",
}


def _text_blocks(fig, blocks, y0: float, width: int = 134, size: float = 8.4) -> float:
    y = y0
    for title, para in blocks:
        fig.text(0.075, y, title, fontsize=10.0, color=S.ACCENT, fontweight="bold")
        y -= 0.024
        wrapped = textwrap.wrap(para, width)
        fig.text(0.075, y, "\n".join(wrapped), fontsize=size, color=S.INK_MID,
                 va="top", linespacing=1.5)
        y -= 0.0205 * len(wrapped) + 0.020
    return y


def load(run_dir: Path) -> dict:
    meta, source_v, blk, raw, fit, bands = _load(run_dir)
    rep = analyze_run(run_dir)
    export = {}
    ex = run_dir / "export" / "resumo_janelas.json"
    if ex.exists():
        export = json.loads(ex.read_text(encoding="utf-8"))
    fs_acq = None
    fwc = run_dir / "fw_counters.json"
    if fwc.exists():
        fs_acq = json.loads(fwc.read_text(encoding="utf-8")).get("fs_acquisition_sps")
    return {
        "dir": run_dir, "meta": meta, "source_v": source_v, "blk": blk, "raw": raw,
        "fit": fit, "bands": bands, "report": rep, "export": export, "fs_acq": fs_acq,
        "by_state": {s["state"]: s for s in rep.state_stats},
    }


def _window_series(run: dict, window: str, n_px: int = 2400):
    """Envoltoria min/max por coluna de pixel da janela pedida. Min/max e nao
    subamostragem: nenhum transiente desaparece do grafico."""
    fs = run["fit"].fs_effective
    sl = _slices(run["bands"], WINDOWS[window], fs)
    cur = np.concatenate([run["blk"].current_uA[lo:hi].astype(np.float64) for lo, hi, _ in sl])
    # fronteiras de estado no eixo concatenado
    marks, acc = [], 0
    for lo, hi, st in sl:
        marks.append((acc / fs, (acc + (hi - lo)) / fs, st))
        acc += hi - lo
    step = max(1, cur.size // n_px)
    n = cur.size // step
    m = cur[: n * step].reshape(n, step)
    t = np.arange(n) * step / fs
    return t, m.min(1) / 1e3, m.max(1) / 1e3, m.mean(1) / 1e3, marks, cur


def fig_cover(run: dict) -> plt.Figure:
    fig = plt.figure(figsize=(11.7, 8.3))
    fig.text(0.062, 0.90, " ".join("ENSAIO 2"), fontsize=8.4, color=S.ACCENT, fontweight="bold")
    fig.text(0.062, 0.83, "Corrente RMS na janela de\noperação, e os dados brutos",
             fontsize=26, color=S.INK, va="top", linespacing=1.22)
    fig.lines.append(matplotlib.lines.Line2D([0.062, 0.938], [0.665, 0.665],
                     transform=fig.transFigure, color=S.ACCENT, lw=1.6))

    ex = run["export"]
    todas = ex.get("todas_etapas", {})
    conec = ex.get("conectado_streaming", {})

    left = (
        "A pergunta\n"
        "  O valor reportado antes era RMS ou média?\n"
        "  E sobre qual janela de tempo?\n\n"
        "A resposta curta\n"
        "  Era a MÉDIA da banda ADVERTISING.\n"
        "  Não era RMS, e não era o run inteiro.\n\n"
        "E a preocupação estava certa\n"
        "  A janela completa inclui as fases desligada\n"
        "  e desconectada, que puxam a média para baixo\n"
        "  e mascaram o consumo real de uso."
    )
    right = (
        "Medido nesta configuração\n"
        f"  Trilho: {run['source_v']:.1f} V     ADC: {run['fs_acq'] or 0:.0f} S/s\n\n"
        f"  Todas as etapas\n"
        f"     média {todas.get('mean_mA', float('nan')):.3f} mA"
        f"     RMS {todas.get('rms_mA', float('nan')):.3f} mA\n\n"
        f"  Conectado + streaming  (pior caso)\n"
        f"     média {conec.get('mean_mA', float('nan')):.3f} mA"
        f"     RMS {conec.get('rms_mA', float('nan')):.3f} mA\n\n"
        "  A janela completa subestima a corrente de\n"
        f"  operação em "
        f"{(1 - todas.get('mean_mA', 1) / max(conec.get('mean_mA', 1), 1e-9)) * 100:.0f}%"
        " na média."
    )
    fig.text(0.062, 0.60, left, fontsize=9.6, color=S.INK_MID, va="top", linespacing=1.75)
    fig.text(0.53, 0.60, right, fontsize=9.6, color=S.INK_MID, va="top", linespacing=1.75)
    S.page_footer(fig, run["dir"].name, "PPK2 · source meter · 100 kS/s")
    return fig


def fig_method(run: dict) -> plt.Figure:
    """A sensibilidade do RMS ao tratamento do artefato da PPK2. Sem isso, o
    numero entregue pode estar errado por um fator de 1,5."""
    fig = plt.figure(figsize=(11.7, 8.3))
    S.page_header(fig, "metodologia", "O RMS depende de como se trata o artefato",
                  "A média é robusta; o RMS não. Isso precisa estar declarado.")

    ex = run["export"].get("conectado_streaming", {})
    conf = [
        ("Bruto\n(sem tratamento)", ex.get("mean_raw_mA", np.nan), ex.get("rms_raw_mA", np.nan),
         ex.get("max_raw_mA", np.nan), S.ALERT),
        ("Spike filter\n(método da Nordic)", ex.get("mean_mA", np.nan), ex.get("rms_mA", np.nan),
         ex.get("max_mA", np.nan), S.ACCENT),
    ]
    ax = fig.add_axes([0.075, 0.545, 0.40, 0.255])
    x = np.arange(len(conf))
    ax.bar(x - 0.19, [c[1] for c in conf], 0.36, color=[c[4] for c in conf], alpha=0.55,
           label="média", zorder=3)
    ax.bar(x + 0.19, [c[2] for c in conf], 0.36, color=[c[4] for c in conf],
           label="RMS", zorder=3)
    for xi, c in zip(x, conf):
        ax.text(xi - 0.19, c[1] + 0.08, f"{c[1]:.2f}", ha="center", fontsize=8)
        ax.text(xi + 0.19, c[2] + 0.08, f"{c[2]:.2f}", ha="center", fontsize=8)
    ax.set_xticks(x); ax.set_xticklabels([c[0] for c in conf], fontsize=8)
    ax.set_ylabel("Corrente (mA)")
    ax.legend(fontsize=7.8, ncol=2, loc="upper left", bbox_to_anchor=(0, -0.14))
    ax.set_title("Conectado + streaming", fontsize=10)

    rows = [("Tratamento", "Média", "RMS", "Máx")]
    for lbl, m, r, mx, _c in conf:
        rows.append((lbl.replace("\n", " "), f"{m:.3f} mA", f"{r:.3f} mA", f"{mx:.1f} mA"))
    tb = fig.add_axes([0.545, 0.545, 0.395, 0.255]); tb.axis("off")
    tbl = tb.table(cellText=rows[1:], colLabels=rows[0], cellLoc="left",
                   loc="upper left", colWidths=[0.42, 0.20, 0.20, 0.18])
    tbl.auto_set_font_size(False); tbl.set_fontsize(8.0); tbl.scale(1, 1.5)
    for (r_, c_), cell in tbl.get_celld().items():
        cell.set_edgecolor(S.LINE); cell.set_linewidth(0.5)
        if r_ == 0:
            cell.set_facecolor("#eef2f3"); cell.set_text_props(color=S.INK, fontweight="bold")
        elif r_ == 2:
            cell.set_facecolor("#e2f0f1")

    body = [
        ("Por que existe artefato",
         "A PPK2 troca a faixa de medição quando a corrente muda de ordem de grandeza, e nas "
         "amostras imediatamente após a transição a leitura é artefato de acomodação. Nesta placa "
         "isso produzia leituras de até 57,7 mA a 3,3 V — fisicamente impossível, o consumo total é "
         "de ~2,7 mA e o pico real fica em ~22 mA. E as trocas de faixa são CAUSADAS pelos picos "
         "reais de corrente (uma excursão de 2 para 20 mA obriga a troca), então artefato e sinal "
         "verdadeiro ocorrem no mesmo instante."),
        ("Por que isso afeta o RMS muito mais que a média",
         "O RMS pondera cada amostra ao quadrado, então é dominado pelos extremos — exatamente onde "
         "o artefato vive. A média, somando linearmente, quase não sente: varia 7,7% entre bruto e "
         "filtrado. O RMS varia 45%. Qualquer número de RMS entregue sem declarar o tratamento é "
         "ambíguo por um fator de ~1,5."),
        ("O tratamento adotado, e um erro que foi corrigido",
         "Adotado o spike filter da própria Nordic: duas médias móveis exponenciais causais rodam "
         "sobre todas as amostras e, nas 3 amostras após cada troca de faixa, a leitura é "
         "SUBSTITUÍDA pela média móvel — não descarta tempo nem usa o artefato como referência. "
         "Um relatório anterior desta bancada descartava ±3 amostras em torno de cada troca, o que "
         "removia 14 a 27% do registro (sistematicamente as altas) e enviesava a média em −27%."),
        ("Como obter um RMS definitivo",
         "A ambiguidade é do instrumento, não da placa. Para eliminá-la: travar a faixa de medição "
         "da PPK2 (o opcode RANGE_SET existe no protocolo), ou medir com shunt e osciloscópio como "
         "referência independente. Até então, o honesto é reportar a faixa: RMS entre 3,85 e "
         "5,60 mA, sendo 3,85 o valor pelo método do fabricante."),
    ]
    _text_blocks(fig, body, y0=0.455)
    S.page_footer(fig, "spike filter: ppk2_decode._apply_spike_filter", "conforme PPK2_API.get_adc_result")
    return fig


def fig_window(run: dict, window: str) -> plt.Figure:
    t, lo_e, hi_e, mu, marks, cur = _window_series(run, window)
    ex = run["export"].get(window, {})
    fig = plt.figure(figsize=(11.7, 8.3))
    title = WINDOW_LABEL[window]
    sub = ("Todas as fases, incluindo desligada e desconectada."
           if window == "todas_etapas" else
           "Somente as fases de uso: o pior caso de consumo.")
    S.page_header(fig, "corrente vs tempo", title, sub)

    ax = fig.add_axes([0.075, 0.46, 0.865, 0.34])
    top = float(np.nanmax(hi_e)) * 1.10
    seen = []
    for t0, t1, st in marks:
        ax.axvspan(t0, t1, color=S.STATE_COLORS.get(st, "#e5e5e5"), alpha=0.32, lw=0, zorder=0)
        if st not in seen:
            seen.append(st)
    ax.fill_between(t, lo_e, hi_e, color=S.ACCENT, alpha=0.30, lw=0, zorder=2)
    ax.plot(t, mu, color="#0a3b40", lw=0.8, zorder=3)

    mean_mA = ex.get("mean_mA", float(cur.mean() / 1000))
    rms_mA = ex.get("rms_mA", float(np.sqrt(np.mean(cur ** 2)) / 1000))
    ax.axhline(mean_mA, color=S.GOOD, ls="-", lw=1.2, zorder=4)
    ax.axhline(rms_mA, color=S.BEFORE, ls=(0, (5, 3)), lw=1.3, zorder=4)
    ax.text(t[-1], mean_mA, f" média {mean_mA:.3f} mA ", color=S.GOOD, fontsize=8,
            va="bottom", ha="right", fontweight="bold")
    ax.text(t[-1], rms_mA, f" RMS {rms_mA:.3f} mA ", color=S.BEFORE, fontsize=8,
            va="bottom", ha="right", fontweight="bold")

    ax.set_xlabel("Tempo na janela (s)"); ax.set_ylabel("Corrente (mA)")
    ax.set_xlim(0, t[-1]); ax.set_ylim(-top * 0.03, top)
    h = [Patch(facecolor=S.STATE_COLORS.get(s, "#e5e5e5"), alpha=0.32, label=s) for s in seen]
    h += [Patch(facecolor=S.ACCENT, alpha=0.30, label="envelope min–max"),
          plt.Line2D([], [], color=S.GOOD, lw=1.4, label="média"),
          plt.Line2D([], [], color=S.BEFORE, lw=1.4, ls=(0, (5, 3)), label="RMS")]
    ax.legend(handles=h, ncol=5, fontsize=7.0, loc="upper center", bbox_to_anchor=(0.5, -0.19))

    stat_rows = [
        ("Duração", f"{ex.get('duration_s', 0):.1f} s"),
        ("Amostras", f"{ex.get('n_samples', 0):,}"),
        ("Média", f"{mean_mA:.3f} mA   ({ex.get('mean_mW', 0):.2f} mW)"),
        ("RMS", f"{rms_mA:.3f} mA   ({ex.get('rms_mW', 0):.2f} mW)"),
        ("p95", f"{ex.get('p95_mA', float('nan')):.2f} mA"),
        ("p99,9", f"{ex.get('p99_9_mA', float('nan')):.2f} mA"),
        ("Máximo", f"{ex.get('max_mA', float('nan')):.2f} mA"),
    ]
    txt = "\n".join(f"  {k:<10} {v}" for k, v in stat_rows)
    fig.text(0.075, 0.30, "Estatísticas da janela", fontsize=10.0, color=S.ACCENT,
             fontweight="bold")
    fig.text(0.075, 0.275, txt, fontsize=8.8, color=S.INK_MID, va="top",
             linespacing=1.7, family="DejaVu Sans")

    files = [
        f"{window}_1000Hz.csv",
        f"{window}_full_uA.npy",
        f"{window}_full.json",
    ]
    fig.text(0.52, 0.30, "Arquivos de dados desta janela", fontsize=10.0, color=S.ACCENT,
             fontweight="bold")
    fig.text(0.52, 0.275, "\n".join("  " + f for f in files) +
             "\n\n  CSV: um bin por linha, com média e RMS do bin.\n"
             "  RMS total = sqrt(média(i_rms_mA²)) — exato.\n"
             "  .npy: 100 kS/s em µA, resolução cheia.",
             fontsize=8.8, color=S.INK_MID, va="top", linespacing=1.7)
    S.page_footer(fig, run["dir"].name, f"{run['source_v']:.1f} V · {run['fs_acq'] or 0:.0f} S/s")
    return fig


def fig_mean_vs_rms(run: dict) -> plt.Figure:
    fig = plt.figure(figsize=(11.7, 8.3))
    S.page_header(fig, "média ou rms?", "Cada uma responde uma pergunta diferente",
                  "Para dimensionar energia use a média. O RMS é para perdas resistivas.")

    ex = run["export"]
    todas, conec = ex.get("todas_etapas", {}), ex.get("conectado_streaming", {})
    rows = [("Janela", "Média (mA)", "RMS (mA)", "Média (mW)", "RMS (mW)")]
    for key, lbl in (("todas_etapas", "Todas as etapas"),
                     ("conectado_streaming", "Conectado + streaming")):
        d = ex.get(key, {})
        rows.append((lbl, f"{d.get('mean_mA', np.nan):.3f}", f"{d.get('rms_mA', np.nan):.3f}",
                     f"{d.get('mean_mW', np.nan):.2f}", f"{d.get('rms_mW', np.nan):.2f}"))
    by = run["by_state"]
    for st, lbl in (("STREAMING", "Somente streaming"),):
        if st in by:
            s = by[st]
            rows.append((lbl, f"{s['mean_uA']/1e3:.3f}", f"{s['rms_uA']/1e3:.3f}",
                         f"{s['mean_mW']:.2f}", f"{s['rms_mW']:.2f}"))

    ax = fig.add_axes([0.075, 0.58, 0.865, 0.22]); ax.axis("off")
    tbl = ax.table(cellText=rows[1:], colLabels=rows[0], cellLoc="right",
                   loc="upper left", colWidths=[0.34, 0.165, 0.165, 0.165, 0.165])
    tbl.auto_set_font_size(False); tbl.set_fontsize(8.6); tbl.scale(1, 1.6)
    for (r_, c_), cell in tbl.get_celld().items():
        cell.set_edgecolor(S.LINE); cell.set_linewidth(0.5)
        if r_ == 0:
            cell.set_facecolor("#eef2f3"); cell.set_text_props(color=S.INK, fontweight="bold")
        if c_ == 0 and r_ > 0:
            cell.set_text_props(ha="left")
        if r_ == 2:
            cell.set_facecolor("#e2f0f1")

    body = [
        ("Para autonomia e dimensionamento de energia: a MÉDIA",
         "A energia consumida numa janela é a integral da corrente vezes a tensão, o que é "
         "exatamente média × V × tempo. É a média que determina quanto tempo o supercapacitor "
         "sustenta o sistema, e é a média que deve entrar no cálculo de autonomia. Usar RMS aí "
         "superestimaria o consumo em ~43% nesta configuração."),
        ("Para perdas resistivas: o RMS",
         "As perdas na ESR do supercapacitor, na resistência série do PMU e em qualquer trilho são "
         "I²R, então dependem do RMS e não da média. É o número certo para verificar aquecimento e "
         "queda de tensão sob carga. Também é o número certo para dimensionar a corrente máxima "
         "contínua que a fonte precisa entregar sem afundar."),
        ("O que muda com desacoplamento local",
         "Os picos que elevam o RMS têm largura mediana de 20 µs e ocorrem uma vez por conversão do "
         "ADC. Com capacitância de bulk junto à carga (~100 µF), esses pulsos são absorvidos "
         "localmente e a fonte passa a ver essencialmente a média. Ou seja: o RMS alto é uma "
         "característica do consumo instantâneo da placa, não necessariamente do que o "
         "supercapacitor vai sentir. Hoje a placa não tem nenhum capacitor de bulk — o maior é de "
         "220 nF."),
    ]
    _text_blocks(fig, body, y0=0.50)
    S.page_footer(fig, "média para energia, RMS para perdas I²R", "")
    return fig


def fig_files(run: dict) -> plt.Figure:
    fig = plt.figure(figsize=(11.7, 8.3))
    S.page_header(fig, "dados entregues", "Inventário dos arquivos",
                  "Como usar cada um, e como recalcular as estatísticas sem erro.")

    export_dir = run["dir"] / "export"
    rows = [("Arquivo", "Tamanho", "O que é")]
    if export_dir.exists():
        for f in sorted(export_dir.iterdir()):
            desc = {
                ".csv": "corrente decimada a 1 kHz, com média e RMS por bin",
                ".npy": "corrente em µA, 100 kS/s, resolução cheia (float32)",
                ".json": "metadados: fs, bandas, tensão do trilho",
            }.get(f.suffix, "")
            if f.name == "resumo_janelas.json":
                desc = "estatísticas de cada janela, já calculadas"
            rows.append((f.name, f"{f.stat().st_size/1024:.0f} kB", desc))

    ax = fig.add_axes([0.062, 0.46, 0.876, 0.34]); ax.axis("off")
    tbl = ax.table(cellText=rows[1:], colLabels=rows[0], cellLoc="left",
                   loc="upper left", colWidths=[0.34, 0.12, 0.54])
    tbl.auto_set_font_size(False); tbl.set_fontsize(8.2); tbl.scale(1, 1.55)
    for (r_, c_), cell in tbl.get_celld().items():
        cell.set_edgecolor(S.LINE); cell.set_linewidth(0.5)
        if r_ == 0:
            cell.set_facecolor("#eef2f3"); cell.set_text_props(color=S.INK, fontweight="bold")

    body = [
        ("Como recalcular média e RMS a partir do CSV",
         "Cada linha do CSV resume um bin de 100 amostras (1 kHz de saída, 100 kS/s de entrada). Os "
         "bins têm tamanho igual, então: média_total = mean(i_mean_mA) e "
         "RMS_total = sqrt(mean(i_rms_mA²)). As duas fórmulas dão o valor EXATO da resolução cheia "
         "— conferido automaticamente na geração, com erro de 5e-6."),
        ("O que NÃO fazer",
         "Não calcule RMS a partir da coluna i_mean_mA. A média dentro de cada bin remove a "
         "variância, e o RMS calculado sobre ela sai sistematicamente baixo. Foi por isso que o CSV "
         "traz a coluna i_rms_mA em vez de só a média: decimar preservando apenas a média "
         "inviabilizaria justamente o cálculo de RMS."),
        ("Para plotar",
         "Use i_min_mA e i_max_mA como envoltória e i_mean_mA como linha central — assim o gráfico "
         "mostra os transientes reais em vez de uma curva suavizada que os esconde. A coluna estado "
         "diz de qual fase veio cada ponto, permitindo sombrear as faixas."),
        ("Nota sobre o eixo de tempo",
         "Na janela conectado+streaming as bandas foram concatenadas, então o tempo é contíguo "
         "dentro de cada fase mas há descontinuidade entre elas (as fases não são adjacentes no run "
         "original). Os índices e tempos originais de cada banda estão no arquivo _full.json."),
    ]
    _text_blocks(fig, body, y0=0.40)
    S.page_footer(fig, str(export_dir), "gerado por export_data.py")
    return fig


def build(run: dict, out_pdf: Path) -> Path:
    S.apply()
    with PdfPages(out_pdf) as pdf:
        for maker in (fig_cover, fig_method):
            pdf.savefig(maker(run)); plt.close("all")
        for window in ("conectado_streaming", "todas_etapas"):
            pdf.savefig(fig_window(run, window)); plt.close("all")
        pdf.savefig(fig_mean_vs_rms(run)); plt.close("all")
        pdf.savefig(fig_files(run)); plt.close("all")
        d = pdf.infodict()
        d["Title"] = "Ensaio 2 - corrente RMS na janela de operacao"
        d["Subject"] = "Medicao com Nordic PPK2; dados brutos para replotagem"
    return out_pdf


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--run", required=True)
    ap.add_argument("-o", "--out", default="power_profiling/relatorio_ensaio_2.pdf")
    a = ap.parse_args()
    run = load(Path(a.run))
    if not run["export"]:
        raise SystemExit(
            "rode export_data.py neste run antes: "
            f"python power_profiling/export_data.py {a.run}"
        )
    out = Path(a.out); out.parent.mkdir(parents=True, exist_ok=True)
    p = build(run, out)
    print(f"PDF gravado em {p}  ({p.stat().st_size/1024:.0f} kB)")


if __name__ == "__main__":
    main()
