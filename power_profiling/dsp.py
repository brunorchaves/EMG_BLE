"""DSP compartilhado da bancada de energia - deliberadamente numpy-only.

scipy e pandas não estavam instalados neste repo antes desta sessão, e os
scripts existentes que os importam (data/processdata/processdata.py,
processData/featuresPlot.py) pertencem ao estudo de qualidade de sinal, não
ao bench de energia - ficam intocados. Este módulo reimplementa em numpy só
o que o bench realmente precisa: envelope RMS deslizante (que é inclusive
mais fiel ao que o próprio artigo descreve - "RMS em janelas deslizantes de
50 ms", README:97 - do que a versão por Hilbert que já existe) e um PSD via
Welch simplificado, usado para a validação espectral em emg_validate.py.
"""

from __future__ import annotations

import numpy as np


def rms_envelope(x: np.ndarray, fs: float, window_ms: float = 50.0) -> np.ndarray:
    """Envoltória por RMS em janela deslizante - método descrito no README
    (linha 97) para a detecção de segmentos ativos do artigo.
    """
    x = np.asarray(x, dtype=np.float64)
    win = max(1, int(round(fs * window_ms / 1000.0)))
    sq = x * x
    csum = np.concatenate(([0.0], np.cumsum(sq)))
    n = len(x)
    out = np.empty(n)
    half = win // 2
    for i in range(n):
        lo = max(0, i - half)
        hi = min(n, i + half + 1)
        out[i] = np.sqrt((csum[hi] - csum[lo]) / (hi - lo))
    return out


def rms_envelope_fast(x: np.ndarray, fs: float, window_ms: float = 50.0) -> np.ndarray:
    """Versão vetorizada (sem loop Python) de ``rms_envelope`` via cumsum,
    usar para arrays grandes (ex: o traço EMG inteiro de um run)."""
    x = np.asarray(x, dtype=np.float64)
    win = max(1, int(round(fs * window_ms / 1000.0)))
    n = len(x)
    if n == 0:
        return np.array([])
    sq = x * x
    csum = np.concatenate(([0.0], np.cumsum(sq)))
    half = win // 2
    lo = np.clip(np.arange(n) - half, 0, n)
    hi = np.clip(np.arange(n) + half + 1, 0, n)
    counts = (hi - lo).astype(np.float64)
    return np.sqrt((csum[hi] - csum[lo]) / counts)


def preprocess(x: np.ndarray) -> np.ndarray:
    """Remove DC e normaliza para [-1, 1] - mesma convenção usada no artigo
    e em data/processdata/processdata.py (reimplementado aqui em numpy)."""
    x = np.asarray(x, dtype=np.float64)
    x = x - np.mean(x)
    peak = np.max(np.abs(x)) or 1.0
    return x / peak


def snr_db(sig: np.ndarray, envelope: np.ndarray, threshold: float) -> float:
    """SNR em dB entre segmentos ativos (envelope > threshold) e segmentos de
    repouso - mesma fórmula do artigo/README (SNR_dB = 20*log10(rms_sig/rms_noise))."""
    sig = np.asarray(sig, dtype=np.float64)
    active = envelope > threshold
    quiet = ~active
    if not np.any(quiet) or not np.any(active):
        return float("nan")
    rms_sig = np.sqrt(np.mean(sig[active] ** 2))
    rms_noise = np.sqrt(np.mean(sig[quiet] ** 2))
    if rms_noise == 0:
        return float("nan")
    return 20.0 * np.log10(rms_sig / rms_noise)


def welch_psd(
    x: np.ndarray,
    fs: float,
    nperseg: int = 512,
    overlap: float = 0.5,
) -> tuple[np.ndarray, np.ndarray]:
    """PSD via método de Welch, implementado com numpy puro (janela de
    Hann + rfft + média de segmentos sobrepostos) - evita a dependência de
    scipy.signal só para isso.
    """
    x = np.asarray(x, dtype=np.float64)
    n = len(x)
    if n < nperseg:
        nperseg = max(8, n)
    step = max(1, int(nperseg * (1 - overlap)))
    window = np.hanning(nperseg)
    win_scale = np.sum(window**2)

    segments = []
    start = 0
    while start + nperseg <= n:
        seg = x[start : start + nperseg] * window
        segments.append(seg)
        start += step
    if not segments:
        segments = [x * np.hanning(len(x))]
        nperseg = len(x)
        win_scale = np.sum(np.hanning(len(x)) ** 2)

    freqs = np.fft.rfftfreq(nperseg, d=1.0 / fs)
    psd_acc = np.zeros(len(freqs))
    for seg in segments:
        spec = np.fft.rfft(seg)
        psd_acc += (np.abs(spec) ** 2) / (fs * win_scale)
    psd = psd_acc / len(segments)
    # dobra energia (exceto DC e Nyquist) por só termos o espectro one-sided
    psd[1:-1] *= 2
    return freqs, psd


