#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Analisa as capturas de osciloscopio do ensaio de qualidade de sinal.

Le os CSV de dois canais que o osciloscopio exporta (cabecalho "x-axis,1,2" /
"second,Volt,Volt"), reconhece a estrutura do laco do player e mede o que da
para medir.

Como reconhece o laco: o marcador de alinhamento sao 3 ciclos de 100 Hz (30 ms)
cercados por >= 1,7 s de 0 V. Nas taxas baixas que o osciloscopio usou ele vira
um pico estreito e solitario, que e exatamente o que o detector procura. Achado
o marcador, o resto do laco e geometria: a amostra comeca 1,800 s depois dele
(marcador em 0,200 s do laco, amostra em 2,000 s) e dura 4,000 s.

    python tools/analisa_capturas.py <pasta-com-os-csv>
    python tools/analisa_capturas.py /mnt/pendrive --saida analise/

Gera, na pasta de saida: resumo.csv, resumo.md, visao_geral.png e um PNG por
captura com a estrutura do laco sobreposta.
"""

from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[1]

# Geometria do laco, de stimulus.json (conferida aqui no load_meta)
MARKER_AT_S = 0.200
SAMPLE_AT_S = 2.000
SAMPLE_LEN_S = 4.000
LOOP_S = 7.500
EMG_BAND_HZ = (20.0, 400.0)   # a banda que o firmware da placa sEMG filtra


def load_meta() -> dict:
    p = ROOT / "stimulus" / "stimulus.json"
    if not p.exists():
        return {}
    m = json.loads(p.read_text(encoding="utf-8"))
    t = m["table"]
    # se a tabela mudar, estes numeros mudam junto - melhor falhar alto
    assert abs(t["loop_s"] - LOOP_S) < 1e-6, f"laco da tabela e {t['loop_s']} s, nao {LOOP_S}"
    lay = m["loop_layout_samples"]
    fs = t["fs"]
    assert abs(lay["marker"][0] / fs - MARKER_AT_S) < 1e-6
    assert abs(lay["segment"][0] / fs - SAMPLE_AT_S) < 1e-6
    return m


def load_csv(path: Path) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Devolve (t, ch1, ch2). Descarta as linhas do fim em que o osciloscopio
    estende o eixo de tempo sem dado."""
    with open(path, newline="") as f:
        rows = list(csv.reader(f))
    data = [r for r in rows[2:] if len(r) >= 3 and r[1].strip() and r[2].strip()]
    if not data:
        return np.array([]), np.array([]), np.array([])
    arr = np.array([[float(c) for c in r[:3]] for r in data])
    return arr[:, 0], arr[:, 1], arr[:, 2]


def find_markers(t: np.ndarray, x: np.ndarray, fs: float) -> list[float]:
    y = np.abs(x - np.median(x))
    if y.max() <= 0:
        return []
    act = y > 0.3 * y.max()
    edges = np.flatnonzero(np.diff(np.r_[0, act.astype(np.int8), 0]))
    starts, ends = edges[::2], edges[1::2]
    out = []
    for a, b in zip(starts, ends):
        if (b - a) / fs > 0.15:        # largo = a amostra de EMG, nao o marcador
            continue
        prev_end = ends[ends <= a]
        next_start = starts[starts >= b]
        gap_before = (a - prev_end[-1]) / fs if len(prev_end) else np.inf
        gap_after = (next_start[0] - b) / fs if len(next_start) else np.inf
        if min(gap_before, gap_after) > 1.0:   # isolado por zona morta
            out.append(float(t[a]))
    return out


def rms(x: np.ndarray) -> float:
    return float(np.sqrt(np.mean(x * x))) if len(x) else float("nan")


