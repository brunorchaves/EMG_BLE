"""Acesso ao UCI EMG Physical Action Data Set e as metricas do artigo.

O dataset (Theodoridis 2011, DOI 10.24432/C53W49, ref. [22] do artigo) foi
gravado com um Delsys de 8 canais. Fatos que nao estao no readme da UCI e
foram verificados nos arquivos:

* taxa de amostragem = 1000 S/s: o cabecalho de cada .log traz Start/End com
  resolucao de segundo, e ~9 800 a 10 000 amostras cobrem 9 a 10 s;
* unidade = uV, com ceifamento em +-4000 (o sub1/Sidekicking passa ~50 % do
  tempo ceifado nos canais de perna);
* colunas 0-7 = R-Bic, R-Tri, L-Bic, L-Tri, R-Thi, R-Ham, L-Thi, L-Ham.
"""

from __future__ import annotations

import hashlib
from dataclasses import dataclass
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parents[2]
DATA_DIR = REPO / "data" / "uci_emg_physical_action"
DATASET_DIR = DATA_DIR / "EMG Physical Action Data Set"
ZIP_PATH = DATA_DIR / "emg_physical_action.zip"
ZIP_URL = "https://archive.ics.uci.edu/static/public/213/emg+physical+action+data+set.zip"
ZIP_SHA256 = "977088b740cb1063abb33efe58923e0bea6a71bdf354a8dbb13de44025f1a91f"

FS_SRC = 1000
CLIP_UV = 4000
CHANNELS = ["R-Bic", "R-Tri", "L-Bic", "L-Tri", "R-Thi", "R-Ham", "L-Thi", "L-Ham"]
LEG_COLUMNS = (4, 5, 6, 7)

# Fig. 5 do artigo: o "original" tem SNR de 10,7 dB pelo metodo da secao III
ARTICLE_SNR_DB = 10.7


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def action_path(subject: int, action: str = "Sidekicking", group: str = "Aggressive") -> Path:
    p = DATASET_DIR / f"sub{subject}" / group / "txt" / f"{action}.txt"
    if not p.exists():
        raise FileNotFoundError(f"{p} nao existe - rode tools/fetch_dataset.py")
    return p


def load_action(subject: int, action: str = "Sidekicking", group: str = "Aggressive") -> np.ndarray:
    """Matriz (amostras, 8) em uV."""
    return np.loadtxt(action_path(subject, action, group))


def channel_index(ch: str | int) -> int:
    if isinstance(ch, int) or str(ch).isdigit():
        i = int(ch)
        if not 0 <= i < 8:
            raise ValueError("coluna fora de 0..7")
        return i
    return CHANNELS.index(ch)


def rms_envelope(x: np.ndarray, fs: float, win_s: float = 0.050) -> np.ndarray:
    """RMS em janela deslizante de 50 ms (secao III do artigo)."""
    n = max(1, int(round(win_s * fs)))
    return np.sqrt(np.convolve(x * x, np.ones(n) / n, mode="same"))


@dataclass
class ArticleMetrics:
    snr_db: float
    bursts: int
    active: np.ndarray  # bool por amostra
    envelope: np.ndarray


def article_metrics(x: np.ndarray, fs: float, thr: float = 0.20, min_burst_s: float = 0.050) -> ArticleMetrics:
    """Pipeline do artigo: normaliza a [-1, 1], envelope RMS de 50 ms, ativo
    onde o envelope passa de 20 % do pico, SNR = RMS(ativo) / RMS(repouso)."""
    x = x - np.mean(x)
    peak = np.max(np.abs(x))
    x = x / peak if peak > 0 else x
    env = rms_envelope(x, fs)
    active = env > thr * env.max()
    edges = np.flatnonzero(np.diff(np.r_[0, active.astype(np.int8), 0]))
    starts, stops = edges[::2], edges[1::2]
    bursts = int(np.sum((stops - starts) >= min_burst_s * fs))
    if active.all() or not active.any():
        snr = float("nan")
    else:
        snr = 20 * np.log10(np.sqrt(np.mean(x[active] ** 2)) / np.sqrt(np.mean(x[~active] ** 2)))
    return ArticleMetrics(float(snr), bursts, active, env)
