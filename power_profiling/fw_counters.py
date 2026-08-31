"""Le os contadores de diagnostico do firmware via J-Link, sem depender do BLE.

Motivo de existir: um travamento do loop principal do firmware e
indistinguivel de "BLE conectado mas sem dados" quando visto de fora - a
pilha BLE responde por interrupcao da SoftDevice (conecta, negocia MTU,
processa CCCD) mesmo com o main() parado. Estes contadores (declarados em
main.c) resolvem isso: se g_loop_count e g_adc_ok_count avancam, o loop
principal e a aquisicao do ADC estao vivos, ponto.

Lendo duas vezes com um intervalo, tambem mede a taxa de amostragem
EFETIVA real do ADC - que neste firmware e ditada pelo tempo do I2C
bloqueante, nao pelo clock do ADC (ver PLANO.md).

Uso:
    python fw_counters.py                 # uma leitura
    python fw_counters.py --watch 10      # duas leituras 10s apart + taxas
"""

from __future__ import annotations

import argparse
import re
import subprocess
import tempfile
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
MAP_PATH = (
    REPO_ROOT
    / "emg_nrf_ses/project/ble_peripheral/ble_app_blinky/pca10056/s140/ses/Output/Release/Exe"
    / "ble_app_blinky_pca10056_s140.map"
)
JLINK_CANDIDATES = [
    Path(r"C:/Program Files/SEGGER/JLink_V970/JLink.exe"),
    Path(r"C:/Program Files/SEGGER/JLink/JLink.exe"),
]

# ordem importa: (nome, tamanho_em_bytes, com_sinal)
COUNTERS = [
    ("g_init_status", 4, False),
    ("g_last_filtered", 2, True),
    ("g_last_raw", 2, True),
    ("g_drdy_count", 4, False),
    ("g_block_drop_count", 4, False),
    ("g_notify_err_count", 4, False),
    ("g_notify_ok_count", 4, False),
    ("g_adc_fail_count", 4, False),
    ("g_adc_ok_count", 4, False),
    ("g_loop_count", 4, False),
    # aquisicao sob demanda: o ADC entra em power-down quando nao ha conexao
    ("g_acq_running", 4, False),
    ("g_acq_start_count", 4, False),
    ("g_acq_stop_count", 4, False),
    ("g_acq_error_count", 4, False),
]

RATE_COUNTERS = [
    "g_loop_count",
    "g_drdy_count",
    "g_adc_ok_count",
    "g_adc_fail_count",
    "g_notify_ok_count",
    "g_notify_err_count",
    "g_block_drop_count",
]

INIT_STATUS_BITS = {
    1: "twi_ok",
    2: "ads_init_ok",
    4: "ads_config_ok",
    8: "ds3502_ok",
    16: "main_loop_entered",
    32: "drdy_irq_ok",
    128: "ADC_INIT_FAILED",
}


class FwCountersError(RuntimeError):
    """Falha ao ler os contadores por J-Link.

    Deliberadamente NAO e SystemExit. Este modulo e usado como biblioteca pelo
    run_bench, que le os contadores apenas para ANOTAR a taxa de aquisicao -
    um dado auxiliar. Com SystemExit, um `except Exception` do chamador nao
    captura (SystemExit deriva de BaseException) e a falha do J-Link derrubava
    o ensaio inteiro antes de medir qualquer coisa. Ja aconteceu.
    """


def resolve_jlink() -> Path:
    for c in JLINK_CANDIDATES:
        if c.exists():
            return c
    for c in Path("C:/Program Files/SEGGER").glob("JLink_V*/JLink.exe"):
        return c
    raise FwCountersError("JLink.exe nao encontrado")


def symbol_addresses(map_path: Path) -> dict[str, int]:
    """Le os enderecos dos simbolos do .map em vez de hardcodar - eles mudam
    a cada build."""
    text = map_path.read_text(encoding="utf-8", errors="replace")
    out: dict[str, int] = {}
    for name, _size, _signed in COUNTERS:
        m = re.search(rf"^\s+(0x[0-9a-fA-F]{{8}})\s+{re.escape(name)}\s*$", text, re.MULTILINE)
        if m:
            out[name] = int(m.group(1), 16)
    return out


def read_memory(jlink: Path, addr: int, n_words: int) -> list[int]:
    """Le n_words palavras de 32 bits a partir de addr, via J-Link."""
    script = f"mem32 {addr:08X} {n_words}\nq\n"
    with tempfile.NamedTemporaryFile("w", suffix=".jlink", delete=False) as f:
        f.write(script)
        cmd_file = f.name
    try:
        proc = subprocess.run(
            [
                str(jlink), "-device", "NRF52840_XXAA", "-if", "SWD", "-speed", "4000",
                "-autoconnect", "1", "-NoGui", "1", "-CommandFile", cmd_file,
            ],
            capture_output=True, text=True, timeout=30,
        )
    finally:
        Path(cmd_file).unlink(missing_ok=True)

    words: list[int] = []
    for line in proc.stdout.splitlines():
        m = re.match(r"^\s*[0-9A-Fa-f]{8}\s*=\s*((?:[0-9A-Fa-f]{8}\s*)+)", line)
        if m:
            for w in m.group(1).split():
                words.append(int(w, 16))
    if not words:
        raise FwCountersError(
            "Nao consegui ler memoria via J-Link. A placa esta alimentada? "
            "(a PPK2 e a unica fonte)\n--- saida do JLink ---\n" + proc.stdout[-2000:]
        )
    return words


