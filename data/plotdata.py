#!/usr/bin/env python3
"""
Percorre todos os CSVs nas pastas data/csvs_clinical e data/csvs_proprietary
e plota cada arquivo em uma janela separada.  Útil para inspecionar visualmente
os sinais e decidir quais séries usar.

Requisitos:
    pip install pandas matplotlib
"""

from pathlib import Path
import pandas as pd
import matplotlib.pyplot as plt
import sys

# --- Configurações ----------------------------------------------------------
ROOT_DIR = Path(__file__).resolve().parent       # .../data
SUBFOLDERS = ["csvs_clinical", "csvs_proprietary"]

# Se seus arquivos tiverem separador diferente ou charset específico, ajuste aqui
CSV_KWARGS = dict(sep=",", encoding="utf-8", engine="python")

# ---------------------------------------------------------------------------

def plot_csv(csv_path: Path) -> None:
    """Lê e plota um CSV qualquer (1-N colunas)."""
    df = pd.read_csv(csv_path, **CSV_KWARGS)

    # Estratégia básica:
    #  • Se houver ≥2 colunas    → 1ª é eixo X, restantes são curvas
    #  • Se houver 1 coluna só   → índice é eixo X, única série é eixo Y
    if df.shape[1] >= 2:
        x = df.iloc[:, 0]
        for col in df.columns[1:]:
            plt.plot(x, df[col], label=col)
        plt.xlabel(df.columns[0])
    else:
        plt.plot(df.index, df.iloc[:, 0], label=csv_path.stem)
        plt.xlabel("Amostra")

    plt.title(f"{csv_path.parent.name}  –  {csv_path.name}")
    plt.ylabel("Amplitude (u.a.)")
    plt.legend(loc="best")
    plt.tight_layout()
    plt.show()


def main() -> None:
    for sub in SUBFOLDERS:
        folder = ROOT_DIR / sub
        if not folder.exists():
            print(f"Aviso: pasta '{folder}' não encontrada.", file=sys.stderr)
            continue

        csv_files = sorted(folder.glob("*.csv"))
        if not csv_files:
            print(f"(Sem CSVs em {folder})")
            continue

        print(f"\n=== {sub} ({len(csv_files)} arquivos) ===")
        for csv_path in csv_files:
            print(f"→ Exibindo {csv_path.name} ...")
            try:
                plot_csv(csv_path)
            except Exception as e:
                print(f"   Erro ao processar {csv_path.name}: {e}", file=sys.stderr)


if __name__ == "__main__":
    main()
