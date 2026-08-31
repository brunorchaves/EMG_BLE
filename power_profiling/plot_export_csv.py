#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Plota um CSV exportado por export_data.py e confere as estatisticas.

Deliberadamente AUTOCONTIDO: nao importa nada do repositorio e usa apenas a
biblioteca padrao mais matplotlib. Este arquivo viaja junto com o CSV para
quem for plotar em outra maquina, e nao pode depender do resto da bancada.

Tres coisas que ele faz e que um plot ingenuo do CSV erraria:

1. Recalcula media e RMS pelas formulas do cabecalho e imprime os dois. RMS a
   partir da coluna i_mean_mA sai sistematicamente BAIXO, porque a media
   dentro de cada bin ja removeu a variancia.
2. Desenha a envoltoria i_min/i_max, nao so a media. Os picos desta placa tem
   ~20 us de largura; uma linha de media a 1 kHz os esconde por completo.
3. Repassa os avisos do cabecalho (linhas ATENCAO), que dizem quando uma banda
   nao mede o estado que o rotulo diz.

Uso:
    python plot_export_csv.py conectado_streaming_1000Hz.csv
    python plot_export_csv.py todas_etapas_1000Hz.csv -o grafico.png
    python plot_export_csv.py *.csv --ymax 25       # corta o eixo Y
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

# Cores por estado. Um estado desconhecido cai no cinza e nao quebra o plot.
STATE_COLORS = {
    "OFF": "#e8e8ee", "OFF_FINAL": "#e8e8ee",
    "BOOT": "#c9b8e0",
    "ADVERTISING": "#d6e8f5", "RE_ADVERTISING": "#d6e8f5",
    "CONNECTING": "#eeeeee", "DISCONNECT": "#eeeeee",
    "CONNECTED_IDLE": "#fbe3bd", "CONNECTED_IDLE_2": "#fbe3bd",
    "STREAMING": "#fbd3dd",
}


def read_csv(path: Path):
    """Le o CSV pulando o cabecalho comentado. Retorna (dados, avisos, notas).

    Parsing manual de proposito: as linhas de comentario contem virgulas, e
    leitores de CSV que tentam inferir o cabecalho automaticamente confundem
    uma delas com a linha de nomes de coluna.
    """
    t, mean, rms, lo, hi, state = [], [], [], [], [], []
    warnings, notes = [], []
    cols = None

    with path.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n").rstrip("\r")
            if not line:
                continue
            if line.startswith("#"):
                txt = line.lstrip("#").strip()
                if txt:
                    notes.append(txt)
                    if "ATENCAO" in txt or "ATENÇÃO" in txt:
                        warnings.append(txt)
                    elif warnings and not txt.startswith("conferido"):
                        # as linhas seguintes ao ATENCAO fazem parte do aviso
                        warnings.append(txt)
                continue
            parts = line.split(",")
            if cols is None:
                cols = parts
                need = ["t_s", "i_mean_mA", "i_rms_mA", "i_min_mA", "i_max_mA", "estado"]
                missing = [c for c in need if c not in cols]
                if missing:
                    raise SystemExit(f"{path.name}: faltam colunas {missing}; achei {cols}")
                idx = {c: cols.index(c) for c in need}
                continue
            try:
                t.append(float(parts[idx["t_s"]]))
                mean.append(float(parts[idx["i_mean_mA"]]))
                rms.append(float(parts[idx["i_rms_mA"]]))
                lo.append(float(parts[idx["i_min_mA"]]))
                hi.append(float(parts[idx["i_max_mA"]]))
                state.append(parts[idx["estado"]].strip())
            except (ValueError, IndexError):
                continue  # linha truncada no fim do arquivo

    if not t:
        raise SystemExit(f"{path.name}: nenhuma linha de dados")
    return {"t": t, "mean": mean, "rms": rms, "min": lo, "max": hi, "state": state}, warnings, notes


