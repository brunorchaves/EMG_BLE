"""Fala com a CLI do player pela serial do ST-LINK (pyserial).

Sem argumento nenhum abre um terminal interativo - e o jeito normal de usar na
bancada. Digite os comandos do firmware (seg, status, amp, ...) e "quit" para
sair; "?" mostra os atalhos:

    python stm32_dac_player/tools/player_cli.py

Tambem da para mandar comandos de uma vez, para script e caderno de bancada:

    python stm32_dac_player/tools/player_cli.py info
    python stm32_dac_player/tools/player_cli.py "seg Walking" status
    python stm32_dac_player/tools/player_cli.py --linearity --dwell 10

--linearity faz o passo 4 do Ensaio 1 (TRES_ENSAIOS_SINAL): senoide de
100 Hz em 10 amplitudes de -40 a -3 dBFS, cada uma por --dwell segundos,
imprimindo o instante de cada troca para casar com a gravacao BLE.

A porta sai sozinha: no Windows pelo VID da ST (0x0483), no Linux/WSL por
/dev/ttyACM*. --port forca uma. ATENCAO: se o ST-LINK estiver anexado ao WSL
por usbipd, a porta COM nao existe no Windows (e vice-versa) - rode este
script do mesmo lado onde o dispositivo esta.
"""

from __future__ import annotations

import argparse
import glob
import sys
import time

import serial
from serial.tools import list_ports

ST_VID = 0x0483
PROMPT = "player> "


def find_port() -> str:
    ports = [p.device for p in list_ports.comports() if p.vid == ST_VID]
    if not ports:
        # no WSL o ttyACM costuma vir sem VID exposto
        ports = sorted(glob.glob("/dev/ttyACM*"))
    if not ports:
        sys.exit("nenhuma porta do ST-LINK encontrada.\n"
                 "  - a Nucleo esta no USB do ST-LINK (CN1)?\n"
                 "  - o dispositivo esta anexado ao WSL por usbipd? entao rode isto de dentro do WSL\n"
                 "  - ou passe --port COM15 / --port /dev/ttyACM0")
    if len(ports) > 1:
        print(f"varias portas ({', '.join(ports)}), usando {ports[0]}", file=sys.stderr)
    return ports[0]


def drain(ser: serial.Serial, wait_s: float = 0.25) -> str:
    """Le o que a placa mandar dentro de wait_s, sem bloquear no fim."""
    time.sleep(wait_s)
    out = b""
    while ser.in_waiting:
        out += ser.read(ser.in_waiting)
        time.sleep(0.05)
    return out.decode(errors="replace").replace("\r", "")


def send(ser: serial.Serial, cmd: str, wait_s: float = 0.3) -> str:
    ser.reset_input_buffer()
    ser.write((cmd + "\r").encode())
    return drain(ser, wait_s)


def clean(text: str, cmd: str | None = None) -> str:
    """Tira o prompt e o eco do firmware, que a gente ja mostrou na tela."""
    lines = [l for l in text.splitlines() if l.strip() not in ("", ">")]
    lines = [l[2:] if l.startswith("> ") else l for l in lines]
    if cmd and lines and lines[0].strip() == cmd.strip():
        lines = lines[1:]   # o firmware ecoa cada caractere digitado
    return "\n".join(lines)


def repl(ser: serial.Serial) -> None:
    print(f"conectado em {ser.port} @ {ser.baudrate}  -  'help' lista os comandos "
          f"do firmware, 'quit' sai")
    boot = drain(ser, 0.3)
    print(clean(send(ser, "status"), "status") or clean(boot))
    while True:
        try:
            line = input(PROMPT).strip()
        except (EOFError, KeyboardInterrupt):
            print()
            return
        if not line:
            # Enter vazio: so mostra o que a placa tiver falado (eventos)
            pending = drain(ser, 0.05)
            if pending.strip():
                print(clean(pending))
            continue
        if line in ("quit", "exit", "q"):
            return
        if line == "?":
            print("  <comando>   manda para o firmware (help lista todos)\n"
                  "  Enter       mostra eventos pendentes\n"
                  "  quit        sai (nao mexe no que a placa esta tocando)")
            continue
        # 'seg' e 'info' respondem mais, 'sweep' leva um instante para o 1o degrau
        wait = 0.6 if line.split()[0] in ("seg", "info", "help", "?") else 0.3
        out = clean(send(ser, line, wait), line)
        if out:
            print(out)


def linearity(ser: serial.Serial, freq: float, dwell: float) -> None:
    import numpy as np

    print(clean(send(ser, f"sine {freq:g}"), f"sine {freq:g}"))
    for db in np.linspace(-40.0, -3.0, 10):
        t = time.time()
        out = clean(send(ser, f"amp {db:.1f}db"), f"amp {db:.1f}db").splitlines()
        print(f"{t:.3f}  amp {db:6.1f} dBFS  | {out[-1] if out else ''}", flush=True)
        time.sleep(max(0.0, dwell - (time.time() - t)))
    print(clean(send(ser, "silence"), "silence"))


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("commands", nargs="*", help="comandos a mandar; sem nenhum, abre o terminal interativo")
    ap.add_argument("--port", help="COM15, /dev/ttyACM0, ...")
    ap.add_argument("--linearity", action="store_true")
    ap.add_argument("--dwell", type=float, default=10.0)
    ap.add_argument("--freq", type=float, default=100.0)
    args = ap.parse_args()

    port = args.port or find_port()
    try:
        ser = serial.Serial(port, 115200, timeout=0.5)
    except serial.SerialException as e:
        sys.exit(f"nao consegui abrir {port}: {e}\n"
                 "  outro terminal (screen, PuTTY) esta com a porta aberta?")
    with ser:
        if args.linearity:
            linearity(ser, args.freq, args.dwell)
        elif args.commands:
            for cmd in args.commands:
                print(f"> {cmd}")
                print(clean(send(ser, cmd, 0.6 if cmd.split()[0] in ("seg", "info") else 0.3), cmd))
        else:
            repl(ser)


if __name__ == "__main__":
    main()
