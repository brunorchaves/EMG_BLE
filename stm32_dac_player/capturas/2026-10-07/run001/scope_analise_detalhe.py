# -*- coding: utf-8 -*-
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

REPO = str(Path(__file__).resolve().parent.parent)   # as capturas ficam um nivel acima
OUT  = str(Path(__file__).resolve().parent)

d = np.genfromtxt(REPO + r"\scope_0.csv", delimiter=",", skip_header=2)
t, ch1, ch2 = d[:, 0], d[:, 1], d[:, 2]
fs = 1.0 / np.median(np.diff(t))

a = ch1 - ch1.mean()
b = ch2 - ch2.mean()

# --- funcao de transferencia CH2/CH1 por cross-espectro, em bandas ---
nseg, nper = 8, 250
A = np.zeros(nper // 2 + 1, complex)
Paa = np.zeros(nper // 2 + 1)
Pba = np.zeros(nper // 2 + 1, complex)
w = np.hanning(nper)
for i in range(nseg):
    s = i * (nper // 2)
    if s + nper > len(a):
        break
    FA = np.fft.rfft(a[s:s + nper] * w)
    FB = np.fft.rfft(b[s:s + nper] * w)
    Paa += np.abs(FA) ** 2
    Pba += FB * np.conj(FA)
fr = np.fft.rfftfreq(nper, 1 / fs)
H = Pba / Paa
print("H = CH2/CH1 por banda (modulo e fase):")
for lo, hi in [(5, 20), (20, 50), (50, 80), (80, 120), (120, 185)]:
    k = (fr >= lo) & (fr < hi)
    print("  %3d-%3d Hz : |H| = 1/%.1f    fase = %+6.1f graus"
          % (lo, hi, 1 / np.abs(H[k]).mean(), np.degrees(np.angle(H[k].mean()))))

# --- envelope RMS ---
def env(v, win):
    n = int(win * fs)
    k = np.ones(n) / n
    return np.sqrt(np.convolve(v ** 2, k, mode="same"))

e1, e2 = env(a, 0.100), env(b, 0.100)

# --- autocorrelacao ---
ac = np.correlate(a, a, "full")[len(a) - 1:]
ac /= ac[0]
lags = np.arange(len(ac)) / fs

# --- o surto perto de +2.28 s ---
m = (t > 2.20) & (t < 2.40)
print()
print("surto em t~2.28 s:  %d amostras na janela,  CH1 pp = %.3f V,  CH2 pp = %.1f mV"
      % (m.sum(), ch1[m].max() - ch1[m].min(), 1e3 * (ch2[m].max() - ch2[m].min())))
sub = a[m]
cruz = np.sum(np.diff(np.sign(sub)) != 0)
dur = (t[m][-1] - t[m][0])
print("  cruzamentos por zero na janela = %d  ->  freq aparente ~ %.1f Hz" % (cruz, cruz / (2 * dur)))

# --- trecho quieto vs ativo ---
quiet = (t > 1.6) & (t < 2.2)
act = (t > -0.3) & (t < 0.3)
print()
print("trecho QUIETO (1,6-2,2 s): CH1 rms = %.4f V   CH2 rms = %.3f mV"
      % (a[quiet].std(), 1e3 * b[quiet].std()))
print("trecho ATIVO  (-0,3-0,3 s): CH1 rms = %.4f V   CH2 rms = %.3f mV"
      % (a[act].std(), 1e3 * b[act].std()))
print("razao ativo/quieto: CH1 = %.1f dB   CH2 = %.1f dB"
      % (20 * np.log10(a[act].std() / a[quiet].std()),
         20 * np.log10(b[act].std() / b[quiet].std())))

fig, ax = plt.subplots(4, 1, figsize=(13, 11))
ax[0].plot(t, e1, color="#1c3f6e", label="CH1 (V)")
ax[0].plot(t, e2 * 13.6, color="#a85a12", ls="--", label="CH2 × 13,6 (V)")
ax[0].set_title("envelope RMS, janela de 100 ms — CH2 escalado por 13,6", fontsize=10)
ax[0].legend(fontsize=8); ax[0].grid(alpha=.3); ax[0].set_ylabel("V")

ax[1].plot(t[m], ch1[m], ".-", ms=3, lw=.7, color="#1c3f6e")
ax[1].set_title("CH1, zoom no surto de t ≈ 2,28 s (pontos = amostras)", fontsize=10)
ax[1].grid(alpha=.3); ax[1].set_ylabel("V"); ax[1].set_xlabel("s")

ax[2].plot(lags, ac, lw=.7, color="#1c3f6e")
ax[2].set_xlim(0, 3); ax[2].set_title("autocorrelação CH1", fontsize=10)
ax[2].grid(alpha=.3); ax[2].set_xlabel("atraso (s)")

ax[3].plot(a, b * 1e3, ".", ms=1.5, alpha=.4, color="#2b6349")
ax[3].set_title("CH2 vs CH1 — se fosse o mesmo sinal escalado, seria uma reta", fontsize=10)
ax[3].set_xlabel("CH1 (V)"); ax[3].set_ylabel("CH2 (mV)"); ax[3].grid(alpha=.3)

plt.tight_layout()
plt.savefig(OUT + r"\scope_detail.png", dpi=110)
print("\nfigura salva")
