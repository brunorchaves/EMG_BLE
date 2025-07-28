#!/usr/bin/env python3
"""
▶ Figura 1  (signals_prop.png)
    • Sinais de contração (Coxa e Tríceps) do sensor proprietário (somente 5 s)
▶ Figura 2  (signals_clin.png)
    • Sinais de contração (Coxa e Tríceps) do circuito clínico (somente 5 s)
▶ Figura 3  (fft_prop.png)
    • FFT dos sinais do sensor proprietário (sinal completo)
▶ Figura 4  (fft_clin.png)
    • FFT dos sinais do circuito clínico (sinal completo)
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

fs  = 2000   # Hz
fc  = 5      # Hz – corte low-pass da envoltória
thr_default = 0.20        # limiar padrão
thr_biceps_prop = 0.35    # limiar mais alto para bíceps proprietário
show_duration = 5.0       # segundos exibidos nos gráficos de sinais
b, a = butter(4, fc / (fs / 2), btype="low")

# Rótulos para títulos dos subplots
TITLES = {
    "biceps_prop": "Bíceps – 5 segundos de coleta",
    "thigh_prop":  "Coxa – 5 segundos de coleta",
    "biceps_clin": "Bíceps – 5 segundos de coleta",
    "thigh_clin":  "Coxa – 5 segundos de coleta",
}

# ------------------------- FUNÇÕES AUXILIARES ------------------------------ #
def load_signal(p: Path):
    df = pd.read_csv(p)
    y = df.iloc[:, 1].to_numpy() if df.shape[1] >= 2 else df.iloc[:, 0].to_numpy()
    n = len(y)
    t = np.arange(n) / fs  # eixo em segundos
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

# ------------------------- CARREGA E PROCESSA ------------------------------ #
processed = {}
signals_info = {}
for key, fname in FILES.items():
    path = BASEDIR / fname
    if not path.exists():
        continue
    t, raw = load_signal(path)
    x = preprocess(raw)
    env = smooth_env(x)
    # Define limiar: maior para bíceps proprietário
    thr = thr_biceps_prop if key == "biceps_prop" else thr_default
    snr = snr_db(x, env, thr)
    processed[key] = x
    signals_info[key] = (t, x, env, snr, thr)

# ------------------------- FUNÇÃO PARA LIMITAR DURAÇÃO --------------------- #
def crop_to_duration(t, x, env, duration):
    mask = t <= duration
    return t[mask], x[mask], env[mask]

# ------------------------- FIGURA 1 – sinais proprietário ------------------ #
fig1, axs1 = plt.subplots(2, 1, figsize=(12, 6), sharey=True)
for ax, key in zip(axs1, ["biceps_prop", "thigh_prop"]):
    if key not in signals_info:
        ax.text(0.5, 0.5, "Arquivo não encontrado", ha="center", va="center")
        continue
    t, x, env, snr, thr = signals_info[key]
    t, x, env = crop_to_duration(t, x, env, show_duration)
    ax.fill_between(t, -1, 1, where=env > thr, color="gold", alpha=0.15, step="mid")
    ax.plot(t, x,   color="#1f77b4", alpha=0.5, label="Sinal EMG Normalizado")
    ax.plot(t, env, color="red", lw=1.4, label="Envoltória (Envelope)")
    ax.axhline(thr, color="orange", ls="--", lw=1.2, label=f"Limiar de Atividade ({thr:.2f})")
    ax.text(0.02, 0.92, f"SNR ≈ {snr:.1f} dB",
            transform=ax.transAxes, fontsize=10,
            bbox=dict(boxstyle="round,pad=0.3", fc="white", alpha=0.8))
    ax.set_title(TITLES.get(key, key))
    ax.set_xlabel("Tempo (s)")
    ax.set_ylabel("Amp. (norm.)")
    ax.legend(fontsize=8)
fig1.suptitle("Sinais de Contração (5 s) – Sensor Proprietário")
plt.tight_layout()
fig1_path = BASEDIR / "signals_prop.png"
fig1.savefig(fig1_path, dpi=300)
print(f"Figura 1 salva em: {fig1_path}")

# ------------------------- FIGURA 2 – sinais clínicos ---------------------- #
fig2, axs2 = plt.subplots(2, 1, figsize=(12, 6), sharey=True)
for ax, key in zip(axs2, ["biceps_clin", "thigh_clin"]):
    if key not in signals_info:
        ax.text(0.5, 0.5, "Arquivo não encontrado", ha="center", va="center")
        continue
    t, x, env, snr, thr = signals_info[key]
    t, x, env = crop_to_duration(t, x, env, show_duration)
    ax.fill_between(t, -1, 1, where=env > thr, color="gold", alpha=0.15, step="mid")
    ax.plot(t, x,   color="#1f77b4", alpha=0.5, label="Sinal EMG Normalizado")
    ax.plot(t, env, color="red", lw=1.4, label="Envoltória (Envelope)")
    ax.axhline(thr, color="orange", ls="--", lw=1.2, label=f"Limiar de Atividade ({thr:.2f})")
    ax.text(0.02, 0.92, f"SNR ≈ {snr:.1f} dB",
            transform=ax.transAxes, fontsize=10,
            bbox=dict(boxstyle="round,pad=0.3", fc="white", alpha=0.8))
    ax.set_title(TITLES.get(key, key))
    ax.set_xlabel("Tempo (s)")
    ax.set_ylabel("Amp. (norm.)")
    ax.legend(fontsize=8)
fig2.suptitle("Sinais de Contração (5 s) – Circuito Clínico")
plt.tight_layout()
fig2_path = BASEDIR / "signals_clin.png"
fig2.savefig(fig2_path, dpi=300)
print(f"Figura 2 salva em: {fig2_path}")

# ------------------------- FIGURA 3 – FFT Proprietário --------------------- #
fig3, ax3 = plt.subplots(figsize=(10, 5))
for key, lbl, col in [("biceps_prop", "Bíceps (Proprietário)", "#1f77b4"),
                      ("thigh_prop",  "Coxa (Proprietário)",   "#ff7f0e")]:
    if key in processed:
        f, A = fft_amplitude(processed[key])
        ax3.plot(f, A, label=lbl, color=col, lw=1)
ax3.set_title("FFT – Sinais do Sensor Proprietário (Sinal Completo)")
ax3.set_xlabel("Frequência (Hz)")
ax3.set_ylabel("|X(f)| (a.u.)")
ax3.set_xlim(0, fs / 2)
ax3.legend()
ax3.grid(alpha=0.3)
fig3_path = BASEDIR / "fft_prop.png"
fig3.savefig(fig3_path, dpi=300)
print(f"Figura 3 salva em: {fig3_path}")

# ------------------------- FIGURA 4 – FFT Clínico -------------------------- #
fig4, ax4 = plt.subplots(figsize=(10, 5))
for key, lbl, col in [("biceps_clin", "Bíceps (Clínico)", "#1f77b4"),
                      ("thigh_clin",  "Coxa (Clínico)",   "#ff7f0e")]:
    if key in processed:
        f, A = fft_amplitude(processed[key])
        ax4.plot(f, A, label=lbl, color=col, lw=1)
ax4.set_title("FFT – Sinais do Circuito Clínico (Sinal Completo)")
ax4.set_xlabel("Frequência (Hz)")
ax4.set_ylabel("|X(f)| (a.u.)")
ax4.set_xlim(0, fs / 2)
ax4.legend()
ax4.grid(alpha=0.3)
fig4_path = BASEDIR / "fft_clin.png"
fig4.savefig(fig4_path, dpi=300)
print(f"Figura 4 salva em: {fig4_path}")

plt.show()
