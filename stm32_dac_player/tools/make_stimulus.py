"""Gera a tabela do DAC: o preambulo compartilhado + N trechos do dataset.

Cada laco tem 7,500 s exatos, com os tempos redondos de proposito - da para
cravar cursor no osciloscopio e para fatiar em script sem procurar borda:

    0,000 - 0,200 s  silencio (0 V)      o trem de pulsos de sync vive aqui
    0,200 - 0,230 s  3 ciclos de 100 Hz  marcador de alinhamento
    0,230 - 2,000 s  ZONA MORTA (0 V)    guarda antes da amostra, >= 1,5 s
    2,000 - 6,000 s  A AMOSTRA           4,000 s do dataset, normalizada a +-1,0
    6,000 - 7,500 s  ZONA MORTA (0 V)    guarda depois da amostra -> repete

Ou seja: a amostra abre exatamente 2,000 s depois do primeiro pulso de sync e
fecha exatamente em 6,000 s, com 1,5 s de zona morta de cada lado. O silencio e
o codigo de meia escala no DAC, que depois do capacitor de acoplamento vira 0 V
nos terminais - entao na gravacao dos dois sistemas o inicio e o fim da amostra
ficam entre dois trechos chatos e inconfundiveis de 0 V. A zona morta tambem da
o piso de ruido medido de cada braco, longe de qualquer transitorio.

Isto difere do TRES_ENSAIOS_SINAL 3, que punha tambem uma varredura de
20->500 Hz em 1,3-1,8 s como "verificacao rapida de banda". Ela saiu: era
redundante com o modo 'sweep' do firmware (degraus de 1/3 de oitava, 2 s por
frequencia), que e a medida de resposta em frequencia de verdade dos ensaios
E1 passo 6 e E3 passo 7. Sem ela o laco caiu de 6,5 s para 5,0 s e a amostra
passou de 62 % para 80 % do laco.

O trecho de 0,0 a 0,5 s e igual para todos os segmentos, entao vai para a flash
uma vez so (stimulus_preamble). Os 4 s do dataset vao num array por segmento
(stimulus_segments). Os 0,5 s de silencio do fim o firmware gera na hora - e so
o codigo de meia escala.

Cada trecho: tira a media, reamostra de 1 kS/s para 8 kS/s com
scipy.signal.resample_poly (FIR de anti-imagem, nada interpolado em tempo real),
aplica rampas de 10 ms nas bordas para nao haver degrau contra o silencio, e
normaliza o pico a 1,0 depois de reamostrar.

Quais trechos entram esta em stimulus/segments.json (ver pick_segment.py).

Saidas (nao editar a mao - regerar invalida a comparacao entre sessoes):
    common/stimulus_table.c / .h     int16 Q15, entra no firmware
    stimulus/stimulus.json           parametros, SHA-256 e CRC32 da tabela
    stimulus/reference.npz           preambulo e segmentos, para a analise
    stimulus/wav/<nome>.wav          um laco completo por segmento, 8 kHz 16 bits
    stimulus/stimulus_loop.png       figura do laco e dos segmentos

    python stm32_dac_player/tools/make_stimulus.py
    python stm32_dac_player/tools/make_stimulus.py --spec outro_spec.json
"""

from __future__ import annotations

import argparse
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

from emg_dataset import (ARTICLE_SNR_DB, CHANNELS, CLIP_UV, FS_SRC, ZIP_PATH, ZIP_SHA256, action_path,
                         article_metrics, channel_index, load_action, sha256_file)

ROOT = Path(__file__).resolve().parents[1]
COMMON = ROOT / "common"
OUT_DIR = ROOT / "stimulus"
DEFAULT_SPEC = OUT_DIR / "segments.json"

FS = 8000
LOOP_S = 7.5
PREAMBLE_S = 2.0          # silencio + marcador + zona morta de entrada
SEGMENT_S = 4.0           # a amostra, de 2,000 a 6,000 s
TAIL_S = 1.5              # zona morta de saida
MARKER_AT_S, MARKER_HZ, MARKER_CYCLES = 0.2, 100.0, 3
DEAD_ZONE_S = 1.5         # minimo de 0 V de cada lado da amostra
EDGE_TAPER_S = 0.010

