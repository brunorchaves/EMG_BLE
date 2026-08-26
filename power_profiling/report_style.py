"""Identidade visual do relatorio de consumo.

Separado de report_pdf.py para que o estilo seja uma decisao unica e
consistente, em vez de cores e fontes espalhadas por cada figura.
"""

from __future__ import annotations

import matplotlib as mpl

# Paleta: cinzas de viés azulado (instrumento de laboratório) com um teal
# saturado para o dado medido, âmbar para o estado "antes"/atenção e
# vermelho-tijolo para limites e alertas. Deliberadamente não usa o
# azul-para-roxo de dashboard genérico.
INK        = "#16211f"
INK_MID    = "#4a5b59"
INK_SOFT   = "#7d8f8d"
LINE       = "#d3dadc"
PAPER      = "#ffffff"
ACCENT     = "#0d6f78"   # medição atual
ACCENT_LT  = "#5fb3ba"
BEFORE     = "#c47f2a"   # estado anterior / referência
ALERT      = "#9d2f2a"   # limite, meta, alerta
GOOD       = "#1f6b45"
VIOLET     = "#7a5ea8"

# Cores das faixas por estado. As quatro do artigo seguem a convenção que ele
# usa (OFF roxo, IDLE azul-claro, CONNECTED laranja, TRANSMITTING rosa); os
# estados novos herdam o tom do estado a que mais se parecem.
STATE_COLORS = {
    "OFF":              "#c3bcd6",
    "BOOT":             "#9b7fc0",
    "ADVERTISING":      "#a9d3e8",
    "CONNECTING":       "#d8d8d8",
    "CONNECTED_IDLE":   "#f4c27f",
    "STREAMING":        "#f2a0b4",
    "CONNECTED_IDLE_2": "#f4c27f",
    "DISCONNECT":       "#d8d8d8",
    "RE_ADVERTISING":   "#a9d3e8",
    "OFF_FINAL":        "#c3bcd6",
}

ARTICLE_TABLE2 = {"IDLE": 2.922, "CONNECTED": 8.497, "TRANSMITTING": 9.063}

# Alvos do projeto
TARGET_MEAN_MA = 2.0
TARGET_PEAK_MA = 5.0


def apply() -> None:
    """Aplica o estilo globalmente. Fontes: DejaVu (sempre presente com o
    matplotlib) para nao depender de fonte instalada no sistema."""
    mpl.rcParams.update({
        "figure.facecolor":  PAPER,
        "axes.facecolor":    PAPER,
        "savefig.facecolor": PAPER,
        "font.family":       "DejaVu Sans",
        "font.size":         9.5,
        "axes.labelsize":    9.5,
        "axes.labelcolor":   INK_MID,
        "axes.titlesize":    11.5,
        "axes.titleweight":  "bold",
        "axes.titlecolor":   INK,
        "axes.titlelocation": "left",
        "axes.titlepad":     11,
        "axes.edgecolor":    LINE,
        "axes.linewidth":    0.9,
        "axes.spines.top":   False,
        "axes.spines.right": False,
        "axes.grid":         True,
        "grid.color":        LINE,
        "grid.linewidth":    0.6,
        "grid.alpha":        0.7,
        "xtick.color":       INK_SOFT,
        "ytick.color":       INK_SOFT,
        "xtick.labelsize":   8.5,
        "ytick.labelsize":   8.5,
        "xtick.direction":   "out",
        "ytick.direction":   "out",
        "legend.frameon":    False,
        "legend.fontsize":   8,
        "lines.solid_capstyle": "round",
        "pdf.fonttype":      42,   # TrueType: texto do PDF fica selecionavel
    })


def page_header(fig, kicker: str, title: str, subtitle: str | None = None) -> None:
    """Cabecalho consistente de pagina: filete + kicker + titulo."""
    # matplotlib nao suporta letter-spacing; o espacamento do kicker e feito
    # inserindo espacos entre os caracteres
    fig.text(0.062, 0.955, " ".join(kicker.upper()), fontsize=7.6, color=ACCENT,
             fontweight="bold")
    fig.text(0.062, 0.925, title, fontsize=17, color=INK, fontweight="bold", va="top")
    y = 0.885
    if subtitle:
        fig.text(0.062, y, subtitle, fontsize=9.6, color=INK_MID, va="top")
        y -= 0.028
    fig.lines.append(
        mpl.lines.Line2D([0.062, 0.938], [y - 0.012, y - 0.012],
                         transform=fig.transFigure, color=LINE, lw=1.0)
    )


def page_footer(fig, left: str, right: str) -> None:
    fig.text(0.062, 0.035, left, fontsize=7.2, color=INK_SOFT)
    fig.text(0.938, 0.035, right, fontsize=7.2, color=INK_SOFT, ha="right")