def stats(d) -> dict:
    """Media e RMS globais pelas formulas do cabecalho do CSV."""
    n = len(d["mean"])
    m = sum(d["mean"]) / n
    r = math.sqrt(sum(v * v for v in d["rms"]) / n)
    # o erro que este script existe para evitar: RMS a partir da media do bin
    r_errado = math.sqrt(sum(v * v for v in d["mean"]) / n)
    return {
        "n_bins": n, "duracao_s": d["t"][-1] - d["t"][0],
        "media_mA": m, "rms_mA": r, "rms_se_calculado_da_media_mA": r_errado,
        "min_mA": min(d["min"]), "max_mA": max(d["max"]),
    }


def aggregate(d, n_px: int):
    """Agrega os bins em no maximo n_px colunas.

    Retorna, por coluna: tempo, media das medias, min/max das medias (a faixa
    onde a media de fato transita) e min/max absolutos (a envoltoria bruta).
    Sem isso, um tracado ponto a ponto de centenas de milhares de bins liga
    base e picos e pinta a area toda como se fosse corrente sustentada.
    """
    n = len(d["t"])
    step = max(1, n // n_px)
    if step == 1:
        return {"t": d["t"], "mu": d["mean"], "mu_lo": d["mean"], "mu_hi": d["mean"],
                "lo": d["min"], "hi": d["max"], "state": d["state"], "step": 1}
    t, mu, mu_lo, mu_hi, lo, hi, stt = [], [], [], [], [], [], []
    for i in range(0, n - step + 1, step):
        sl = slice(i, i + step)
        m = d["mean"][sl]
        t.append(d["t"][i])
        mu.append(sum(m) / len(m))
        mu_lo.append(min(m)); mu_hi.append(max(m))
        lo.append(min(d["min"][sl])); hi.append(max(d["max"][sl]))
        stt.append(d["state"][i])
    return {"t": t, "mu": mu, "mu_lo": mu_lo, "mu_hi": mu_hi,
            "lo": lo, "hi": hi, "state": stt, "step": step}


def bands(d):
    """Trechos contiguos de mesmo estado, para sombrear o fundo."""
    out, start, cur = [], d["t"][0], d["state"][0]
    for i in range(1, len(d["state"])):
        if d["state"][i] != cur:
            out.append((start, d["t"][i], cur))
            start, cur = d["t"][i], d["state"][i]
    out.append((start, d["t"][-1], cur))
    return out


def plot(d, st, path: Path, out: Path, ymax: float | None, n_px: int) -> Path:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Patch

    g = aggregate(d, n_px)
    fig, ax = plt.subplots(figsize=(13, 6.5))
    seen = []
    for t0, t1, name in bands(g):
        ax.axvspan(t0, t1, color=STATE_COLORS.get(name, "#eeeeee"), alpha=0.55, lw=0, zorder=0)
        if name not in seen:
            seen.append(name)

    ax.fill_between(g["t"], g["lo"], g["hi"], color="#a8d3da", alpha=0.55, lw=0,
                    zorder=2)
    ax.fill_between(g["t"], g["mu_lo"], g["mu_hi"], color="#4c9aa8", alpha=0.75, lw=0,
                    zorder=3)
    ax.plot(g["t"], g["mu"], color="#0a3b40", lw=0.9, zorder=4)
    ax.axhline(st["media_mA"], color="#1a7f37", lw=1.3, zorder=5)
    ax.axhline(st["rms_mA"], color="#b3541e", lw=1.3, ls=(0, (5, 3)), zorder=5)
    ax.text(d["t"][0], st["media_mA"], f"  média {st['media_mA']:.3f} mA",
            color="#1a7f37", fontsize=9, va="top", ha="left", fontweight="bold", zorder=6)
    ax.text(d["t"][-1], st["rms_mA"], f"RMS {st['rms_mA']:.3f} mA  ",
            color="#b3541e", fontsize=9, va="bottom", ha="right", fontweight="bold", zorder=6)

    ax.set_xlabel("Tempo na janela (s)")
    ax.set_ylabel("Corrente (mA)")
    ax.set_xlim(d["t"][0], d["t"][-1])
    if ymax:
        ax.set_ylim(-ymax * 0.03, ymax)
        n_acima = sum(1 for v in d["max"] if v > ymax)
        if n_acima:
            ax.text(0.995, 0.97, f"{n_acima} bins com máximo acima de {ymax:g} mA",
                    transform=ax.transAxes, ha="right", va="top", fontsize=8, color="#b3541e")
    else:
        ax.set_ylim(-st["max_mA"] * 0.03, st["max_mA"] * 1.08)
    sub = (f"  ·  {len(d['t']):,} bins agregados em {len(g['t']):,} colunas "
           f"({g['step']} bins por coluna)" if g["step"] > 1 else "")
    ax.set_title(path.name + sub, fontsize=11)

    h = [Patch(facecolor=STATE_COLORS.get(s, "#eeeeee"), alpha=0.55, label=s) for s in seen]
    h += [Patch(facecolor="#a8d3da", alpha=0.55, label="envoltória min–max"),
          Patch(facecolor="#4c9aa8", alpha=0.75, label="faixa da média por bin"),
          plt.Line2D([], [], color="#0a3b40", lw=1.2, label="média agregada"),
          plt.Line2D([], [], color="#1a7f37", lw=1.4, label="média da janela"),
          plt.Line2D([], [], color="#b3541e", lw=1.4, ls=(0, (5, 3)), label="RMS da janela")]
    ax.legend(handles=h, ncol=5, fontsize=8, loc="upper center", bbox_to_anchor=(0.5, -0.11))
    fig.subplots_adjust(bottom=0.26, top=0.93, left=0.07, right=0.98)
    fig.savefig(out, dpi=150)
    plt.close(fig)
    return out


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", nargs="+", help="arquivo(s) *_1000Hz.csv")
    ap.add_argument("-o", "--out", default=None,
                    help="PNG de saida (default: mesmo nome do CSV, com .png)")
    ap.add_argument("--ymax", type=float, default=None,
                    help="limite do eixo Y em mA; util porque poucas amostras "
                         "extremas achatam o resto do grafico")
    ap.add_argument("--no-plot", action="store_true", help="so imprime as estatisticas")
    ap.add_argument("--px", type=int, default=1800,
                    help="colunas do grafico (default 1800). Bins sao agregados "
                         "por min/max/media; sem isso o tracado vira um borrao")
    ap.add_argument("--de", type=float, default=None, metavar="SEG",
                    help="recorta a partir deste instante (para dar zoom)")
    ap.add_argument("--ate", type=float, default=None, metavar="SEG",
                    help="recorta ate este instante")
    a = ap.parse_args()

    if a.out and len(a.csv) > 1:
        raise SystemExit("-o so faz sentido com um CSV de cada vez")

    for name in a.csv:
        path = Path(name)
        if not path.exists():
            print(f"nao encontrei {path}", file=sys.stderr)
            continue
        d, warns, _notes = read_csv(path)
        if a.de is not None or a.ate is not None:
            t0 = a.de if a.de is not None else d["t"][0]
            t1 = a.ate if a.ate is not None else d["t"][-1]
            keep = [i for i, tv in enumerate(d["t"]) if t0 <= tv <= t1]
            if not keep:
                raise SystemExit(f"nenhum ponto entre {t0} e {t1} s")
            d = {k: [v[i] for i in keep] for k, v in d.items()}
            print(f"\n  (recorte: {t0:.3f} a {t1:.3f} s — as estatísticas abaixo "
                  f"são apenas deste trecho)")
        st = stats(d)

        print(f"\n=== {path.name} ===")
        print(f"  bins: {st['n_bins']:,}   duracao: {st['duracao_s']:.1f} s")
        print(f"  MEDIA  {st['media_mA']:.4f} mA")
        print(f"  RMS    {st['rms_mA']:.4f} mA")
        print(f"  min / max por bin: {st['min_mA']:.3f} / {st['max_mA']:.2f} mA")
        print(f"  (se o RMS fosse calculado da coluna i_mean_mA daria "
              f"{st['rms_se_calculado_da_media_mA']:.4f} mA - "
              f"{(1 - st['rms_se_calculado_da_media_mA'] / st['rms_mA']) * 100:.1f}% baixo)")
        for w in warns:
            print("  ! " + w)

        if not a.no_plot:
            out = Path(a.out) if a.out else path.with_suffix(".png")
            print("  grafico: " + str(plot(d, st, path, out, a.ymax, a.px)))


if __name__ == "__main__":
    main()
