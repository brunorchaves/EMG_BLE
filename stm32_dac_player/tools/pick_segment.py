"""Escolhe o trecho de 4 s de Sidekicking que vai para o DAC.

O trecho exato da Fig. 5 do artigo nao e recuperavel: o envelope
digitalizado da figura nao correlaciona (r < 0,66) com nenhuma janela de
nenhum sujeito, canal, acao ou escala de tempo do dataset - o "Original
Signal" foi pre-processado de um jeito que nao ficou registrado. Entao a
escolha e por criterio, e reprodutivel:

1. so canais de perna (R-Thi, R-Ham, L-Thi, L-Ham), so Sidekicking;
2. no maximo 0,5 % das amostras ceifadas (+-4000 uV). Zero nao da: fora
   do sub2, praticamente toda janela de 4 s tem algum pico ceifado;
3. 8 a 18 rajadas pelo metodo do artigo (limiar de 20 % fragmenta as
   rajadas; a Fig. 5 mostra ~13 trechos cinza);
4. a SNR pelo metodo do artigo mais perto de 10,7 dB, que e o valor
   publicado para o sinal original; empate decide pelo menor ceifamento.

O sub2 fica de fora por padrao: o readme da UCI avisa que ele nao foi
filtrado e e ruidoso (--include-sub2 para incluir).

    python stm32_dac_player/tools/pick_segment.py
Gera stm32_dac_player/stimulus/candidates.csv e candidates.png.
"""

from __future__ import annotations

import argparse
import csv
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from emg_dataset import ARTICLE_SNR_DB, CHANNELS, CLIP_UV, FS_SRC, LEG_COLUMNS, article_metrics, load_action

OUT_DIR = Path(__file__).resolve().parents[1] / "stimulus"
RESERVE_CLIP_PCT = 5.0


def scan(subjects, duration_s: float, step_s: float, max_clip_pct: float):
    n = int(duration_s * FS_SRC)
    hop = int(step_s * FS_SRC)
    rows = []
    for s in subjects:
        d = load_action(s)
        for col in LEG_COLUMNS:
            x = d[:, col]
            for k in range(0, len(x) - n + 1, hop):
                w = x[k : k + n]
                clip_pct = 100.0 * np.mean(np.abs(w) >= CLIP_UV - 1)
                if clip_pct > max_clip_pct:
                    continue
                m = article_metrics(w, FS_SRC)
                if np.isnan(m.snr_db):
                    continue
                rows.append(
                    dict(subject=s, column=col, channel=CHANNELS[col], start_s=k / FS_SRC,
                         snr_db=round(m.snr_db, 2), bursts=m.bursts, clip_pct=round(clip_pct, 2),
                         err_db=round(abs(m.snr_db - ARTICLE_SNR_DB), 2))
                )
    return rows


def rank(rows, min_bursts: int, max_bursts: int, max_clip_pct: float):
    """Primeiro os que passam no limite de ceifamento; os reservas (ate
    RESERVE_CLIP_PCT) vem depois, so para a comparacao visual."""
    ok = [r for r in rows if min_bursts <= r["bursts"] <= max_bursts]
    return sorted(ok, key=lambda r: (r["clip_pct"] > max_clip_pct, r["err_db"], r["clip_pct"]))


def plot_grid(best, duration_s: float, path: Path) -> None:
    fig, axes = plt.subplots(len(best), 1, figsize=(10, 1.9 * len(best)), sharex=True)
    axes = np.atleast_1d(axes)
    t = np.arange(int(duration_s * FS_SRC)) / FS_SRC
    for ax, (i, r) in zip(axes, enumerate(best)):
        k = int(r["start_s"] * FS_SRC)
        x = load_action(r["subject"])[k : k + len(t), r["column"]]
        x = x - x.mean()
        x = x / np.max(np.abs(x))
        m = article_metrics(x, FS_SRC)
        ax.plot(t, x, lw=0.5, color="#1f77b4")
        ax.plot(t, m.envelope / m.envelope.max(), lw=1.6, color="k")
        ax.fill_between(t, -1, 1, where=m.active, color="0.85", zorder=0, step="mid")
        ax.set_ylim(-1.05, 1.05)
        ax.set_ylabel(f"#{i + 1}", rotation=0, labelpad=14)
        ax.set_title(
            f"#{i + 1}  sub{r['subject']} col{r['column']} {r['channel']}  inicio {r['start_s']:.2f} s  "
            f"SNR {r['snr_db']:.1f} dB  rajadas {r['bursts']}  ceifado {r['clip_pct']:.2f} %",
            fontsize=9, loc="left",
        )
    axes[-1].set_xlabel("tempo (s)")
    fig.suptitle("Candidatos a trecho de estimulo - compare com a Fig. 5 (Original Signal)", fontsize=11)
    fig.tight_layout()
    fig.savefig(path, dpi=130)
    plt.close(fig)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--duration", type=float, default=4.0)
    ap.add_argument("--step", type=float, default=0.05)
    ap.add_argument("--min-bursts", type=int, default=8)
    ap.add_argument("--max-bursts", type=int, default=18)
    ap.add_argument("--max-clip-pct", type=float, default=0.5)
    ap.add_argument("--top", type=int, default=8)
    ap.add_argument("--include-sub2", action="store_true")
    args = ap.parse_args()

    subjects = [1, 2, 3, 4] if args.include_sub2 else [1, 3, 4]
    rows = scan(subjects, args.duration, args.step, max(args.max_clip_pct, RESERVE_CLIP_PCT))
    ranked = rank(rows, args.min_bursts, args.max_bursts, args.max_clip_pct)
    if not ranked:
        raise SystemExit("nenhuma janela passou nos criterios")

    # um candidato por (sujeito, canal) e janelas que nao se sobreponham
    best, seen = [], []
    for r in ranked:
        if any(r["subject"] == b["subject"] and r["column"] == b["column"] and abs(r["start_s"] - b["start_s"]) < args.duration
               for b in seen):
            continue
        seen.append(r)
        best.append(r)
        if len(best) == args.top:
            break

    OUT_DIR.mkdir(exist_ok=True)
    with open(OUT_DIR / "candidates.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(best[0].keys()))
        w.writeheader()
        w.writerows(best)
    plot_grid(best, args.duration, OUT_DIR / "candidates.png")

    print(f"{'#':>2}  sujeito canal         inicio   SNR    |erro|  rajadas  ceifado")
    for i, r in enumerate(best, 1):
        tag = "" if r["clip_pct"] <= args.max_clip_pct else "  (reserva: ceifa acima do limite)"
        print(f"{i:>2}  sub{r['subject']}    col{r['column']} {r['channel']:<6}  {r['start_s']:5.2f} s  "
              f"{r['snr_db']:5.1f}  {r['err_db']:5.2f}   {r['bursts']:>3}     {r['clip_pct']:.2f} %{tag}")
    b = best[0]
    print(f"\nescolha #1 -> make_stimulus.py --subject {b['subject']} --channel {b['column']} --start {b['start_s']:.2f}")
    print(f"grade em {OUT_DIR / 'candidates.png'}")


if __name__ == "__main__":
    main()
