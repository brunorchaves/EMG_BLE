#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D  # noqa: F401

# ------------------------- CONFIG ------------------------------------------ #
BASEDIR = Path(__file__).resolve().parent
FILES = {
    "biceps_clin":   "biceps_20s_2_clinical.csv",
    "thigh_clin":    "thigh_20s_1_clinical.csv",
    "biceps_prop":   "biceps_10s_4_proprietary.csv",
    "thigh_prop":    "thigh_5s_2_1x_proprietary.csv",
}

fs              = 2000        # Hz
NFFT            = 4096        # FFT comum
F_MAX           = 500         # Hz para visualização
DOWNSAMPLE_3D   = 2           # reduzir pontos no 3D
OUT_2D_PATH     = BASEDIR / "fft_norm_2d.png"
OUT_3D_PATH     = BASEDIR / "fft_norm_3d.png"

plt.rcParams.update({
    "font.family": "Times New Roman",
    "axes.titlesize": 16,
    "axes.titleweight": "bold",
    "axes.labelsize": 14,
    "axes.labelweight": "bold",
    "legend.fontsize": 12,
})

# ------------------------- FUNÇÕES ----------------------------------------- #
def load_signal_csv(path: Path):
    df = pd.read_csv(path)
    y = df.iloc[:, 1].to_numpy() if df.shape[1] >= 2 else df.iloc[:, 0].to_numpy()
    return y

def normalize_time_signal(x):
    x = x - np.mean(x)
    m = np.max(np.abs(x)) or 1
    return x / m

def fft_normalized(x_norm, nfft=NFFT, fs=fs):
    freqs = np.fft.rfftfreq(nfft, d=1/fs)
    amps  = np.abs(np.fft.rfft(x_norm, n=nfft))
    # normaliza espectro por seu próprio máximo
    amps /= (amps.max() or 1)
    return freqs, amps

def plot_fft_2d(freqs, amps_dict, f_max, out_path):
    fig, ax = plt.subplots(figsize=(10, 5))
    mask = freqs <= f_max
    for label, amps in amps_dict.items():
        ax.plot(freqs[mask], amps[mask], lw=1, label=label)
    ax.set_title("Normalized FFT (0–1)")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("|FFT| (norm.)")
    ax.grid(alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_path, dpi=300)
    print(f"2D FFT saved: {out_path}")

def plot_fft_3d(freqs, amps_mat, labels, f_max, downsample, out_path):
    mask = freqs <= f_max
    f_ds  = freqs[mask][::downsample]
    Z     = amps_mat[:, mask][:, ::downsample]  # (Nsinais, F)
    Y = np.arange(len(labels))
    F, Ygrid = np.meshgrid(f_ds, Y, indexing='xy')  # (Nsinais, F)

    fig = plt.figure(figsize=(11, 6))
    ax  = fig.add_subplot(111, projection='3d')
    surf = ax.plot_surface(F, Ygrid, Z, cmap='viridis', linewidth=0, antialiased=True)

    ax.set_title("Normalized FFT 3D")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Signal")
    ax.set_zlabel("|FFT| (norm.)")
    ax.set_yticks(Y)
    ax.set_yticklabels(labels)

    fig.colorbar(surf, shrink=0.6, pad=0.05, label="|FFT| (norm.)")
    fig.tight_layout()
    fig.savefig(out_path, dpi=300)
    print(f"3D FFT saved: {out_path}")

# ------------------------- MAIN -------------------------------------------- #
amps_dict   = {}
labels_plot = []
for key, fname in FILES.items():
    path = BASEDIR / fname
    if not path.exists():
        print(f"[WARN] File not found: {fname}")
        continue
    x = load_signal_csv(path)
    x_norm = normalize_time_signal(x)
    freqs, amps = fft_normalized(x_norm, NFFT, fs)
    amps_dict[key] = amps
    labels_plot.append(key)

# Nada carregado?
if not amps_dict:
    raise RuntimeError("Nenhum arquivo encontrado. Verifique os caminhos.")

# 2D
plot_fft_2d(freqs, amps_dict, F_MAX, OUT_2D_PATH)

# 3D
amps_mat = np.vstack([amps_dict[k] for k in labels_plot])  # (Nsinais, F)
plot_fft_3d(freqs, amps_mat, labels_plot, F_MAX, DOWNSAMPLE_3D, OUT_3D_PATH)

plt.show()
