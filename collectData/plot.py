import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# === CONFIGURAÇÕES ===
ARQUIVO_CSV = 'biceps_10s_6.csv'  # Altere para o nome do arquivo CSV
TAXA_AMOSTRAGEM = 2000  # Hz (ajuste conforme necessário)

def plotar_csv(nome_arquivo):
    # Carrega o CSV
    df = pd.read_csv(nome_arquivo)

    # Gera vetor de tempo baseado na taxa de amostragem
    tempo = np.arange(len(df)) / TAXA_AMOSTRAGEM

    # Plota
    plt.figure(figsize=(10, 4))
    plt.plot(tempo, df['emg_value'], linewidth=1)
    plt.title('Sinal EMG - 5 segundos')
    plt.xlabel('Tempo (s)')
    plt.ylabel('EMG (ADC)')
    plt.grid(True)
    plt.tight_layout()
    plt.show()

if __name__ == '__main__':
    plotar_csv(ARQUIVO_CSV)