PREAMBLE_LEN = int(round(PREAMBLE_S * FS))
SEGMENT_LEN = int(round(SEGMENT_S * FS))
TAIL_LEN = int(round(TAIL_S * FS))
MARKER_END_LEN = int(round(MARKER_AT_S * FS)) + int(round(MARKER_CYCLES * FS / MARKER_HZ))
assert PREAMBLE_LEN + SEGMENT_LEN + TAIL_LEN == int(round(LOOP_S * FS))
# a zona morta tem de existir dos dois lados: entre o fim do marcador e o inicio
# da amostra, e entre o fim da amostra e o fim do laco
assert PREAMBLE_LEN - MARKER_END_LEN >= int(round(DEAD_ZONE_S * FS)), "zona morta de entrada curta"
assert TAIL_LEN >= int(round(DEAD_ZONE_S * FS)), "zona morta de saida curta"


def cosine_taper(n: int, edge: int) -> np.ndarray:
    w = np.ones(n)
    if edge > 0:
        ramp = 0.5 - 0.5 * np.cos(np.pi * np.arange(edge) / edge)
        w[:edge] = ramp
        w[-edge:] = ramp[::-1]
    return w


def build_preamble() -> tuple[np.ndarray, dict]:
    """Silencio, marcador de 100 Hz, zona morta - 2,0 s."""
    y = np.zeros(PREAMBLE_LEN)
    i = int(MARKER_AT_S * FS)
    n = int(round(MARKER_CYCLES * FS / MARKER_HZ))
    y[i : i + n] = np.sin(2 * np.pi * MARKER_HZ * np.arange(n) / FS)
    return y, {"marker": [i, i + n]}


def build_segment(spec: dict) -> tuple[np.ndarray, dict]:
    """Um trecho de 4 s do dataset, reamostrado para FS e normalizado a +-1."""
    sub, action, group = spec["subject"], spec["action"], spec.get("group", "Aggressive")
    col = channel_index(spec["channel"])
    start_s, fs_src = float(spec["start_s"]), int(spec.get("fs_src", FS_SRC))

    d = load_action(sub, action, group)
    k0 = int(round(start_s * fs_src))
    k1 = k0 + int(round(SEGMENT_S * fs_src))
    if k1 > len(d):
        raise SystemExit(f"{action} sub{sub} tem {len(d) / fs_src:.2f} s; "
                         f"trecho {start_s}+{SEGMENT_S} s nao cabe")
    raw = d[k0:k1, col].astype(float)
    m = article_metrics(raw, fs_src)

    x = raw - raw.mean()
    ratio = Fraction(FS, fs_src).limit_denominator(1000)
    x = signal.resample_poly(x, ratio.numerator, ratio.denominator)
    x = x[:SEGMENT_LEN] * cosine_taper(SEGMENT_LEN, int(EDGE_TAPER_S * FS))
    x = x / np.max(np.abs(x))

    meta = {
        "name": spec["name"],
        "file": str(action_path(sub, action, group).relative_to(ZIP_PATH.parent)),
        "subject": sub, "group": group, "action": action,
        "column": col, "channel": CHANNELS[col],
        "start_s": start_s, "duration_s": SEGMENT_S, "fs_src": fs_src,
        "clip_pct": round(100.0 * float(np.mean(np.abs(raw) >= CLIP_UV - 1)), 3),
        "snr_db_article_method": round(m.snr_db, 2),
        "bursts": m.bursts,
        "peak_uv": round(float(np.max(np.abs(raw - raw.mean()))), 1),
    }
    if spec.get("note"):
        meta["note"] = spec["note"]
    return x, meta


def to_q15(y: np.ndarray) -> np.ndarray:
    return np.clip(np.round(y * 32767.0), -32768, 32767).astype("<i2")


