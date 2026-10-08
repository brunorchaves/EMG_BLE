# -*- coding: utf-8 -*-
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

REPO = str(Path(__file__).resolve().parent.parent)   # as capturas ficam um nivel acima
OUT  = str(Path(__file__).resolve().parent)

files = ["scope_0.csv", "scope_1.csv"]
data = {}
for f in files:
    d = np.genfromtxt(REPO + "\\" + f, delimiter=",", skip_header=2)
    data[f] = (d[:, 0], d[:, 1], d[:, 2])

fs = 1.0 / np.median(np.diff(data[files[0]][0]))

def spec(v, fs):
    v = v - v.mean()
    w = np.hanning(len(v))
    V = np.fft.rfft(v * w)
    fr = np.fft.rfftfreq(len(v), 1 / fs)
    mag = np.abs(V) / (np.sum(w) / 2)
    return fr, mag

fig, ax = plt.subplots(4, 2, figsize=(14, 12))

for col, f in enumerate(files):
    t, ch1, ch2 = data[f]
    ax[0, col].plot(t, ch1, lw=.5, color="#1c3f6e")
    ax[0, col].set_title(f + "  —  CH1", fontsize=10)
    ax[0, col].set_ylabel("V")
    ax[1, col].plot(t, ch2 * 1e3, lw=.5, color="#a85a12")
    ax[1, col].set_title(f + "  —  CH2", fontsize=10)
    ax[1, col].set_ylabel("mV")
    ax[1, col].set_xlabel("s")

    fr, m1 = spec(ch1, fs)
    _, m2 = spec(ch2, fs)
    ax[2, col].semilogy(fr, m1, lw=.6, color="#1c3f6e")
    ax[2, col].set_title("espectro CH1", fontsize=10)
    ax[2, col].set_xlim(0, fs / 2)
    ax[2, col].set_ylabel("V")
    ax[3, col].semilogy(fr, m2 * 1e3, lw=.6, color="#a85a12")
    ax[3, col].set_title("espectro CH2", fontsize=10)
    ax[3, col].set_xlim(0, fs / 2)
    ax[3, col].set_ylabel("mV")
    ax[3, col].set_xlabel("Hz")
    for r in range(4):
        ax[r, col].grid(alpha=.3)

plt.tight_layout()
plt.savefig(OUT + "\\scope_overview.png", dpi=110)
print("figura salva")

# --- relacao entre canais e entre arquivos ---
print()
for f in files:
    t, ch1, ch2 = data[f]
    a = ch1 - ch1.mean()
    b = ch2 - ch2.mean()
    r = np.corrcoef(a, b)[0, 1]
    # ganho por minimos quadrados: a ~ k*b
    k = np.dot(a, b) / np.dot(b, b)
    print("%s  corr(CH1,CH2)=%+.4f   CH1/CH2 por LSQ = %.1f" % (f, r, k))

t0, a0, b0 = data[files[0]]
t1, a1, b1 = data[files[1]]
print("corr CH1 entre arquivos = %+.4f" % np.corrcoef(a0 - a0.mean(), a1 - a1.mean())[0, 1])
print("corr CH2 entre arquivos = %+.4f" % np.corrcoef(b0 - b0.mean(), b1 - b1.mean())[0, 1])

# --- picos espectrais ---
print()
for f in files:
    t, ch1, ch2 = data[f]
    for name, v in (("CH1", ch1), ("CH2", ch2)):
        fr, m = spec(v, fs)
        idx = np.argsort(m)[::-1][:6]
        idx = idx[fr[idx] > 1.0]
        picos = ", ".join("%.1f Hz (%.3g)" % (fr[i], m[i]) for i in idx[:5])
        print("%s %s picos: %s" % (f, name, picos))

# --- banda de energia ---
print()
for f in files:
    t, ch1, ch2 = data[f]
    for name, v in (("CH1", ch1), ("CH2", ch2)):
        fr, m = spec(v, fs)
        p = m ** 2
        tot = p[fr > 1].sum()
        b1_ = p[(fr > 1) & (fr < 20)].sum() / tot
        b2_ = p[(fr >= 20) & (fr < 185)].sum() / tot
        print("%s %s  energia: <20 Hz = %4.1f%%   20-185 Hz = %4.1f%%" % (f, name, 100 * b1_, 100 * b2_))

# --- quantizacao: menor passo nao nulo ---
print()
for f in files:
    t, ch1, ch2 = data[f]
    for name, v in (("CH1", ch1), ("CH2", ch2)):
        u = np.unique(np.abs(np.diff(np.unique(v))))
        u = u[u > 1e-12]
        print("%s %s  passo minimo entre niveis = %.6g V   niveis distintos = %d"
              % (f, name, u.min() if len(u) else float('nan'), len(np.unique(v))))