def fft_amplitude(x_norm: np.ndarray, fs: float) -> tuple[np.ndarray, np.ndarray]:
    """Amplitude do espectro (não densidade de potência) - usado para os
    plots de FFT no estilo dos scripts existentes de análise de sinal."""
    x_norm = np.asarray(x_norm, dtype=np.float64)
    n = len(x_norm)
    freqs = np.fft.rfftfreq(n, d=1.0 / fs)
    amps = np.abs(np.fft.rfft(x_norm)) / n
    return freqs, amps


_trapz = getattr(np, "trapezoid", None) or np.trapz  # numpy >=2.0 renomeou trapz -> trapezoid


def spectral_band_fraction(freqs: np.ndarray, psd: np.ndarray, lo_hz: float, hi_hz: float) -> float:
    """Fração da potência total contida entre lo_hz e hi_hz."""
    total = _trapz(psd, freqs)
    if total <= 0:
        return float("nan")
    mask = (freqs >= lo_hz) & (freqs <= hi_hz)
    band = _trapz(psd[mask], freqs[mask]) if np.any(mask) else 0.0
    return float(band / total)


def find_corner_frequencies(freqs: np.ndarray, psd: np.ndarray) -> tuple[float, float]:
    """Estima as frequências de corte -3 dB do patamar de passband, relativas
    à mediana do platô (não assume uma banda fixa - ver risco de 'a banda
    passante depende da taxa de loop efetiva', não é 20-500 Hz fixo)."""
    if len(psd) < 8:
        return (float("nan"), float("nan"))
    plateau = np.median(psd[psd > np.percentile(psd, 60)])
    if plateau <= 0:
        return (float("nan"), float("nan"))
    half_power = plateau / 2.0
    above = psd >= half_power
    idx = np.flatnonzero(above)
    if idx.size == 0:
        return (float("nan"), float("nan"))
    return float(freqs[idx[0]]), float(freqs[idx[-1]])


def welch_ttest(a: np.ndarray, b: np.ndarray) -> tuple[float, float]:
    """Teste t de Welch (variâncias desiguais) implementado em numpy puro,
    sem scipy.stats - usado por analyze.compare_runs para significância
    estatística das otimizações antes/depois. Retorna (t, p) - p aproximado
    via a distribuição normal (razoável para os tamanhos de amostra desta
    bancada, tipicamente >> 30)."""
    a, b = np.asarray(a, dtype=np.float64), np.asarray(b, dtype=np.float64)
    na, nb = len(a), len(b)
    if na < 2 or nb < 2:
        return float("nan"), float("nan")
    va, vb = a.var(ddof=1), b.var(ddof=1)
    se = np.sqrt(va / na + vb / nb)
    if se == 0:
        return float("nan"), float("nan")
    t = (a.mean() - b.mean()) / se
    # aproximação normal do p-valor bicaudal (adequada para n grande)
    p = 2.0 * (1.0 - _norm_cdf(abs(t)))
    return float(t), float(p)


def _norm_cdf(z: float) -> float:
    return 0.5 * (1.0 + _erf(z / np.sqrt(2.0)))


def _erf(x: float) -> float:
    # aproximação de Abramowitz & Stegun 7.1.26, erro máximo ~1.5e-7
    t = 1.0 / (1.0 + 0.3275911 * abs(x))
    y = 1.0 - (
        ((((1.061405429 * t - 1.453152027) * t) + 1.421413741) * t - 0.284496736) * t
        + 0.254829592
    ) * t * np.exp(-x * x)
    return y if x >= 0 else -y