def analisa(path: Path) -> dict:
    t, ch1, ch2 = load_csv(path)
    r: dict = {"arquivo": path.name, "n": len(t)}
    if len(t) < 50:
        r["obs"] = f"captura truncada ({len(t)} pontos)"
        return r

    dt = float(np.median(np.diff(t)))
    fs = 1.0 / dt
    r.update(fs_hz=round(fs, 1), t0_s=round(float(t[0]), 3), t1_s=round(float(t[-1]), 3),
             dur_s=round(float(t[-1] - t[0]), 3))

    # Qual canal tem o estimulo: ele repousa na meia escala do DAC (~1,6 V) e nao
    # em zero, porque nao passa por acoplamento AC. Exige um patamar claro - se
    # nenhum canal tiver, o DAC nao estava sendo medido nesta captura, e chutar
    # um canal poria um rotulo errado no relatorio.
    PLATEAU_MIN_V = 0.5
    off1, off2 = abs(float(np.median(ch1))), abs(float(np.median(ch2)))
    if max(off1, off2) < PLATEAU_MIN_V:
        r["canal_estimulo"] = "nao identificado"
        r["obs"] = "nenhum canal repousa num patamar de DAC - o estimulo nao estava sendo medido"
        stim, other = ch2, ch1          # segue medindo o que der, sem rotular
    else:
        stim, other, nome = (ch2, ch1, "ch2") if off2 > off1 else (ch1, ch2, "ch1")
        r["canal_estimulo"] = nome
        r["estimulo_repouso_V"] = round(float(np.median(stim)), 4)

    marks = find_markers(t, stim, fs)
    r["marcadores_s"] = ";".join(f"{m:.3f}" for m in marks)
    if len(marks) >= 2:
        d = np.diff(marks)
        r["periodo_laco_s"] = round(float(np.mean(d)), 4)

    for nome, x in (("estimulo", stim), ("outro", other)):
        r[f"{nome}_pp_V"] = round(float(x.max() - x.min()), 4)
        # ceifamento: muitas amostras coladas no extremo
        for lim, lab in ((x.max(), "max"), (x.min(), "min")):
            n_lim = int(np.sum(np.abs(x - lim) < 1e-4))
            r[f"{nome}_n_no_{lab}"] = n_lim
    r["ceifa"] = ("sim" if max(r["estimulo_n_no_max"], r["estimulo_n_no_min"],
                               r["outro_n_no_max"], r["outro_n_no_min"]) > 5 else "nao")

    # janela da amostra e zona morta, a partir do marcador
    if marks:
        m0 = marks[0]
        a, b = m0 + (SAMPLE_AT_S - MARKER_AT_S), m0 + (SAMPLE_AT_S - MARKER_AT_S) + SAMPLE_LEN_S
        dz0, dz1 = b, m0 + (LOOP_S - MARKER_AT_S)      # zona morta de saida
        r["amostra_de_s"], r["amostra_ate_s"] = round(a, 3), round(b, 3)
        in_s = (t >= a) & (t < b)
        in_d = (t >= dz0 + 0.1) & (t < dz1 - 0.1)      # margem, para nao pegar borda
        if in_s.sum() > 20 and in_d.sum() > 20:
            for nome, x in (("estimulo", stim), ("outro", other)):
                xs, xd = x[in_s] - np.median(x[in_d]), x[in_d] - np.median(x[in_d])
                r[f"{nome}_rms_amostra_V"] = round(rms(xs), 5)
                r[f"{nome}_rms_zona_morta_V"] = round(rms(xd), 5)
                if rms(xd) > 0:
                    r[f"{nome}_snr_dB"] = round(20 * np.log10(rms(xs) / rms(xd)), 2)
    return r