def write_c(preamble: np.ndarray, segments: list[np.ndarray], metas: list[dict],
            sha: str, crc: int, idx: dict) -> None:
    n = len(segments)
    names = ", ".join(f'"{m["name"]}"' for m in metas)
    hdr = f"""/* GERADO por tools/make_stimulus.py - nao editar. Regerar invalida a comparacao entre sessoes. */
#ifndef STIMULUS_TABLE_H
#define STIMULUS_TABLE_H

#include <stdint.h>

#define STIMULUS_FS             {FS}u
#define STIMULUS_SHA256         "{sha}"
#define STIMULUS_CRC32          0x{crc:08x}u

/* Um laco = preambulo + um segmento + zona morta de saida = {LOOP_S} s exatos:
 *
 *   0,000 - 0,200 s  silencio (trem de pulsos de sync)
 *   0,200 - 0,230 s  marcador, 3 ciclos de 100 Hz
 *   0,230 - 2,000 s  zona morta, 0 V
 *   2,000 - 6,000 s  A AMOSTRA do dataset
 *   6,000 - 7,500 s  zona morta, 0 V -> repete
 *
 * A zona morta de saida o firmware gera na hora (e so o codigo de meia escala),
 * entao nao custa flash. */
#define STIMULUS_PREAMBLE_LEN   {PREAMBLE_LEN}u   /* {PREAMBLE_S} s */
#define STIMULUS_SEGMENT_LEN    {SEGMENT_LEN}u   /* {SEGMENT_S} s */
#define STIMULUS_TAIL_LEN       {TAIL_LEN}u   /* {TAIL_S} s */
#define STIMULUS_LOOP_LEN       {PREAMBLE_LEN + SEGMENT_LEN + TAIL_LEN}u
#define STIMULUS_SEGMENT_COUNT  {n}u

/* Indices no laco. A amostra vai de SEGMENT_START a SEGMENT_END. */
#define STIMULUS_MARKER_START   {idx['marker'][0]}u
#define STIMULUS_MARKER_END     {idx['marker'][1]}u
#define STIMULUS_SEGMENT_START  {PREAMBLE_LEN}u
#define STIMULUS_SEGMENT_END    {PREAMBLE_LEN + SEGMENT_LEN}u
#define STIMULUS_DEAD_ZONE_LEN  {int(round(DEAD_ZONE_S * FS))}u   /* {DEAD_ZONE_S} s de cada lado */

/* Q15: -32768..32767 = -1..+1 do pico; o firmware escala por amp e soma mid */
extern const int16_t stimulus_preamble[STIMULUS_PREAMBLE_LEN];
extern const int16_t stimulus_segments[STIMULUS_SEGMENT_COUNT][STIMULUS_SEGMENT_LEN];

/* Nomes na ordem dos segmentos: {names} */
extern const char *const stimulus_segment_names[STIMULUS_SEGMENT_COUNT];
/* "sub4 Aggressive/Sidekicking R-Thi t=4.90 s, SNR 10.7 dB" */
extern const char *const stimulus_segment_source[STIMULUS_SEGMENT_COUNT];

#endif /* STIMULUS_TABLE_H */
"""
    (COMMON / "stimulus_table.h").write_text(hdr, encoding="utf-8", newline="\n")

    def rows(q: np.ndarray, indent: str) -> str:
        return "\n".join(indent + ",".join(f"{v:6d}" for v in q[k : k + 16]) + ","
                         for k in range(0, len(q), 16))

    parts = [f"/* GERADO por tools/make_stimulus.py - nao editar. SHA-256 {sha} */",
             '#include "stimulus_table.h"', "",
             "const int16_t stimulus_preamble[STIMULUS_PREAMBLE_LEN] = {",
             rows(to_q15(preamble), "    "), "};", ""]

    parts.append("const int16_t stimulus_segments[STIMULUS_SEGMENT_COUNT][STIMULUS_SEGMENT_LEN] = {")
    for seg, m in zip(segments, metas):
        parts.append(f"    /* {m['name']}: {m['source_str']} */")
        parts.append("    {")
        parts.append(rows(to_q15(seg), "        "))
        parts.append("    },")
    parts += ["};", ""]

    parts.append("const char *const stimulus_segment_names[STIMULUS_SEGMENT_COUNT] = {")
    parts += [f'    "{m["name"]}",' for m in metas]
    parts += ["};", ""]
    parts.append("const char *const stimulus_segment_source[STIMULUS_SEGMENT_COUNT] = {")
    parts += [f'    "{m["source_str"]}",' for m in metas]
    parts += ["};", ""]

    (COMMON / "stimulus_table.c").write_text("\n".join(parts), encoding="utf-8", newline="\n")


