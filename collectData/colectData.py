import serial
import time
import csv

# === CONFIGURAÇÕES ===
PORTA_SERIAL = 'COM57'         # Altere para a porta correta
BAUDRATE = 115200
TEMPO_COLETA = 10              # segundos
ARQUIVO_SAIDA = 'biceps_10s_7.csv'

def ler_serial_por_5s(ser):
    print(f"\n[INFO] Coletando dados por {TEMPO_COLETA} segundos...")
    dados = []
    t0 = time.time()

    while (time.time() - t0) < TEMPO_COLETA:
        linha = ser.readline().decode(errors='ignore').strip()
        if linha:
            try:
                valor = int(linha)
                dados.append(valor)
            except ValueError:
                pass  # ignora linhas inválidas

    print(f"[OK] Coleta finalizada ({len(dados)} amostras).")
    return dados

def salvar_csv(valores, nome_arquivo):
    with open(nome_arquivo, mode='w', newline='') as file:
        writer = csv.writer(file)
        writer.writerow(['emg_value'])
        for val in valores:
            writer.writerow([val])
    print(f"[SALVO] Dados salvos em: {nome_arquivo}")

def main():
    try:
        with serial.Serial(PORTA_SERIAL, BAUDRATE, timeout=1) as ser:
            print(f"[ABERTO] Porta {PORTA_SERIAL} pronta.")
            while True:
                input("\n>>> Pressione Enter para iniciar a coleta de 5 segundos...")
                dados = ler_serial_por_5s(ser)
                salvar_csv(dados, ARQUIVO_SAIDA)
    except serial.SerialException as e:
        print(f"[ERRO] Falha ao abrir a porta serial: {e}")

if __name__ == '__main__':
    main()
