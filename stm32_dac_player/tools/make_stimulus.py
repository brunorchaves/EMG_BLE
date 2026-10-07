"""Gera a tabela do DAC: o laco de 6,5 s do TRES_ENSAIOS_SINAL 3.

    0,0 - 0,5 s  silencio            piso de ruido de cada braco
    0,5 s        3 ciclos de 100 Hz  marcador de alinhamento (30 ms; resto ate 0,8 s em silencio)
    0,8 - 1,3 s  silencio
    1,3 - 1,8 s  varredura 20->500 Hz (chirp linear, bordas de 5 ms)
    1,8 - 2,3 s  silencio
    2,3 - 6,3 s  trecho do dataset, normalizado a +-1,0
    6,3 - 6,5 s  silencio            -> repete

O trecho: tira a media, reamostra de 1 kS/s para 8 kS/s com
scipy.signal.resample_poly (FIR de anti-imagem, nada interpolado em tempo
real), aplica rampas de 10 ms nas bordas para nao haver degrau contra o
silencio, e normaliza o pico a 1,0 depois de reamostrar.

Saidas (nao editar a mao - regerar invalida a comparacao entre sessoes):
    common/stimulus_table.c / .h     int16 Q15, entra no firmware
    stimulus/stimulus_loop.csv       referencia amostra a amostra para a analise
    stimulus/stimulus_loop.wav       a mesma coisa, 8 kHz mono 16 bits
    stimulus/stimulus.json           parametros, SHA-256 e CRC32 da tabela, SHA do zip do dataset
    stimulus/stimulus_loop.png       figura do laco

    python stm32_dac_player/tools/make_stimulus.py            (padrao = escolha #1 do pick_segment.py)
    python stm32_dac_player/tools/make_stimulus.py --subject 3 --channel L-Thi --start 4.75
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import wave
import zlib
from fractions import Fraction
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from scipy import signal

from emg_dataset import (ARTICLE_SNR_DB, CHANNELS, CLIP_UV, FS_SRC, ZIP_PATH, ZIP_SHA256, action_path, article_metrics,
                         channel_index, load_action, sha256_file)

ROOT = Path(__file__).resolve().parents[1]
COMMON = ROOT / "common"
OUT_DIR = ROOT / "stimulus"

FS = 8000
LOOP_S = 6.5
MARKER_AT_S, MARKER_HZ, MARKER_CYCLES = 0.5, 100.0, 3
SWEEP_AT_S, SWEEP_S, SWEEP_F0, SWEEP_F1 = 1.3, 0.5, 20.0, 500.0
SEGMENT_AT_S, SEGMENT_S = 2.3, 4.0
EDGE_TAPER_S, SWEEP_TAPER_S = 0.010, 0.005


def cosine_taper(n: int, edge: int) -> np.ndarray:
    w = np.ones(n)
    if edge > 0:
        ramp = 0.5 - 0.5 * np.cos(np.pi * np.arange(edge) / edge)
        w[:edge] = ramp
        w[-edge:] = ramp[::-1]
    return w


def dataset_segment(subject: int, column: int, start_s: float, fs_src: int) -> tuple[np.ndarray, dict]:
    d = load_action(subject)
    k0 = int(round(start_s * fs_src))
    k1 = k0 + int(round(SEGMENT_S * fs_src))
    if k1 > len(d):
        raise SystemExit(f"o arquivo tem {len(d) / fs_src:.2f} s; trecho {start_s}+{SEGMENT_S} s nao cabe")
    raw = d[k0:k1, column].astype(float)
    m = article_metrics(raw, fs_src)
    stats = dict(clip_pct=round(100.0 * float(np.mean(np.abs(raw) >= CLIP_UV - 1)), 3),
                 snr_db_article_method=round(m.snr_db, 2), bursts=m.bursts,
                 peak_uv=float(np.max(np.abs(raw - raw.mean()))))
    x = raw - raw.mean()
    ratio = Fraction(FS, fs_src).limit_denominator(1000)
    x = signal.resample_poly(x, ratio.numerator, ratio.denominator)
    n = int(round(SEGMENT_S * FS))
    x = x[:n] * cosine_taper(n, int(EDGE_TAPER_S * FS))
    return x / np.max(np.abs(x)), stats


def build_loop(segment: np.ndarray) -> tuple[np.ndarray, dict]:
    y = np.zeros(int(round(LOOP_S * FS)))
    idx = {}

    i = int(MARKER_AT_S * FS)
    n = int(round(MARKER_CYCLES * FS / MARKER_HZ))
    y[i : i + n] = np.sin(2 * np.pi * MARKER_HZ * np.arange(n) / FS)
    idx["marker"] = [i, i + n]

    i = int(SWEEP_AT_S * FS)
    n = int(SWEEP_S * FS)
    t = np.arange(n) / FS
    y[i : i + n] = signal.chirp(t, SWEEP_F0, SWEEP_S, SWEEP_F1, method="linear", phi=-90) * cosine_taper(
        n, int(SWEEP_TAPER_S * FS))
    idx["sweep"] = [i, i + n]

    i = int(SEGMENT_AT_S * FS)
    y[i : i + len(segment)] = segment
    idx["segment"] = [i, i + len(segment)]
    return y, idx


def to_q15(y: np.ndarray) -> np.ndarray:
    return np.clip(np.round(y * 32767.0), -32768, 32767).astype("<i2")


def write_c(q: np.ndarray, sha: str, crc: int, source: str, idx: dict) -> None:
    hdr = f"""/* GERADO por tools/make_stimulus.py - nao editar. Regerar invalida a comparacao entre sessoes. */