def write_refs(preamble: np.ndarray, segments: list[np.ndarray], metas: list[dict], meta: dict) -> None:
    OUT_DIR.mkdir(exist_ok=True)
    pq = to_q15(preamble)
    tail = np.zeros(TAIL_LEN, dtype="<i2")

    np.savez_compressed(OUT_DIR / "reference.npz", fs=FS, preamble=pq, tail=tail,
                        names=np.array([m["name"] for m in metas]),
                        segments=np.stack([to_q15(s) for s in segments]))

    wav_dir = OUT_DIR / "wav"
    wav_dir.mkdir(exist_ok=True)
    for seg, m in zip(segments, metas):
        loop = np.concatenate([pq, to_q15(seg), tail])
        with wave.open(str(wav_dir / f"{m['name']}.wav"), "wb") as wv:
            wv.setnchannels(1)
            wv.setsampwidth(2)
            wv.setframerate(FS)
            wv.writeframes(loop.tobytes())

    (OUT_DIR / "stimulus.json").write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n",
                                           encoding="utf-8")

    n = len(segments)
    fig, axes = plt.subplots(n + 1, 1, figsize=(11, 1.5 * (n + 1)), sharex=True)
    t = np.arange(PREAMBLE_LEN + SEGMENT_LEN + TAIL_LEN) / FS
    axes[0].plot(t[:PREAMBLE_LEN], preamble, lw=0.4, color="0.25")
    axes[0].set_title(f"preambulo compartilhado: silencio, marcador de 100 Hz em "
                      f"{MARKER_AT_S:.3f} s, zona morta ate {PREAMBLE_S:.3f} s",
                      fontsize=9, loc="left")
    axes[0].set_ylim(-1.15, 1.15)
    for ax in axes:
        # as zonas mortas, iguais em todos os paineis
        ax.axvspan(meta["loop_layout_samples"]["marker"][1] / FS, PREAMBLE_S, color="#fde8d0", zorder=0)
        ax.axvspan(PREAMBLE_S + SEGMENT_S, LOOP_S, color="#fde8d0", zorder=0)
        ax.axvline(PREAMBLE_S, color="#c0392b", lw=0.8, ls="--", zorder=1)
        ax.axvline(PREAMBLE_S + SEGMENT_S, color="#c0392b", lw=0.8, ls="--", zorder=1)
    axes[0].text(PREAMBLE_S / 2 + 0.2, -0.95, "zona morta", fontsize=7, color="#8a5a2b", ha="center")
    axes[0].text(PREAMBLE_S + SEGMENT_S + TAIL_S / 2, -0.95, "zona morta", fontsize=7,
                 color="#8a5a2b", ha="center")
    for ax, seg, m in zip(axes[1:], segments, metas):
        ts = (PREAMBLE_LEN + np.arange(SEGMENT_LEN)) / FS
        ax.plot(ts, seg, lw=0.4)
        ax.set_ylim(-1.15, 1.15)
        ax.set_title(f"{m['name']}  ({m['group']}, sub{m['subject']} {m['channel']}, "
                     f"SNR {m['snr_db_article_method']:.1f} dB)", fontsize=9, loc="left")
    axes[-1].set_xlabel("tempo no laco (s)")
    axes[-1].set_xlim(0, LOOP_S)
    fig.suptitle(f"Tabela de estimulo: {n} segmentos, laco de {LOOP_S} s @ {FS} S/s  -  "
                 f"a amostra vai de {PREAMBLE_S:.3f} s a {PREAMBLE_S + SEGMENT_S:.3f} s, "
                 f"com {TAIL_S:.1f} s de zona morta de cada lado", fontsize=10)
    fig.tight_layout()
    fig.savefig(OUT_DIR / "stimulus_loop.png", dpi=120)
    plt.close(fig)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--spec", type=Path, default=DEFAULT_SPEC, help="JSON com os segmentos (default: stimulus/segments.json)")
    args = ap.parse_args()

    spec = json.loads(args.spec.read_text(encoding="utf-8"))
    specs = spec["segments"]
    if not specs:
        raise SystemExit("o spec nao tem segmento nenhum")
    if len(specs) > 255:
        raise SystemExit("maximo de 255 segmentos (o indice do sync e um byte)")

    preamble, idx = build_preamble()
    segments, metas = [], []
    for s in specs:
        seg, m = build_segment(s)
        m["source_str"] = (f"sub{m['subject']} {m['group']}/{m['action']} {m['channel']} "
                           f"t={m['start_s']:.2f} s, SNR {m['snr_db_article_method']:.1f} dB")
        segments.append(seg)
        metas.append(m)

    # o CRC cobre a mesma sequencia de bytes que o firmware recalcula no boot:
    # preambulo seguido dos segmentos em ordem
    blob = to_q15(preamble).tobytes() + b"".join(to_q15(s).tobytes() for s in segments)
    sha = hashlib.sha256(blob).hexdigest()
    crc = zlib.crc32(blob) & 0xFFFFFFFF

    meta = {
        "table": {
            "fs": FS, "loop_s": LOOP_S, "format": "int16 Q15 little-endian",
            "preamble_len": PREAMBLE_LEN, "segment_len": SEGMENT_LEN, "tail_len": TAIL_LEN,
            "segment_count": len(segments),
            "flash_bytes": 2 * (PREAMBLE_LEN + SEGMENT_LEN * len(segments)),
            "sha256": sha, "crc32": f"0x{crc:08x}",
        },
        "loop_layout_samples": {
            "preamble": [0, PREAMBLE_LEN], "marker": idx["marker"],
            "segment": [PREAMBLE_LEN, PREAMBLE_LEN + SEGMENT_LEN],
            "tail": [PREAMBLE_LEN + SEGMENT_LEN, PREAMBLE_LEN + SEGMENT_LEN + TAIL_LEN],
        },
        "marker": {"at_s": MARKER_AT_S, "hz": MARKER_HZ, "cycles": MARKER_CYCLES},
        "dead_zone_s": DEAD_ZONE_S,
        "sync": "o canal 2 do DAC (PA5) emite (indice+1) pulsos de 10 ms no inicio de cada laco",
        "dataset": {
            "name": "UCI EMG Physical Action Data Set (Theodoridis 2011, DOI 10.24432/C53W49)",
            "zip_sha256": sha256_file(ZIP_PATH) if ZIP_PATH.exists() else None,
            "zip_sha256_expected": ZIP_SHA256,
            "article_snr_db_original": ARTICLE_SNR_DB,
        },
        "processing": f"media removida; resample_poly {FS_SRC}->{FS}; rampas cos de "
                      f"{EDGE_TAPER_S * 1000:.0f} ms; pico normalizado a 1,0 depois de reamostrar",
        "segments": metas,
        "spec": str(args.spec.relative_to(ROOT)),
    }

    write_c(preamble, segments, metas, sha, crc, idx)
    write_refs(preamble, segments, metas, meta)

    print(f"{len(segments)} segmentos, preambulo de {PREAMBLE_LEN} amostras:")
    print(f"{'#':>2}  {'nome':<13} {'grupo':<11} {'origem':<26} {'SNR':>6} {'raj':>4} {'clip%':>6}")
    for i, m in enumerate(metas):
        print(f"{i:>2}  {m['name']:<13} {m['group']:<11} "
              f"sub{m['subject']} {m['channel']:<6} t={m['start_s']:5.2f} s "
              f"{m['snr_db_article_method']:6.1f} {m['bursts']:4d} {m['clip_pct']:6.2f}")
    kb = meta["table"]["flash_bytes"] / 1024
    print(f"\nflash da tabela: {kb:.1f} kB  ({PREAMBLE_LEN} + {len(segments)} x {SEGMENT_LEN} amostras)")
    print(f"sha256  {sha}\ncrc32   0x{crc:08x}")
    print(f"-> {COMMON / 'stimulus_table.c'}\n-> {OUT_DIR}")


if __name__ == "__main__":
    main()
