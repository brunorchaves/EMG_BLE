"""Compatibilidade com o formato de saída antigo (ppk2_capture.py):
results/<estado>_raw_uA.csv + resumo_tabela2.csv, no mesmo formato exato de
antes, para quem já tinha scripts/planilhas em cima dele.

Opt-in (não roda automaticamente) porque um traço de 30s a 100 kS/s em CSV
vira ~40 MB por estado - decima por padrão, com o fator gravado num
comentário no header.
"""

from __future__ import annotations

import csv
import json
from pathlib import Path

import numpy as np

import config
from ppk2_decode import decode_words, load_raw
from timeline import EventLog, build_bands, fit_stream_clock


def write_legacy_outputs(run_dir: Path, out_dir: Path | None = None, decimate: int = 100) -> list[Path]:
    run_dir = Path(run_dir)
    out_dir = Path(out_dir) if out_dir else run_dir / "legacy"
    out_dir.mkdir(parents=True, exist_ok=True)

    meta = json.loads((run_dir / "run_meta.json").read_text(encoding="utf-8"))
    calib = meta["ppk2_calibration"]
    source_v = meta["stream_config"]["source_mv"] / 1000.0

    words = load_raw(run_dir / "current_raw.u32")
    block = decode_words(words.astype(np.uint32), calib, source_v)

    chunks = np.load(run_dir / "chunks.npy")
    fit = fit_stream_clock(chunks, fs_nominal=config.PPK2_FS_NOMINAL)
    log = EventLog.from_jsonl(run_dir / "events.jsonl")
    bands = build_bands(log, [s.value for s in config.State], fit)

    written = []
    summary_rows = []
    for legacy_state, canonical in config.LEGACY_STATE_MAP.items():
        matching = [b for b in bands if b.state == legacy_state]
        if not matching:
            continue
        b = matching[0]
        lo, hi = b.guarded_indices(fit.fs_effective)
        seg = block.current_uA[max(0, lo) : min(len(block.current_uA), hi)]
        if len(seg) == 0:
            continue

        decimated = seg[::decimate]
        csv_path = out_dir / f"{canonical}_raw_uA.csv"
        with open(csv_path, "w", newline="", encoding="utf-8") as f:
            f.write(f"# decimado {decimate}x a partir de {run_dir.name}/current_raw.u32\n")
            writer = csv.writer(f)
            writer.writerow(["amostra", "corrente_uA"])
            for i, v in enumerate(decimated):
                writer.writerow([i, float(v)])
        written.append(csv_path)

        rms_mA = float(np.sqrt(np.mean(seg.astype(np.float64) ** 2))) / 1000.0
        power_mW = rms_mA * source_v
        summary_rows.append(
            {"estado": canonical, "corrente_mA": round(rms_mA, 4), "potencia_mW": round(power_mW, 4), "n_amostras": len(seg)}
        )

    summary_path = out_dir / "resumo_tabela2.csv"
    with open(summary_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["estado", "corrente_mA", "potencia_mW", "n_amostras"])
        writer.writeheader()
        writer.writerows(summary_rows)
    written.append(summary_path)

    return written


if __name__ == "__main__":
    import sys

    paths = write_legacy_outputs(Path(sys.argv[1]))
    for p in paths:
        print(p)
