"""Captura de corrente por estado de operacao do EMG_BLE usando a PPK2 (PCA63100).

Reproduz a metodologia da Tabela 2 do artigo (corrente RMS e potencia por
estado, com fonte fixa em 5,0 V): OFF, IDLE, CONNECTED, TRANSMITTING.

Modo "source" (recomendado para bater com a Tabela 2): a PPK2 alimenta a
placa diretamente (bateria/boost desconectados) e mede a corrente no mesmo
instrumento. Ligue VOUT da PPK2 no trilho de 5 V da placa e GND no GND.

Modo "ampere": a PPK2 so mede; ela fica em serie entre o boost e a placa
(boost+ -> VIN da PPK2, VOUT da PPK2 -> entrada da placa, GND comum). Use
para medir o consumo real com bateria/boost no circuito.

O script pede para você levar o sensor para cada estado (via app BLE /
observando os LEDs) e pressiona Enter para iniciar a captura daquele
estado. Para o estado OFF em modo source, a PPK2 corta a propria saida
(toggle_DUT_power) em vez de você desconectar fios.

Uso:
    python ppk2_capture.py --port COM8
    python ppk2_capture.py --port COM8 --mode ampere --states IDLE,CONNECTED,TRANSMITTING
"""

import argparse
import csv
import statistics
import time
from pathlib import Path

from ppk2_api.ppk2_api import PPK2_API

DEFAULT_STATES = ["OFF", "IDLE", "CONNECTED", "TRANSMITTING"]


def find_port():
    ports = PPK2_API.list_devices()
    if len(ports) == 1:
        return ports[0]
    if len(ports) > 1:
        raise SystemExit(f"Mais de uma PPK2 encontrada: {ports}. Use --port para escolher.")
    raise SystemExit(
        "PPK2 nao encontrada automaticamente. Veja a porta COM no Gerenciador "
        "de Dispositivos e informe com --port COMx."
    )


def capture(ppk2, duration_s, poll_period_s=0.1):
    samples_uA = []
    ppk2.start_measuring()
    t_end = time.monotonic() + duration_s
    try:
        while time.monotonic() < t_end:
            time.sleep(poll_period_s)
            raw = ppk2.get_data()
            if raw:
                readings, _bits = ppk2.get_samples(raw)
                samples_uA.extend(readings)
    finally:
        ppk2.stop_measuring()
    return samples_uA


def summarize(samples_uA):
    if not samples_uA:
        return {"n": 0, "mean_mA": 0.0, "rms_mA": 0.0}
    mean_uA = statistics.fmean(samples_uA)
    rms_uA = statistics.fmean(x * x for x in samples_uA) ** 0.5
    return {"n": len(samples_uA), "mean_mA": mean_uA / 1000, "rms_mA": rms_uA / 1000}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default=None, help="Porta serial da PPK2 (ex: COM8)")
    parser.add_argument("--voltage-mv", type=int, default=5000,
                         help="Tensao (mV) no modo source, ou tensao nominal do trilho no modo ampere "
                              "(usada so para calcular a potencia). Default 5000, igual ao artigo.")
    parser.add_argument("--mode", choices=["source", "ampere"], default="source",
                         help="'source': PPK2 alimenta e mede (bypass da bateria/boost, replica a Tabela 2). "
                              "'ampere': PPK2 so mede, boost/bateria seguem no circuito.")
    parser.add_argument("--states", default=",".join(DEFAULT_STATES),
                         help="Estados a capturar, separados por virgula.")
    parser.add_argument("--duration", type=float, default=10.0, help="Segundos de captura por estado.")
    parser.add_argument("--out", default="power_profiling/results", help="Diretorio de saida.")
    args = parser.parse_args()

    port = args.port or find_port()
    print(f"Conectando a PPK2 em {port}...")
    ppk2 = PPK2_API(port, timeout=1)
    if not ppk2.get_modifiers():
        raise SystemExit("Falha ao ler calibracao da PPK2 - confira a conexao USB e a porta.")

    if args.mode == "source":
        ppk2.use_source_meter()
        ppk2.set_source_voltage(args.voltage_mv)
    else:
        ppk2.use_ampere_meter()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    states = [s.strip() for s in args.states.split(",") if s.strip()]
    results = []

    for state in states:
        input(f"\n>> Coloque o sensor no estado '{state}' e pressione Enter para "
              f"capturar {args.duration:.0f} s...")

        if state.upper() == "OFF" and args.mode == "source":
            ppk2.toggle_DUT_power("OFF")
            samples = capture(ppk2, args.duration)
            ppk2.toggle_DUT_power("ON")
        else:
            if args.mode == "source":
                ppk2.toggle_DUT_power("ON")
            samples = capture(ppk2, args.duration)

        stats = summarize(samples)
        power_mw = stats["rms_mA"] * (args.voltage_mv / 1000)
        results.append({
            "estado": state,
            "corrente_mA": round(stats["rms_mA"], 4),
            "potencia_mW": round(power_mw, 4),
            "n_amostras": stats["n"],
        })

        csv_path = out_dir / f"{state.lower()}_raw_uA.csv"
        with csv_path.open("w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["amostra", "corrente_uA"])
            for i, v in enumerate(samples):
                writer.writerow([i, v])

        print(f"   {state}: {stats['rms_mA']:.3f} mA RMS | {power_mw:.2f} mW "
              f"({stats['n']} amostras -> {csv_path.name})")

    summary_path = out_dir / "resumo_tabela2.csv"
    with summary_path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["estado", "corrente_mA", "potencia_mW", "n_amostras"])
        writer.writeheader()
        writer.writerows(results)

    print(f"\nResumo salvo em {summary_path}\n")
    print(f"{'Estado':<15}{'Corrente (mA)':>15}{'Potencia (mW)':>16}")
    for r in results:
        print(f"{r['estado']:<15}{r['corrente_mA']:>15.3f}{r['potencia_mW']:>16.2f}")


if __name__ == "__main__":
    main()
