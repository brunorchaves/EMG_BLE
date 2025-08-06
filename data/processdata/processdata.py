#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Gera quatro figuras (sinais e FFT) em PNG e EPS sem transparência.

signals_prop.(png|eps)
signals_clin.(png|eps)
fft_prop.(png|eps)
fft_clin.(png|eps)
"""

from pathlib import Path
import numpy as np
import pandas as pd
from scipy.signal import hilbert, butter, filtfilt
import matplotlib.pyplot as plt

# ---------------------------- CONFIGURAÇÃO --------------------------------- #
BASEDIR = Path(__file__).resolve().parent
FILES = {
    "biceps_clin":   "biceps_20s_2_clinical.csv",
    "thigh_clin":    "thigh_20s_1_clinical.csv",
    "biceps_prop":   "biceps_10s_4_proprietary.csv",
    "thigh_prop":    "thigh_5s_2_1x_proprietary.csv",
}

fs  = 2000           # Hz
fc  = 5              # Hz – corte low-pass da envoltória
thr_default      = 0.20
thr_biceps_prop  = 0.35
show_duration    = 5.0  # s exibidos nos gráficos de sinais
b, a = butter(4, fc / (fs / 2), btype="low")

# Paleta pastel clara (sem alfa)
COL_FILL   = "#FFF8CC"   # amarelo bem suave
COL_RAW    = "#78A6E6"   # azul claro
COL_ENV    = "#E23B3B"   # vermelho
COL_GRID   = "#DDDDDD"

# -------------------------- FUNÇÃO DE SALVAMENTO --------------------------- #
def save_fig(fig: plt.Figure, stem: str, dpi_png: int = 300):
    png = BASEDIR / f"{stem}.png"
    eps = BASEDIR / f"{stem}.eps"
    fig.savefig(png, dpi=dpi_png, bbox_inches="tight")
    fig.savefig(eps, format="eps", bbox_inches="tight")
    print(f"Figuras salvas: {png.name}  |  {eps.name}")

# ------------------------- FUNÇÕES AUXILIARES ------------------------------ #
def load_signal(p: Path):
    df = pd.read_csv(p)
    y = df.iloc[:, 1].to_numpy() if df.shape[1] >= 2 else df.iloc[:, 0].to_numpy()
    n = len(y)
    t = np.arange(n) / fs
    return t, y

def preprocess(x):
    x = x - np.mean(x)
    return x / (np.max(np.abs(x)) or 1)

def smooth_env(x_norm):
    env = np.abs(hilbert(np.abs(x_norm)))
    env = filtfilt(b, a, env)
    return env / (np.max(env) or 1)

def snr_db(sig, env, thr):
    active = env > thr
    quiet  = ~active
    if not quiet.any():
        return np.nan
    rms_sig = np.sqrt(np.mean(sig[active]**2))
    rms_noi = np.sqrt(np.mean(sig[quiet]**2))
    return 20 * np.log10(rms_sig / rms_noi) if rms_noi else np.nan

def fft_amplitude(x_norm):
    n = len(x_norm)
    freqs = np.fft.rfftfreq(n, d=1/fs)
    amps  = np.abs(np.fft.rfft(x_norm)) / n
    return freqs, amps

def crop_to_duration(t, x, env, duration):
    mask = t <= duration
    return t[mask], x[mask], env[mask]

# ------------------------- CARREGAMENTO E PROCESSO ------------------------- #
processed = {}
signals_info = {}
for key, fname in FILES.items():
    path = BASEDIR / fname
    if not path.exists():
        continue
    t, raw = load_signal(path)
    x   = preprocess(raw)
    env = smooth_env(x)
    thr = thr_biceps_prop if key == "biceps_prop" else thr_default
    snr = snr_db(x, env, thr)
    processed[key] = x
    signals_info[key] = (t, x, env, snr, path.stem, thr)

# ------------------------- FIGURA 1 – Proprietário ------------------------- #
fig1, axs1 = plt.subplots(2, 1, figsize=(12, 6), sharey=True)
for ax, key in zip(axs1, ["biceps_prop", "thigh_prop"]):
    if key not in signals_info:
        ax.text(0.5, 0.5, "Arquivo não encontrado", ha="center", va="center")
        continue
    t, x, env, snr, title, thr = signals_info[key]
    t, x, env = crop_to_duration(t, x, env, show_duration)
    ax.fill_between(t, -1, 1, where=env > thr, color=COL_FILL, step="mid")
    ax.plot(t, x,   color=COL_RAW, lw=1.0, label="Sinal EMG Normalizado")
    ax.plot(t, env, color=COL_ENV, lw=1.4, label="Envoltória (Envelope)")
    ax.axhline(thr, color="orange", ls="--", lw=1.2, label=f"Limiar {thr:.2f}")
    ax.text(0.02, 0.92, f"SNR ≈ {snr:.1f} dB", transform=ax.transAxes,
            fontsize=10, bbox=dict(boxstyle="round,pad=0.3", fc="white"))
    ax.set_title(title)
    ax.set_xlabel("Tempo (s)")
    ax.set_ylabel("Amp. (norm.)")
    ax.legend(fontsize=8)
    ax.grid(color=COL_GRID, lw=0.6)
fig1.suptitle("Sinais de Contração (5 s) – Sensor Proprietário (Coxa e Tríceps)")
plt.tight_layout()
save_fig(fig1, "signals_prop")

# ------------------------- FIGURA 2 – Clínico ------------------------------ #
fig2, axs2 = plt.subplots(2, 1, figsize=(12, 6), sharey=True)
for ax, key in zip(axs2, ["biceps_clin", "thigh_clin"]):
    if key not in signals_info:
        ax.text(0.5, 0.5, "Arquivo não encontrado", ha="center", va="center")
        continue
    t, x, env, snr, title, thr = signals_info[key]
    t, x, env = crop_to_duration(t, x, env, show_duration)
    ax.fill_between(t, -1, 1, where=env > thr, color=COL_FILL, step="mid")
    ax.plot(t, x,   color=COL_RAW, lw=1.0, label="Sinal EMG Normalizado")
    ax.plot(t, env, color=COL_ENV, lw=1.4, label="Envoltória (Envelope)")
    ax.axhline(thr, color="orange", ls="--", lw=1.2, label=f"Limiar {thr:.2f}")
    ax.text(0.02, 0.92, f"SNR ≈ {snr:.1f} dB", transform=ax.transAxes,
            fontsize=10, bbox=dict(boxstyle="round,pad=0.3", fc="white"))
    ax.set_title(title)
    ax.set_xlabel("Tempo (s)")
    ax.set_ylabel("Amp. (norm.)")
    ax.legend(fontsize=8)
    ax.grid(color=COL_GRID, lw=0.6)
fig2.suptitle("Sinais de Contração (5 s) – Circuito Clínico (Coxa e Tríceps)")
plt.tight_layout()
save_fig(fig2, "signals_clin")

# ------------------------- FIGURA 3 – FFT Proprietário --------------------- #
fig3, ax3 = plt.subplots(figsize=(10, 5))
for key, lbl, col in [("biceps_prop", "Bíceps (Proprietário)", COL_RAW),
                      ("thigh_prop",  "Coxa (Proprietário)",   "#FF9B33")]:
    if key in processed:
        f, A = fft_amplitude(processed[key])
        ax3.plot(f, A, label=lbl, color=col, lw=1)
ax3.set_title("FFT – Sensor Proprietário (Sinal Completo)")
ax3.set_xlabel("Frequência (Hz)")
ax3.set_ylabel("|X(f)| (a.u.)")
ax3.set_xlim(0, fs / 2)
ax3.legend()
ax3.grid(color=COL_GRID, lw=0.6)
plt.tight_layout()
save_fig(fig3, "fft_prop")

# ------------------------- FIGURA 4 – FFT Clínico -------------------------- #
fig4, ax4 = plt.subplots(figsize=(10, 5))
for key, lbl, col in [("biceps_clin", "Bíceps (Clínico)", COL_RAW),
                      ("thigh_clin",  "Coxa (Clínico)",   "#FF9B33")]:
    if key in processed:
        f, A = fft_amplitude(processed[key])
        ax4.plot(f, A, label=lbl, color=col, lw=1)
ax4.set_title("FFT – Circuito Clínico (Sinal Completo)")
ax4.set_xlabel("Frequência (Hz)")
ax4.set_ylabel("|X(f)| (a.u.)")
ax4.set_xlim(0, fs / 2)
ax4.legend()
ax4.grid(color=COL_GRID, lw=0.6)
plt.tight_layout()
save_fig(fig4, "fft_clin")

plt.show()
