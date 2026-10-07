"""Controla o player pela serial do ST-LINK (pyserial).

    python stm32_dac_player/tools/player_cli.py info
    python stm32_dac_player/tools/player_cli.py "sine 100" "amp 1400mv" status
    python stm32_dac_player/tools/player_cli.py --linearity --dwell 10

--linearity faz o passo 4 do Ensaio 1 (TRES_ENSAIOS_SINAL): senoide de
100 Hz em 10 amplitudes de -40 a -3 dBFS, cada uma por --dwell segundos,
imprimindo o instante de cada troca para casar com a gravacao BLE.
A porta e detectada pelo VID da ST (0x0483); --port forca uma.
"""

from __future__ import annotations

import argparse
import sys
import time

import numpy as np
import serial
from serial.tools import list_ports

ST_VID = 0x0483


def find_port() -> str:
    ports = [p for p in list_ports.comports() if p.vid == ST_VID]
    if not ports:
        sys.exit("nenhuma porta do ST-LINK encontrada (VID 0x0483); use --port")
    if len(ports) > 1:
        print("varias portas ST-LINK, usando " + ports[0].device, file=sys.stderr)
    return ports[0].device


def send(ser: serial.Serial, cmd: str, wait_s: float = 0.3) -> str:
    ser.reset_input_buffer()
    ser.write((cmd + "\r").encode())
    time.sleep(wait_s)
    return ser.read(ser.in_waiting or 1).decode(errors="replace").replace("\r", "")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("commands", nargs="*")
    ap.add_argument("--port")
    ap.add_argument("--linearity", action="store_true")
    ap.add_argument("--dwell", type=float, default=10.0)
    ap.add_argument("--freq", type=float, default=100.0)
    args = ap.parse_args()

    with serial.Serial(args.port or find_port(), 115200, timeout=0.5) as ser:
        for cmd in args.commands:
            print(f"> {cmd}")
            print(send(ser, cmd).strip())
        if args.linearity:
            print(send(ser, f"sine {args.freq:g}").strip())
            for db in np.linspace(-40.0, -3.0, 10):
                t = time.time()
                out = send(ser, f"amp {db:.1f}db").strip().splitlines()
                print(f"{t:.3f}  amp {db:6.1f} dBFS  | {out[-1] if out else ''}", flush=True)
                time.sleep(max(0.0, args.dwell - (time.time() - t)))
            print(send(ser, "silence").strip())


if __name__ == "__main__":
    main()
