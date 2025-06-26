#!/usr/bin/env python3
"""
Plota 4 sinais EMG com:
    • Traço bruto normalizado (-1 … 1)        (linha azul clara)
    • Envoltória retificada suavizada (0 … 1) (linha vermelha)

Requisitos: pandas numpy scipy matplotlib
"""

from pathlib import Path
import numpy as np
import pandas as pd
from scipy.signal import hilbert, butter, filtfilt
import matplotlib.pyplot as plt

# --------------------------------------------------------------------------- #
BASEDIR = Path(__file__).resolve().parent              # …/data/processedata
FILES = [
    "biceps_20s_2_clinical.csv",
    "thigh_20s_1_clinical.csv",
    "biceps_10s_4_proprietary.csv",
    "thigh_5s_2_1x_proprietary.csv",
]

# -------------------- PARÂMETROS DO FILTRO ---------------------------------- #
# fc = frequência de corte (Hz) da envoltória *depois* de normalizada.
# fs = taxa de amostragem (Hz) do sinal bruto. Ajuste se souber.
fs = 1000          # exemplo: 1000 amostras/s
fc = 5             # queremos só tendências lentas da envoltória
b, a = butter(4, fc / (fs / 2),  btype="low")   # 4ª ordem, Butterworth
# --------------------------------------------------------------------------- #

def load_signal(path: Path):
    df = pd.read_csv(path)
    if df.shape[1] >= 2:                       # [tempo, EMG]
        t = df.iloc[:, 0].to_numpy()
        x = df.iloc[:, 1].to_numpy()
    else:
        t = np.arange(len(df))
        x = df.iloc[:, 0].to_numpy()
    return t, x

def preprocess(x):
    x = x - np.mean(x)                         # remove offset
    vmax = np.max(np.abs(x))
    return x / vmax if vmax else x             # normaliza (-1 … 1)

def envelope_smooth(x_norm):
    rect = np.abs(x_norm)
    env  = np.abs(hilbert(rect))               # envoltória bruta
    env  = filtfilt(b, a, env)                 # suaviza (fase-zero)
    env /= np.max(env) or 1                    # normaliza (0 … 1)
    return env

# ------------------- PLOT ---------------------------------------------------- #
fig, axs = plt.subplots(2, 2, figsize=(12, 8), sharey=True)
axs = axs.ravel()

for ax, fname in zip(axs, FILES):
    path = BASEDIR / fname
    if not path.exists():
        ax.text(0.5, 0.5, "Arquivo\nnão encontrado", ha="center", va="center")
        ax.set_title(fname)
        continue

    t, raw = load_signal(path)
    x_norm = preprocess(raw)
    env    = envelope_smooth(x_norm)

    ax.plot(t, x_norm, color="#1f77b4", alpha=0.5, label="Raw norm.")
    ax.plot(t, env,    color="red",     lw=1.4,  label="Envelope smooth")
    ax.set_title(path.stem)
    ax.set_xlabel("Tempo / Amostra")
    ax.set_ylabel("Amplitude (norm.)")
    ax.legend(loc="upper right")

plt.tight_layout()
plt.show()