def decode(words: list[int], base_addr: int, addrs: dict[str, int]) -> dict:
    raw = b"".join(w.to_bytes(4, "little") for w in words)
    result: dict[str, int] = {}
    for name, size, signed in COUNTERS:
        if name not in addrs:
            continue
        off = addrs[name] - base_addr
        if off < 0 or off + size > len(raw):
            continue
        result[name] = int.from_bytes(raw[off : off + size], "little", signed=signed)
    return result


def describe_init_status(value: int) -> str:
    flags = [label for bit, label in INIT_STATUS_BITS.items() if value & bit]
    return ", ".join(flags) if flags else "(nenhum)"


def snapshot() -> dict:
    jlink = resolve_jlink()
    if not MAP_PATH.exists():
        raise FwCountersError(f".map nao encontrado em {MAP_PATH} - compile em Release primeiro")
    addrs = symbol_addresses(MAP_PATH)
    if not addrs:
        raise FwCountersError("nenhum contador encontrado no .map - o firmware tem os contadores?")
    base = min(addrs.values())
    span = max(addrs.values()) + 4 - base
    n_words = (span + 3) // 4
    words = read_memory(jlink, base, n_words)
    return decode(words, base, addrs)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--watch", type=float, default=0.0,
                        help="segundos entre duas leituras, para calcular taxas")
    args = parser.parse_args()

    snap1 = snapshot()
    print("=== contadores do firmware ===")
    print(f"  g_init_status      = 0x{snap1.get('g_init_status', 0):02X}  "
          f"[{describe_init_status(snap1.get('g_init_status', 0))}]")
    for name, _s, _sg in COUNTERS:
        if name == "g_init_status":
            continue
        print(f"  {name:20s} = {snap1.get(name)}")

    if args.watch > 0:
        print(f"\naguardando {args.watch:.0f}s...")
        time.sleep(args.watch)
        snap2 = snapshot()
        print("\n=== taxas medidas ===")
        for name in RATE_COUNTERS:
            d = snap2.get(name, 0) - snap1.get(name, 0)
            print(f"  {name:20s} +{d:<10d} -> {d / args.watch:.1f}/s")
        print(f"\n  g_last_raw agora   = {snap2.get('g_last_raw')}")
        print(f"  g_last_filtered    = {snap2.get('g_last_filtered')}")

        d_loop = snap2.get("g_loop_count", 0) - snap1.get("g_loop_count", 0)
        d_adc = snap2.get("g_adc_ok_count", 0) - snap1.get("g_adc_ok_count", 0)
        d_drdy = snap2.get("g_drdy_count", 0) - snap1.get("g_drdy_count", 0)

        # Distinguir "aquisicao pausada de proposito" de "firmware travado" e
        # essencial: nos dois casos o loop quase nao avanca e o ADC nao entrega
        # nada. Sem essa checagem, o power-down intencional pareceria o bug de
        # 1 amostra/s que existia antes.
        acq = snap2.get("g_acq_running")
        if acq == 0:
            print("\n  aquisicao: EM POWER-DOWN (esperado sem conexao BLE)")
            print(f"  loop principal vivo: {'SIM' if d_loop > 0 else 'dormindo (normal neste estado)'}")
            print(f"  start/stop/erros: {snap2.get('g_acq_start_count')} / "
                  f"{snap2.get('g_acq_stop_count')} / {snap2.get('g_acq_error_count')}")
            return
        print(f"\n  aquisicao: ATIVA")
        print(f"  loop principal vivo: {'SIM' if d_loop > 0 else 'NAO'}")
        print(f"  ADC entregando dado: {'SIM' if d_adc > 0 else 'NAO'}")
        print(f"  taxa de amostragem efetiva: {d_adc / args.watch:.1f} S/s")
        if d_drdy > 0:
            # se muitas conversoes do ADC nao viraram leitura, o loop nao esta
            # dando conta (o I2C bloqueante e o gargalo) e amostras sao perdidas
            print(f"  conversoes do ADC nao lidas: {d_drdy - d_adc} "
                  f"({(1 - d_adc / d_drdy) * 100:.1f}% perdidas)" if d_drdy >= d_adc else "")


if __name__ == "__main__":
    try:
        main()
    except FwCountersError as e:  # como CLI, falha e falha
        raise SystemExit(str(e))
