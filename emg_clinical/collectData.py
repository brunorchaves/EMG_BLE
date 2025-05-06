import serial
import csv
import time

# Configurações da porta serial
SERIAL_PORT = 'COM55'       # Altere para a porta correta no seu sistema
BAUD_RATE = 115200
DURATION_SECONDS = 20
EXPECTED_SAMPLE_RATE_HZ = 1000
MAX_SAMPLES = DURATION_SECONDS * EXPECTED_SAMPLE_RATE_HZ

def main():
    filename = input("Digite o nome do arquivo CSV (ex: dados_emg.csv): ").strip()
    if not filename.endswith('.csv'):
        filename += '.csv'

    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"Lendo dados da porta {SERIAL_PORT} por {DURATION_SECONDS} segundos...")
        print(f"Salvando em: {filename}")

        with open(filename, mode='w', newline='') as file:
            writer = csv.writer(file)
            count = 0
            start_time = time.time()

            while count < MAX_SAMPLES:
                line = ser.readline().decode('utf-8').strip()
                if line.isdigit():
                    writer.writerow([line])
                    count += 1
                    print(f"{count}: {line}")

            print(f"\nColeta finalizada: {count} amostras salvas.")

    except Exception as e:
        print(f"Erro: {e}")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

if __name__ == "__main__":
    main()
