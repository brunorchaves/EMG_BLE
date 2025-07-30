#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Current‑versus‑time analysis from an oscilloscope CSV.

Highlights three operating modes, computes RMS current and power
for each window.

Operating windows
-----------------
BLE active : (18.6 – 36 s) and (61 – 79 s)
System OFF : (0 – 6 s), (40 – 47 s), (≥ 81 s)
IDLE       : gaps between the windows above
"""

from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

BLE_WINDOWS = [(18.6, 36.0), (61.0, 79.0)]
OFF_WINDOWS = [(0.0, 6.0), (40.0, 47.0), (81.0, np.inf)]

COLORS = {
    "BLE":  "#ffd28d",
    "OFF":  "#d0d0d0",
    "IDLE": "#c8d9ff",
    "TRACE": "tab:blue",
}

def plot_current_from_csv(
    csv_path: str | Path,
    timestamp_col: int | None = None,
    signal_col: int | None = None,
    gain: float = 32.0,
    v_supply: float = 5.0,        # <-- default changed to 5 V
    min_valid_ratio: float = 0.5,
    time_label: str = "Time (s)",
    current_label: str = "Current (mA)",
) -> None:
    # --- CSV → numeric -------------------------------------------------------
    df = pd.read_csv(csv_path, header=None)
    numeric_df = df.apply(pd.to_numeric, errors="coerce")

    # --- detect numeric columns ---------------------------------------------
    if timestamp_col is None or signal_col is None:
        threshold = len(df) * min_valid_ratio
        valid_cols = numeric_df.columns[numeric_df.notna().sum() > threshold]
        if len(valid_cols) < 2:
            raise ValueError("Could not find two sufficiently numeric columns.")
        timestamp_col, signal_col = valid_cols[:2]

    time  = numeric_df[timestamp_col].dropna().to_numpy()
    volts = numeric_df[signal_col].dropna().to_numpy()

    # --- V → mA --------------------------------------------------------------
    current_mA = (volts / gain) * 1e3

    # --- helpers -------------------------------------------------------------
    rms = lambda arr: float(np.sqrt(np.mean(arr**2))) if arr.size else float("nan")
    win_mask = lambda t0, t1: (time >= t0) & (
        time <= (time[-1] if np.isinf(t1) else t1)
    )

    # --- RMS / power for BLE and OFF ----------------------------------------
    print(f"Supply voltage: {v_supply:.2f} V\n")

    for i, (t0, t1) in enumerate(BLE_WINDOWS, 1):
        i_rms = rms(current_mA[win_mask(t0, t1)])
        print(
            f"BLE active #{i}  ({t0:.1f}–{t1:.1f}s): "
            f"RMS {i_rms:.3f} mA  →  {i_rms * v_supply:.2f} mW"
        )

    for i, (t0, t1) in enumerate(OFF_WINDOWS, 1):
        rng = f"{t0:.1f}–{t1 if t1 != np.inf else '∞'}"
        i_rms = rms(current_mA[win_mask(t0, t1)])
        print(
            f"OFF #{i}       ({rng}s): "
            f"RMS {i_rms:.3f} mA  →  {i_rms * v_supply:.2f} mW"
        )

    # --- derive IDLE windows -------------------------------------------------
    all_win = sorted(
        [(a, b if not np.isinf(b) else time[-1]) for a, b in BLE_WINDOWS + OFF_WINDOWS],
        key=lambda w: w[0],
    )

    idle_windows, cursor = [], time[0]
    for a, b in all_win:
        if a > cursor:
            idle_windows.append((cursor, a))
        cursor = max(cursor, b)
    if cursor < time[-1]:
        idle_windows.append((cursor, time[-1]))

    idle_mask_tot = np.zeros_like(time, dtype=bool)
    print()
    for i, (t0, t1) in enumerate(idle_windows, 1):
        m = win_mask(t0, t1)
        idle_mask_tot |= m
        i_rms = rms(current_mA[m])
        print(
            f"IDLE #{i}      ({t0:.1f}–{t1:.1f}s): "
            f"RMS {i_rms:.3f} mA  →  {i_rms * v_supply:.2f} mW"
        )

    i_rms_idle_aggr = rms(current_mA[idle_mask_tot])
    print(
        f"\nIDLE (aggregate): RMS {i_rms_idle_aggr:.3f} mA  →  "
        f"{i_rms_idle_aggr * v_supply:.2f} mW"
    )

    # --- plotting ------------------------------------------------------------
    fig, ax = plt.subplots(figsize=(11, 4))
    ax.plot(time, current_mA, color=COLORS["TRACE"], lw=1)

    for t0, t1 in BLE_WINDOWS:
        ax.axvspan(t0, t1, facecolor=COLORS["BLE"], alpha=0.4)
    for t0, t1 in OFF_WINDOWS:
        ax.axvspan(
            t0,
            t1 if not np.isinf(t1) else time[-1],
            facecolor=COLORS["OFF"],
            alpha=0.4,
        )
    for t0, t1 in idle_windows:
        ax.axvspan(t0, t1, facecolor=COLORS["IDLE"], alpha=0.25)

    legend_lines = [
        plt.Line2D([], [], color=COLORS["BLE"],  lw=8, alpha=0.5, label="BLE active"),
        plt.Line2D([], [], color=COLORS["OFF"],  lw=8, alpha=0.5, label="System OFF"),
        plt.Line2D([], [], color=COLORS["IDLE"], lw=8, alpha=0.5, label="IDLE"),
        plt.Line2D([], [], color=COLORS["TRACE"], lw=2,            label="Current"),
    ]
    ax.legend(handles=legend_lines, loc="lower right", framealpha=0.95)

    ax.set_xlabel(time_label)
    ax.set_ylabel(current_label)
    ax.grid(True)
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    plot_current_from_csv("F0009CH1.CSV", gain=32.0, v_supply=5.0)
