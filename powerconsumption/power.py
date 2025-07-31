#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Current‑versus‑time analysis from an oscilloscope CSV.

Destaca quatro modos (IDLE, CONNECTED, TRANSMITTING, OFF) e
calcula a corrente RMS e a potência (5 V) de cada janela.

Operating windows
-----------------
IDLE           :  2.88 – 14.0 s   (publicando advertising, sem conexão)
CONNECTED      : 14.0 – 26.4 s   (associado, sem streaming)
TRANSMITTING   : 26.4 – 39.8 s   (notificações EMG ativas)
SYSTEM OFF     :  0 – 2.88 s  e  ≥ 39.8 s
"""

from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

# --------------------------- TIMING WINDOWS -------------------------------- #
OFF_WINDOWS          = [(0.0, 2.88), (39.8, np.inf)]
IDLE_WINDOWS         = [(2.88, 14.0)]
CONNECTED_WINDOWS    = [(14.0, 26.4)]
TRANSMITTING_WINDOWS = [(26.4, 39.8)]

COLORS = {
    "IDLE":       "#c8d9ff",
    "CONNECTED":  "#ffd28d",
    "TRANSMIT":   "#ffb3b3",
    "OFF":        "#d0d0d0",
    "TRACE":      "tab:blue",
}

# --------------------------------------------------------------------------- #
def plot_current_from_csv(
    csv_path: str | Path,
    timestamp_col: int | None = None,
    signal_col: int | None = None,
    gain: float = 10.0,            # ganho do shunt/op‑amp
    v_supply: float = 5.0,         # tensão de alimentação
    min_valid_ratio: float = 0.5,
    time_label: str = "Time (s)",
    current_label: str = "Current (mA)",
) -> None:
    # ---------------- CSV → numpy ------------------------------------------ #
    df = pd.read_csv(csv_path, header=None)
    numeric_df = df.apply(pd.to_numeric, errors="coerce")

    if timestamp_col is None or signal_col is None:
        threshold = len(df) * min_valid_ratio
        valid = numeric_df.columns[numeric_df.notna().sum() > threshold]
        if len(valid) < 2:
            raise ValueError("Could not find two sufficiently numeric columns.")
        timestamp_col, signal_col = valid[:2]

    time  = numeric_df[timestamp_col].dropna().to_numpy()
    volts = numeric_df[signal_col].dropna().to_numpy()
    current_mA = (volts / gain) * 1e3

    # ---------------- helpers ---------------------------------------------- #
    rms  = lambda arr: float(np.sqrt(np.mean(arr**2))) if arr.size else float("nan")
    mask = lambda t0, t1: (time >= t0) & (time <= (time[-1] if np.isinf(t1) else t1))

    # -------------------------- REPORT -------------------------------------- #
    print(f"Supply voltage: {v_supply:.2f} V\n")

    def report(label, windows):
        for i, (t0, t1) in enumerate(windows, 1):
            rng = f"{t0:.2f}–{t1 if np.isinf(t1) else f'{t1:.2f}'}"
            i_rms = rms(current_mA[mask(t0, t1)])
            print(
                f"{label:<12}#{i:<2} ({rng}s): "
                f"RMS {i_rms:6.3f} mA  →  {i_rms * v_supply:7.2f} mW"
            )

    report("IDLE",          IDLE_WINDOWS)
    report("CONNECTED",     CONNECTED_WINDOWS)
    report("TRANSMITTING",  TRANSMITTING_WINDOWS)
    report("OFF",           OFF_WINDOWS)

    # -------------------------- PLOT ---------------------------------------- #
    fig, ax = plt.subplots(figsize=(11, 4))
    ax.plot(time, current_mA, color=COLORS["TRACE"], lw=1)

    def shade(wins, key, alpha=0.4):
        for t0, t1 in wins:
            ax.axvspan(
                t0,
                t1 if not np.isinf(t1) else time[-1],
                facecolor=COLORS[key],
                alpha=alpha,
            )

    shade(IDLE_WINDOWS,         "IDLE")
    shade(CONNECTED_WINDOWS,    "CONNECTED")
    shade(TRANSMITTING_WINDOWS, "TRANSMIT")
    shade(OFF_WINDOWS,          "OFF", alpha=0.35)

    ax.legend(
        handles=[
            plt.Line2D([], [], color=COLORS["IDLE"],      lw=8, alpha=0.5, label="Idle"),
            plt.Line2D([], [], color=COLORS["CONNECTED"], lw=8, alpha=0.5, label="Connected"),
            plt.Line2D([], [], color=COLORS["TRANSMIT"],  lw=8, alpha=0.5, label="Transmitting"),
            plt.Line2D([], [], color=COLORS["OFF"],       lw=8, alpha=0.5, label="System OFF"),
            plt.Line2D([], [], color=COLORS["TRACE"],     lw=2,             label="Current"),
        ],
        loc="lower right",
        framealpha=0.95,
    )

    ax.set_xlabel(time_label)
    ax.set_ylabel(current_label)
    ax.grid(True)
    plt.tight_layout()
    plt.show()

# --------------------------------------------------------------------------- #
if __name__ == "__main__":
    plot_current_from_csv("F0010CH1.CSV")
