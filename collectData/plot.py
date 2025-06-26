import pandas as pd
import numpy as np
import glob
import os
import matplotlib.pyplot as plt
from scipy.signal import butter, filtfilt

def calcular_envoltoria_suave(sinal, fs, cutoff=10):
    sinal_retificado = np.abs(sinal)
    b, a = butter(N=4, Wn=cutoff/(fs/2), btype='low')
    return filtfilt(b, a, sinal_retificado)

def normalize_signal(sig):
    return (sig - np.min(sig)) / (np.max(sig) - np.min(sig))

fs = 2000
csv_folder = 'csvs'
csv_files  = glob.glob(os.path.join(csv_folder, '*.csv'))

for path in csv_files:
    filename = os.path.basename(path)
    # lê o CSV ignorando linhas “mal-formadas”
    df = pd.read_csv(
        path,
        comment='#',            # se quiser ignorar linhas começando com #
        engine='python',        # precisa do Python engine para on_bad_lines
        on_bad_lines='skip'     # pula qualquer linha que dê erro de parsing
    )
    # converte a coluna em numérico, forçando NaN onde falhar, e joga fora as NaNs
    adc = pd.to_numeric(df.iloc[:,0], errors='coerce').dropna().values

    # calcula envoltória e normaliza
    adc_n = normalize_signal(adc)
    env = calcular_envoltoria_suave(adc_n, fs)


    t = np.arange(len(adc_n)) / fs
    plt.figure(figsize=(8,3))
    plt.plot(t, adc_n,  label='Sinal bruto (norm.)', alpha=0.7)
    plt.plot(t, env,  label='Envoltória (norm.)', color='red', lw=2)
    plt.title(filename)
    plt.xlabel('Tempo (s)')
    plt.ylabel('Amplitude normalizada')
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.show()