def plot_capture(path: Path, out: Path, r: dict) -> None:
    t, ch1, ch2 = load_csv(path)
    if len(t) < 50:
        return
    fig, ax = plt.subplots(figsize=(12, 3.4))
    ax.plot(t, ch2, lw=0.6, color="#c0392b", label="ch2")
    ax.plot(t, ch1, lw=0.5, color="#1f77b4", label="ch1")
    for m in [float(x) for x in r.get("marcadores_s", "").split(";") if x]:
        ax.axvline(m, color="#16a085", lw=1.1, ls="-", alpha=.9)
        a = m + (SAMPLE_AT_S - MARKER_AT_S)
        ax.axvspan(a, a + SAMPLE_LEN_S, color="#2ecc71", alpha=.10, zorder=0)
        ax.axvspan(a + SAMPLE_LEN_S, m + (LOOP_S - MARKER_AT_S), color="#f39c12", alpha=.10, zorder=0)
    snr = r.get("outro_snr_dB")
    titulo = (f"{path.stem}   {r.get('fs_hz', '?')} S/s, {r.get('n')} pts"
              f"   estimulo em {r.get('canal_estimulo', '?')}")
    if snr is not None:
        titulo += f"   SNR(outro) {snr:.1f} dB"
    if r.get("ceifa") == "sim":
        titulo += "   !! CEIFA"
    ax.set_title(titulo, fontsize=9, loc="left")
    ax.set_xlabel("tempo (s)")
    ax.set_ylabel("V")
    ax.grid(alpha=.25)
    ax.legend(fontsize=7, loc="upper right")
    fig.tight_layout()
    fig.savefig(out, dpi=110)
    plt.close(fig)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pasta", type=Path, help="pasta com os CSV do osciloscopio")
    ap.add_argument("--saida", type=Path, default=ROOT / "analise")
    ap.add_argument("--padrao", default="*.csv")
    ap.add_argument("--ganho-clinico", type=float, default=None, metavar="G",
                    help="ganho do EMG clinico; converte o canal dele para mV nos terminais")
    ap.add_argument("--atenuacao", type=float, default=None, metavar="N",
                    help="atenuacao do divisor entre o DAC e os terminais (ex.: 213.8 para "
                         "100k/470R). Com ela o resumo infere o ganho do clinico pela razao "
                         "entre os dois canais, que serve de conferencia independente.")
    args = ap.parse_args()

    meta = load_meta()
    files = sorted(args.pasta.rglob(args.padrao),
                   key=lambda p: [int(s) if s.isdigit() else s for s in re.split(r"(\d+)", p.name)])
    if not files:
        raise SystemExit(f"nenhum '{args.padrao}' em {args.pasta}")
    args.saida.mkdir(parents=True, exist_ok=True)
    (args.saida / "plots").mkdir(exist_ok=True)

    rows = [analisa(f) for f in files]
    if args.ganho_clinico:
        for r in rows:
            for k in ("outro_pp_V", "outro_rms_amostra_V", "outro_rms_zona_morta_V"):
                if k in r:
                    r[k.replace("_V", "_mV_terminais")] = round(r[k] / args.ganho_clinico * 1000, 4)
    for f, r in zip(files, rows):
        plot_capture(f, args.saida / "plots" / f"{f.stem}.png", r)

    campos: list[str] = []
    for r in rows:
        for k in r:
            if k not in campos:
                campos.append(k)
    with open(args.saida / "resumo.csv", "w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=campos)
        w.writeheader()
        w.writerows(rows)

    # --- visao geral ---
    ok = [r for r in rows if r.get("n", 0) >= 50]
    fig, axes = plt.subplots(2, 2, figsize=(12, 7))
    fsv = [r["fs_hz"] for r in ok]
    axes[0, 0].hist(fsv, bins=20, color="#c0392b")
    axes[0, 0].axvline(2 * EMG_BAND_HZ[1], color="k", ls="--", lw=1)
    axes[0, 0].text(2 * EMG_BAND_HZ[1], 0.9 * axes[0, 0].get_ylim()[1],
                    f"  minimo p/ {EMG_BAND_HZ[1]:.0f} Hz\n  (Nyquist)", fontsize=7, va="top")
    axes[0, 0].set_title("taxa de amostragem das capturas (S/s)", fontsize=9, loc="left")
    axes[0, 0].set_xlabel("S/s")

    per = [r["periodo_laco_s"] for r in ok if "periodo_laco_s" in r]
    axes[0, 1].axis("off")
    axes[0, 1].text(0, 1, "Periodo do laco medido\n\n" +
                    (f"n = {len(per)} capturas com 2 marcadores\n"
                     f"media  {np.mean(per):.4f} s\n"
                     f"desvio {np.std(per):.4f} s\n"
                     f"nominal {LOOP_S:.3f} s\n\n"
                     f"erro {abs(np.mean(per) - LOOP_S) * 1000:.1f} ms "
                     f"({abs(np.mean(per) / LOOP_S - 1) * 100:.3f} %)"
                     if per else "nenhuma captura tem dois marcadores"),
                    va="top", family="monospace", fontsize=9)

    snr = [(r["arquivo"], r["outro_snr_dB"]) for r in ok if "outro_snr_dB" in r]
    if snr:
        axes[1, 0].barh(range(len(snr)), [s for _, s in snr], color="#1f77b4")
        axes[1, 0].set_yticks(range(len(snr)))
        axes[1, 0].set_yticklabels([n.replace(".csv", "") for n, _ in snr], fontsize=6)
        axes[1, 0].set_xlabel("dB")
        axes[1, 0].set_title("SNR do canal 'outro': RMS(amostra) / RMS(zona morta)", fontsize=9, loc="left")
        axes[1, 0].grid(alpha=.25, axis="x")
    else:
        axes[1, 0].axis("off")

    pp = [r["estimulo_pp_V"] for r in ok]
    axes[1, 1].hist(pp, bins=20, color="#f39c12")
    axes[1, 1].axvline(2.8, color="k", ls="--", lw=1)
    axes[1, 1].text(2.8, 0.9 * axes[1, 1].get_ylim()[1], "  comandado\n  2,800 V pp", fontsize=7, va="top")
    axes[1, 1].set_title("excursao do estimulo (V pp)", fontsize=9, loc="left")
    axes[1, 1].set_xlabel("V pp")
    fig.suptitle("Capturas do ensaio de qualidade de sinal - visao geral", fontsize=11)
    fig.tight_layout()
    fig.savefig(args.saida / "visao_geral.png", dpi=120)
    plt.close(fig)

    # --- resumo em markdown ---
    trunc = [r for r in rows if r.get("n", 0) < 50]
    sem_marca = [r for r in ok if not r.get("marcadores_s")]
    ceifa = [r for r in ok if r.get("ceifa") == "sim"]
    nyq_ok = [r for r in ok if r["fs_hz"] >= 2 * EMG_BAND_HZ[1]]
    md = [f"# Capturas do ensaio de qualidade de sinal\n",
          f"{len(files)} arquivos, {len(ok)} com dado utilizavel.\n",
          "## Taxa de amostragem\n",
          f"- faixa: {min(fsv):.0f} a {max(fsv):.0f} S/s (mediana {np.median(fsv):.0f})",
          f"- para a banda de {EMG_BAND_HZ[0]:.0f}-{EMG_BAND_HZ[1]:.0f} Hz o minimo teorico e "
          f"{2 * EMG_BAND_HZ[1]:.0f} S/s",
          f"- capturas que atendem: **{len(nyq_ok)} de {len(ok)}**",
          f"- com {np.median(fsv):.0f} S/s o Nyquist e {np.median(fsv) / 2:.0f} Hz, e "
          f"{(EMG_BAND_HZ[1] - np.median(fsv) / 2) / (EMG_BAND_HZ[1] - EMG_BAND_HZ[0]) * 100:.0f} % "
          "da banda cai acima dele",
          "- **se cada arquivo tem ~2000 pontos, a taxa baixa e do SALVAMENTO, nao da "
          "aquisicao**: 2000 e o registro de tela do InfiniiVision. Nos prints da sessao de "
          "2026-10-07 o Agilent DSO-X 2012A estava adquirindo a 2,5 kSa/s. Ao salvar em CSV, "
          "por o comprimento em Max dentro de Definicoes traz a memoria inteira, e o arquivo "
          "passa a ter ~18 mil linhas para os 7,5 s do laco.\n",
          "## Periodo do laco\n"]
    md.append(f"- medido em {len(per)} captura(s) com dois marcadores: "
              f"**{np.mean(per):.4f} s** (desvio {np.std(per):.4f} s), nominal {LOOP_S:.3f} s\n"
              if per else "- nenhuma captura contem dois marcadores\n")

    if args.atenuacao:
        rz = [r["outro_pp_V"] / r["estimulo_pp_V"] for r in ok
              if r.get("estimulo_pp_V") and r.get("outro_pp_V") and r.get("canal_estimulo", "").startswith("ch")]
        if rz:
            g = np.median(rz) * args.atenuacao
            md += ["\n## Ganho do clinico, inferido dos dados\n",
                   f"- atenuacao informada: 1:{args.atenuacao:.1f}",
                   f"- razao mediana entre os canais (n={len(rz)}): {np.median(rz):.4f}",
                   f"- **ganho implicito {g:.0f}x**",
                   "- serve de conferencia contra o ajuste do aparelho: se nao casar com nenhum "
                   "valor da tabela de ganhos, ou a atenuacao ou o ajuste esta diferente do "
                   "anotado\n"]

    pps = [r["outro_pp_mV_terminais"] for r in ok if "outro_pp_mV_terminais" in r]
    if pps:
        md += [f"\n## Nivel nos terminais (ganho do clinico = {args.ganho_clinico:g}x)\n",
               f"- mediana **{np.median(pps):.2f} mV pp**, faixa {min(pps):.2f} a {max(pps):.2f}",
               "- projeto do atenuador 1:501 (CIRCUITO_CONDICIONAMENTO): **5,59 mV pp**",
               "- faixa fisiologica de sEMG citada no artigo: pp abaixo de 6 mV",
               f"- razao medido/projeto: **{np.median(pps) / 5.59:.2f}x**\n"]
    md += ["## Problemas por captura\n",
           f"- truncadas (< 50 pontos): {', '.join(r['arquivo'] for r in trunc) or 'nenhuma'}",
           f"- sem marcador na janela: {len(sem_marca)} de {len(ok)}",
           f"- com ceifamento: {', '.join(r['arquivo'] for r in ceifa) or 'nenhuma'}\n",
           "## Tabela\n",
           "| arquivo | S/s | janela (s) | estim. | repouso V | marcador(es) | pp estim. | SNR outro | ceifa |",
           "|---|---|---|---|---|---|---|---|---|"]
    for r in rows:
        if r.get("n", 0) < 50:
            md.append(f"| {r['arquivo']} | — | — | — | — | — | — | — | {r.get('obs', '')} |")
            continue
        rep = f"{r['estimulo_repouso_V']:.3f}" if "estimulo_repouso_V" in r else "—"
        md.append(f"| {r['arquivo']} | {r['fs_hz']:.0f} | {r['t0_s']:.2f}..{r['t1_s']:.2f} | "
                  f"{r['canal_estimulo']} | {rep} | "
                  f"{r.get('marcadores_s', '') or '—'} | {r['estimulo_pp_V']:.3f} | "
                  f"{r.get('outro_snr_dB', '—')} | {r['ceifa']} |")
    (args.saida / "resumo.md").write_text("\n".join(md) + "\n", encoding="utf-8", newline="\n")

    print(f"{len(files)} capturas -> {args.saida}")
    print(f"  resumo.csv, resumo.md, visao_geral.png, plots/ ({len(ok)} PNGs)")
    print(f"  fs mediana {np.median(fsv):.0f} S/s; {len(nyq_ok)}/{len(ok)} atendem a banda de "
          f"{EMG_BAND_HZ[1]:.0f} Hz")
    if per:
        print(f"  periodo do laco {np.mean(per):.4f} s (nominal {LOOP_S:.3f})")


if __name__ == "__main__":
    main()
