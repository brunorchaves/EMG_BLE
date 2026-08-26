"""Varredura de tensao de alimentacao: mede corrente e potencia a cada tensao.

Responde empiricamente "vale a pena alimentar a 3,3 V em vez de 5 V?" em vez
de estimar. A PPK2 em source meter gera de 0,8 a 5,0 V, entao da para medir a
placa real em cada tensao sem tocar no hardware.

Duas grandezas importam e nao sao a mesma coisa:
  - CORRENTE: o que a bateria entrega (via boost) e o que dimensiona picos.
  - POTENCIA: o que dimensiona autonomia e supercapacitor (energia).
Reduzir a tensao pode reduzir a potencia e AUMENTAR a corrente, entao as duas
sao reportadas.

A cada tensao o script confere, pelos contadores do firmware, que a placa
continua funcionando (loop vivo e ADC amostrando) - descer a tensao demais faz
o regulador do modulo entrar em dropout e o resultado deixa de ser valido.

Uso:
    python voltage_sweep.py --port COM8
    python voltage_sweep.py --port COM8 --voltages 5000,4500,4000,3600,3300
"""

from __future__ import annotations

import argparse
import csv
import json
import time
from pathlib import Path

import numpy as np

import fw_counters
from ppk2_decode import decode_words
from ppk2_stream import Ppk2Stream, StreamConfig

DEFAULT_VOLTAGES = [5000, 4700, 4300, 4000, 3600, 3300, 3000]


def measure_at(stream: Ppk2Stream, calib: dict, mv: int, settle_s: float, dwell_s: float) -> dict:
    stream.dut_power(False)
    time.sleep(0.4)
    stream.set_source_mv(mv)
    stream.dut_power(True)
    time.sleep(settle_s)  # espera boot + estabilizacao

    # confirma que o firmware esta vivo NESTA tensao antes de medir
    alive = None
    fs_acq = None
    try:
        s1 = fw_counters.snapshot()
        time.sleep(2.0)
        s2 = fw_counters.snapshot()
        d_loop = s2.get("g_loop_count", 0) - s1.get("g_loop_count", 0)
        fs_acq = (s2.get("g_adc_ok_count", 0) - s1.get("g_adc_ok_count", 0)) / 2.0
        alive = d_loop > 0
    except Exception:
        alive = None  # sem J-Link nao da para confirmar; segue e marca como desconhecido

    stream.start()
    time.sleep(dwell_s)
    stream.stop()

    raw = np.fromfile(stream.out_dir / "current_raw.u32", dtype="<u4")
    blk = decode_words(raw, calib, mv / 1000.0)
    uA = blk.current_uA.astype(np.float64)

    # exclui vizinhanca de troca de faixa (artefato do instrumento)
    switches = np.flatnonzero(np.diff(blk.range_idx.astype(np.int16)) != 0) + 1
    mask = np.zeros(len(uA), dtype=bool)
    for off in range(-3, 4):
        idx = switches + off
        mask[idx[(idx >= 0) & (idx < len(uA))]] = True
    clean = uA[~mask]

    mean_mA = float(uA.mean()) / 1000.0
    return {
        "mv": mv,
        "firmware_alive": alive,
        "fs_acquisition_sps": fs_acq,
        "mean_mA": mean_mA,
        "rms_mA": float(np.sqrt(np.mean(uA**2))) / 1000.0,
        "mean_mW": mean_mA * mv / 1000.0,
        "p99_9_clean_mA": float(np.percentile(clean, 99.9)) / 1000.0 if len(clean) else float("nan"),
        "max_clean_mA": float(clean.max()) / 1000.0 if len(clean) else float("nan"),
        "n_samples": len(uA),
    }


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", default="COM8")
    ap.add_argument("--voltages", default=",".join(str(v) for v in DEFAULT_VOLTAGES))
    ap.add_argument("--settle", type=float, default=4.0)
    ap.add_argument("--dwell", type=float, default=6.0)
    ap.add_argument("--out", default="power_profiling/runs/voltage_sweep")
    args = ap.parse_args()

    voltages = [int(v) for v in args.voltages.split(",")]
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    stream = Ppk2Stream(StreamConfig(port=args.port, source_mv=voltages[0]), out_dir)
    calib = stream.open()

    rows = []
    try:
        for mv in voltages:
            r = measure_at(stream, calib, mv, args.settle, args.dwell)
            rows.append(r)
            flag = {True: "ok", False: "MORTO", None: "?"}[r["firmware_alive"]]
            print(
                f"{mv/1000:.2f} V  ->  {r['mean_mA']:6.3f} mA   {r['mean_mW']:6.2f} mW   "
                f"pico {r['max_clean_mA']:5.2f} mA   fw={flag}  "
                f"fs={r['fs_acquisition_sps'] or float('nan'):.0f} S/s",
                flush=True,
            )
    finally:
        stream.dut_power(False)
        stream.close()

    with (out_dir / "voltage_sweep.csv").open("w", newline="", encoding="utf-8") as f:
        wr = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        wr.writeheader()
        wr.writerows(rows)
    (out_dir / "voltage_sweep.json").write_text(json.dumps(rows, indent=2), encoding="utf-8")

    base = next((r for r in rows if r["mv"] == 5000), rows[0])
    print("\n=== relativo a 5,00 V ===")
    for r in rows:
        d_i = (r["mean_mA"] - base["mean_mA"]) / base["mean_mA"] * 100
        d_p = (r["mean_mW"] - base["mean_mW"]) / base["mean_mW"] * 100
        print(f"  {r['mv']/1000:.2f} V: corrente {d_i:+6.1f}%   potencia {d_p:+6.1f}%")


if __name__ == "__main__":
    main()
