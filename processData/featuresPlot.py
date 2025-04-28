import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import butter, filtfilt

# Função para retificar e calcular envoltória suavizada
def calcular_envoltoria_suave(sinal, fs, cutoff=10):
    sinal_retificado = np.abs(sinal)
    b, a = butter(N=4, Wn=cutoff / (fs/2), btype='low')
    envoltoria = filtfilt(b, a, sinal_retificado)
    return envoltoria

# Função para calcular FFT
def calcular_fft(sinal, fs):
    N = len(sinal)
    freq = np.fft.rfftfreq(N, d=1/fs)
    fft_magnitude = np.abs(np.fft.rfft(sinal)) / N
    return freq, fft_magnitude

# Conversão para milivolts
def converter_para_mV(adc_values, ganho_total, vref=5.0):
    tensao_adc_v = (adc_values / 32768.0) * vref
    tensao_entrada_v = tensao_adc_v / ganho_total
    tensao_entrada_mv = tensao_entrada_v * 1000.0
    return tensao_entrada_mv

# Parâmetros
fs = 2000  # Frequência de amostragem em Hz
ganho_triceps = 34.33 * 1.55 * 1.55 * 10     # Tríceps tem ganho extra de 10x
ganho_thigh   = 34.33 * 1.55 * 1.55           # Coxa não tem ganho extra

# Fator de ampliação para visualização da FFT do tríceps
fft_triceps_scale_factor = 50

# Carregar arquivos CSV
triceps_df = pd.read_csv('triceps_5s_1.csv')
thigh_df = pd.read_csv('thigh_5s_1_1x.csv')

# Dados ADC
adc_triceps = triceps_df.iloc[:, 0].values
adc_thigh = thigh_df.iloc[:, 0].values

# Conversão para mV com ganhos corretos
sinal_triceps_mv = converter_para_mV(adc_triceps, ganho_triceps)
sinal_thigh_mv = converter_para_mV(adc_thigh, ganho_thigh)

# Vetores de tempo em segundos
tempo_triceps = np.arange(len(sinal_triceps_mv)) / fs
tempo_thigh = np.arange(len(sinal_thigh_mv)) / fs

# Calcula as envoltórias suaves
envoltoria_triceps = calcular_envoltoria_suave(sinal_triceps_mv, fs)
envoltoria_thigh = calcular_envoltoria_suave(sinal_thigh_mv, fs)

# --- Plot 1: Sinal de Tríceps ---
plt.figure(figsize=(10, 4))
plt.plot(tempo_triceps, sinal_triceps_mv, label='Sinal bruto - Tríceps (mV)', alpha=0.7)
plt.plot(tempo_triceps, envoltoria_triceps, color='red', label='Envoltória suave', linestyle='-', linewidth=2)
plt.title('EMG Tríceps')
plt.xlabel('Tempo (s)')
plt.ylabel('Amplitude (mV)')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig('emg_triceps.svg', dpi=300)
plt.show()

# --- Plot 2: Sinal de Coxa ---
plt.figure(figsize=(10, 4))
plt.plot(tempo_thigh, sinal_thigh_mv, label='Sinal bruto - Coxa (mV)', alpha=0.7)
plt.plot(tempo_thigh, envoltoria_thigh, color='red', label='Envoltória suave', linestyle='-', linewidth=2)
plt.title('EMG Coxa')
plt.xlabel('Tempo (s)')
plt.ylabel('Amplitude (mV)')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig('emg_thigh.svg', dpi=300)
plt.show()

# --- FFTs ---
freq_triceps, fft_triceps = calcular_fft(sinal_triceps_mv, fs)
freq_thigh, fft_thigh = calcular_fft(sinal_thigh_mv, fs)

# Aplica o fator de escala para visualizar melhor a FFT do tríceps
fft_triceps_visual = fft_triceps * fft_triceps_scale_factor

# --- Plot 3: FFTs sobrepostas ---
plt.figure(figsize=(10, 5))
plt.plot(freq_triceps, fft_triceps_visual, label=f'FFT Tríceps (×{fft_triceps_scale_factor} para visualização)', alpha=0.8)
plt.plot(freq_thigh, fft_thigh, label='FFT Coxa', color='red', alpha=0.8)
plt.title('FFT dos sinais EMG')
plt.xlabel('Frequência (Hz)')
plt.ylabel('Magnitude (mV)')
plt.legend()
plt.grid(True)
plt.xlim(0, 500)
plt.tight_layout()
plt.savefig('emg_ffts.svg', dpi=300)
plt.show()