#ifndef STIMULUS_TABLE_H
#define STIMULUS_TABLE_H

#include <stdint.h>

#define STIMULUS_LEN    {len(q)}u
#define STIMULUS_FS     {FS}u
#define STIMULUS_SHA256 "{sha}"
#define STIMULUS_CRC32  0x{crc:08x}u
#define STIMULUS_SOURCE "{source}"

/* Indices [inicio, fim) de cada trecho do laco */
#define STIMULUS_MARKER_START   {idx['marker'][0]}u
#define STIMULUS_SWEEP_START    {idx['sweep'][0]}u
#define STIMULUS_SEGMENT_START  {idx['segment'][0]}u
#define STIMULUS_SEGMENT_END    {idx['segment'][1]}u

/* Q15: -32768..32767 = -1..+1 do pico; o firmware escala por amp e soma mid */
extern const int16_t stimulus_table[STIMULUS_LEN];

#endif /* STIMULUS_TABLE_H */
"""
    (COMMON / "stimulus_table.h").write_text(hdr, encoding="utf-8", newline="\n")
    lines = []
    for k in range(0, len(q), 16):
        lines.append("    " + ",".join(f"{v:6d}" for v in q[k : k + 16]) + ",")
    body = (f"/* GERADO por tools/make_stimulus.py - nao editar. SHA-256 {sha} */\n"
            '#include "stimulus_table.h"\n\n'
            "const int16_t stimulus_table[STIMULUS_LEN] = {\n" + "\n".join(lines) + "\n};\n")
    (COMMON / "stimulus_table.c").write_text(body, encoding="utf-8", newline="\n")


def write_refs(q: np.ndarray, idx: dict, meta: dict) -> None:
    OUT_DIR.mkdir(exist_ok=True)
    with open(OUT_DIR / "stimulus_loop.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["index", "t_s", "q15", "norm"])
        for k, v in enumerate(q):
            w.writerow([k, f"{k / FS:.6f}", int(v), f"{v / 32767.0:.6f}"])
    with wave.open(str(OUT_DIR / "stimulus_loop.wav"), "wb") as wv:
        wv.setnchannels(1)
        wv.setsampwidth(2)
        wv.setframerate(FS)
        wv.writeframes(q.tobytes())
    (OUT_DIR / "stimulus.json").write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    t = np.arange(len(q)) / FS
    y = q / 32767.0
    fig, ax = plt.subplots(2, 1, figsize=(11, 5), gridspec_kw=dict(height_ratios=[1.3, 1]))
    ax[0].plot(t, y, lw=0.4)
    for name, (a, b) in idx.items():
        ax[0].axvspan(a / FS, b / FS, color="0.9", zorder=0)
        ax[0].text((a + b) / 2 / FS, 1.05, name, ha="center", fontsize=8)
    ax[0].set_xlim(0, LOOP_S)
    ax[0].set_ylim(-1.15, 1.2)
    ax[0].set_title(f"Laco de estimulo, {LOOP_S} s @ {FS} S/s  -  {meta['source']['description']}", fontsize=9)
    a, b = idx["marker"]
    ax[1].plot(t[a - 80 : b + 80] * 1000, y[a - 80 : b + 80], lw=0.8)
    ax[1].set_xlabel("tempo (ms) - marcador de alinhamento")
    fig.tight_layout()
    fig.savefig(OUT_DIR / "stimulus_loop.png", dpi=120)
    plt.close(fig)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--subject", type=int, default=4)
    ap.add_argument("--channel", default="4", help="coluna 0-7 ou nome (R-Thi, R-Ham, L-Thi, L-Ham, ...)")
    ap.add_argument("--start", type=float, default=4.90, help="inicio do trecho no arquivo, s")
    ap.add_argument("--fs-src", type=int, default=FS_SRC, help="taxa do dataset (verificada: 1000 S/s)")
    ap.add_argument("--action", default="Sidekicking")
    args = ap.parse_args()

    col = channel_index(args.channel)
    seg, stats = dataset_segment(args.subject, col, args.start, args.fs_src)
    y, idx = build_loop(seg)
    q = to_q15(y)
    sha = hashlib.sha256(q.tobytes()).hexdigest()
    crc = zlib.crc32(q.tobytes()) & 0xFFFFFFFF
    desc = (f"UCI EMG Physical Action, sub{args.subject} Aggressive/{args.action} col{col} {CHANNELS[col]}, "
            f"t={args.start:.2f}..{args.start + SEGMENT_S:.2f} s")

    zip_sha = sha256_file(ZIP_PATH) if ZIP_PATH.exists() else None
    meta = {
        "table": {"len": len(q), "fs": FS, "loop_s": LOOP_S, "format": "int16 Q15 little-endian",
                  "sha256": sha, "crc32": f"0x{crc:08x}"},
        "layout_samples": idx,
        "marker": {"at_s": MARKER_AT_S, "hz": MARKER_HZ, "cycles": MARKER_CYCLES},
        "sweep": {"at_s": SWEEP_AT_S, "duration_s": SWEEP_S, "f0_hz": SWEEP_F0, "f1_hz": SWEEP_F1, "method": "linear"},
        "source": {
            "dataset": "UCI EMG Physical Action Data Set (Theodoridis 2011, DOI 10.24432/C53W49)",
            "zip_sha256": zip_sha, "zip_sha256_expected": ZIP_SHA256,
            "file": str(action_path(args.subject, args.action).relative_to(ZIP_PATH.parent)),
            "subject": args.subject, "column": col, "channel": CHANNELS[col],
            "start_s": args.start, "duration_s": SEGMENT_S, "fs_src": args.fs_src,
            "description": desc, **stats,
            "article_snr_db_original": ARTICLE_SNR_DB,
        },
        "processing": f"media removida; resample_poly {args.fs_src}->{FS}; rampas cos de {EDGE_TAPER_S * 1000:.0f} ms; "
                      "pico normalizado a 1,0 depois de reamostrar",
    }
    write_c(q, sha, crc, desc, idx)
    write_refs(q, idx, meta)
    print(desc)
    print(f"trecho: SNR (metodo do artigo) {stats['snr_db_article_method']} dB, {stats['bursts']} rajadas, "
          f"{stats['clip_pct']} % ceifado no original")
    print(f"tabela: {len(q)} amostras, {2 * len(q) / 1024:.1f} kB")
    print(f"sha256  {sha}\ncrc32   0x{crc:08x}")
    print(f"-> {COMMON / 'stimulus_table.c'}\n-> {OUT_DIR}")


if __name__ == "__main__":
    main()
