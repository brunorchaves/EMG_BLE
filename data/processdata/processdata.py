#!/usr/bin/env python3
"""
▶ Figure 1  (signals_envelope_snr.png)
    • Raw norm. + Envelope + limiar + SNR
▶ Figure 2  (fft_clinico_vs_proprietario.png)
    • FFT dos sinais normalizados (clínico vs proprietário)

As figuras também são exibidas na tela.  Requisitos: pandas numpy scipy matplotlib
"""

from pathlib import Path
import numpy as np
import pandas as pd
from scipy.signal import hilbert, butter, filtfilt
import matplotlib.pyplot as plt

# ------------------------- CONFIG GERAL ------------------------------------ #
BASEDIR = Path(__file__).resolve().parent
FILES = {
    "biceps_clin":   "biceps_20s_2_clinical.csv",
    "thigh_clin":    "thigh_20s_1_clinical.csv",
    "biceps_prop":   "biceps_10s_4_proprietary.csv",
    "thigh_prop":    "thigh_5s_2_1x_proprietary.csv",
}

fs  = 1000   # Hz – ajuste se necessário
fc  = 5      # Hz – corte low-pass da envoltória
thr = 0.20   # 20 % do pico → limiar ativo/inativo
b, a = butter(4, fc / (fs / 2), btype="low")

# ------------------------- FUNÇÕES AUXILIARES ------------------------------ #
def load_signal(p: Path):
    df = pd.read_csv(p)
    if df.shape[1] >= 2:
        return df.iloc[:, 0].to_numpy(), df.iloc[:, 1].to_numpy()
    return np.arange(len(df)), df.iloc[:, 0].to_numpy()

def preprocess(x):
    x = x - np.mean(x)                      # remove offset
    return x / (np.max(np.abs(x)) or 1)     # normaliza (-1…1)

def smooth_env(x_norm):
    env = np.abs(hilbert(np.abs(x_norm)))
    env = filtfilt(b, a, env)
    return env / (np.max(env) or 1)

def snr_db(sig, env):
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

# ------------------------- FIGURE 1 – domínio do tempo --------------------- #
fig1, axs = plt.subplots(2, 2, figsize=(13, 8), sharey=True)
axs = axs.ravel()
processed = {}

for ax, (key, fname) in zip(axs, FILES.items()):
    path = BASEDIR / fname
    if not path.exists():
        ax.text(0.5, 0.5, "Arquivo\nnão encontrado", ha="center", va="center")
        ax.set_title(fname)
        continue

    t, raw = load_signal(path)
    x = preprocess(raw)
    env = smooth_env(x)
    snr = snr_db(x, env)
    processed[key] = x

    ax.fill_between(t, -1, 1, where=env > thr,
                    color="gold", alpha=0.15, step="mid")
    ax.plot(t, x,   color="#1f77b4", alpha=0.5, label="Raw norm.")
    ax.plot(t, env, color="red",     lw=1.4,    label="Envelope")
    ax.axhline(thr, color="orange", ls="--", lw=1.2, label=f"thr={thr:.2f}")
    ax.text(0.02, 0.92, f"SNR ≈ {snr:.1f} dB",
            transform=ax.transAxes, fontsize=10,
            bbox=dict(boxstyle="round,pad=0.3", fc="white", alpha=0.8))
    ax.set_title(path.stem)
    ax.set_xlabel("Tempo / Amostra")
    ax.set_ylabel("Amp. (norm.)")
    ax.legend(fontsize=8, loc="upper right")

plt.tight_layout()
fig1_path = BASEDIR / "signals_envelope_snr.png"
fig1.savefig(fig1_path, dpi=300)
print(f"Figure 1 salva em: {fig1_path}")

plt.show()

# ------------------------- FIGURE 2 – domínio da frequência ---------------- #
fig2, (axA, axB) = plt.subplots(1, 2, figsize=(14, 5), sharey=True)

# Clínico
for key, lbl, col in [("biceps_clin", "Bíceps Clínico", "#1f77b4"),
                      ("thigh_clin",  "Coxa Clínica",   "#ff7f0e")]:
    if key in processed:
        f, A = fft_amplitude(processed[key])
        axA.plot(f, A, label=lbl, color=col, lw=1)
axA.set_title("FFT – Sinais Clínicos")
axA.set_xlabel("Frequência (Hz)")
axA.set_ylabel("|X(f)| (a.u.)")
axA.set_xlim(0, fs / 2)
axA.legend()
axA.grid(alpha=0.3)

# Proprietário
for key, lbl, col in [("biceps_prop", "Bíceps Proprietário", "#1f77b4"),
                      ("thigh_prop",  "Coxa Proprietária",   "#ff7f0e")]:
    if key in processed:
        f, A = fft_amplitude(processed[key])
        axB.plot(f, A, label=lbl, color=col, lw=1)
axB.set_title("FFT – Sinais Proprietários")
axB.set_xlabel("Frequência (Hz)")
axB.set_xlim(0, fs / 2)
axB.legend()
axB.grid(alpha=0.3)

plt.suptitle("Espectro de Amplitude (FFT) – EMG Normalizado", y=0.98)
plt.tight_layout()
fig2_path = BASEDIR / "fft_clinico_vs_proprietario.png"
fig2.savefig(fig2_path, dpi=300)
print(f"Figure 2 salva em: {fig2_path}")

plt.show()
