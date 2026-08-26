"""Mantem o VOUT da PPK2 ligado continuamente (Source Meter) para permitir
checagem manual com multimetro / observacao de LEDs.

A PPK2 desliga o VOUT quando a conexao serial com o host e encerrada, por
isso este processo fica vivo (loop) em vez de sair logo apos ligar a saida.
Encerre com Ctrl+C (ou finalizando o processo) quando terminar de checar.

Uso:
    python ppk2_hold_on.py --port COM8 --voltage-mv 5000
"""

import argparse
import time

from ppk2_api.ppk2_api import PPK2_API


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM8")
    parser.add_argument("--voltage-mv", type=int, default=5000)
    args = parser.parse_args()

    ppk2 = PPK2_API(args.port, timeout=1)
    if not ppk2.get_modifiers():
        raise SystemExit("Falha ao ler calibracao da PPK2.")

    ppk2.use_source_meter()
    ppk2.set_source_voltage(args.voltage_mv)
    ppk2.toggle_DUT_power("ON")
    ppk2.start_measuring()
    print(f"VOUT ligado em {args.voltage_mv} mV. Mantendo ligado - Ctrl+C para sair.", flush=True)

    try:
        while True:
            time.sleep(2)
            raw = ppk2.get_data()
            if raw:
                samples, _bits = ppk2.get_samples(raw)
                if samples:
                    media_mA = (sum(samples) / len(samples)) / 1000
                    print(f"corrente media: {media_mA:.3f} mA", flush=True)
    except KeyboardInterrupt:
        pass
    finally:
        ppk2.stop_measuring()
        ppk2.toggle_DUT_power("OFF")
        print("VOUT desligado.", flush=True)


if __name__ == "__main__":
    main()
