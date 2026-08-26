"""Teste rápido de comunicação com a PPK2 (PCA63100) via USB.

Não precisa de nada ligado nos terminais VIN/VOUT/GND - só verifica se a
porta serial responde, se a calibração é lida corretamente e se dá para
iniciar/parar uma medição. Útil para validar a placa antes de ligá-la no
circuito do EMG_BLE.

Uso:
    python ppk2_check.py [--port COM8]
"""

import argparse
import sys
import time

from ppk2_api.ppk2_api import PPK2_API


def find_port():
    ports = PPK2_API.list_devices()
    if len(ports) == 1:
        return ports[0]
    if len(ports) > 1:
        raise SystemExit(f"Mais de uma PPK2 encontrada: {ports}. Use --port para escolher.")
    raise SystemExit(
        "PPK2 nao encontrada automaticamente (comum no Windows sem o driver "
        "USB da Nordic instalado). Veja a porta COM no Gerenciador de "
        "Dispositivos (procure 'USB\\VID_1915&PID_C00A') e informe com --port COMx."
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default=None, help="Porta serial da PPK2 (ex: COM8)")
    args = parser.parse_args()

    port = args.port or find_port()
    print(f"Abrindo {port}...")
    ppk2 = PPK2_API(port, timeout=1)

    if not ppk2.get_modifiers():
        print("Falha ao ler metadados/calibracao da PPK2.")
        sys.exit(1)

    print("Metadados OK.")
    print("  HW:", ppk2.modifiers.get("HW"))
    print("  Calibrated:", ppk2.modifiers.get("Calibrated"))

    ppk2.use_source_meter()
    ppk2.set_source_voltage(5000)
    ppk2.start_measuring()
    time.sleep(0.5)
    raw = ppk2.get_data()
    ppk2.stop_measuring()

    samples, _bits = ppk2.get_samples(raw) if raw else ([], [])
    print(f"Amostras lidas em 0.5s: {len(samples)}")
    print("COMUNICACAO OK - placa pronta para uso.")


if __name__ == "__main__":
    main()
