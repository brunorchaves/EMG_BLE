"""Orquestrador da bancada de caracterização de energia.

Percorre a sequência de estados (config.State) em UM stream contínuo da
PPK2 - nunca para de medir entre bandas, porque é isso que torna a timeline
inteira válida e o traço final um único gráfico "Fig. 7 e muito mais".

Uso:
    python run_bench.py --port COM8 --out power_profiling/runs
    python run_bench.py --port COM8 --quick   # plano curto p/ testar a
                                                 # sequenciação sem gastar
                                                 # minutos reais
"""

from __future__ import annotations

import argparse
import asyncio
import json
import shutil
import sys
import time
from dataclasses import asdict
from pathlib import Path

import config
from ble_client import EmgBleClient
from ppk2_stream import Ppk2Stream, StreamConfig
from timeline import Clock, EventLog


def default_plan(quick: bool = False) -> list[tuple[config.State, float]]:
    """Sequência de estados e durações. `quick=True` encolhe tudo pra
    segundos, só para validar a sequenciação/teardown sem gastar minutos
    reais de captura."""
    durations = dict(config.DEFAULT_BAND_DURATIONS_S)
    if quick:
        durations = {k: (2.0 if v > 0 else 0.0) for k, v in durations.items()}
    return [(s, durations[s]) for s in config.State]


def preflight(cfg: StreamConfig, out_dir: Path, plan: list[tuple[config.State, float]]) -> dict:
    """P1-P8 do plano: falha rápido e com mensagem clara em vez de gastar
    minutos de captura para descobrir um problema evitável."""
    problems: list[str] = []

    try:
        import bleak  # noqa: F401
        import numpy  # noqa: F401
        import serial  # noqa: F401
        from ppk2_api.ppk2_api import PPK2_API  # noqa: F401
    except ImportError as e:
        problems.append(f"dependencia faltando: {e}")

    total_s = sum(d for _, d in plan)
    est_bytes = total_s * 400_000
    free = shutil.disk_usage(out_dir.parent if out_dir.exists() else Path.cwd()).free
    if free < 1.5 * est_bytes:
        problems.append(
            f"espaco em disco insuficiente: {free/1e6:.0f} MB livres, "
            f"estimativa de {est_bytes/1e6:.0f} MB para este run"
        )

    import psutil  # type: ignore

    jlink_attached = False
    try:
        for p in psutil.process_iter(["name"]):
            if p.info["name"] and "jlink" in p.info["name"].lower():
                jlink_attached = True
    except Exception:
        pass  # psutil pode nao estar instalado; nao bloqueia o preflight

    return {"problems": problems, "jlink_attached": jlink_attached, "estimated_bytes": est_bytes}


async def run_bench(
    cfg: StreamConfig,
    plan: list[tuple[config.State, float]],
    out_dir: Path,
    input_condition: str = "shorted_10k",
    n_cycles: int = 1,
    gain_sweep: bool = True,
) -> Path:
    out_dir.mkdir(parents=True, exist_ok=True)
    pf = preflight(cfg, out_dir, plan)
    if pf["problems"]:
        raise RuntimeError("Preflight falhou: " + "; ".join(pf["problems"]))
    if pf["jlink_attached"]:
        print(
            "AVISO: JLink.exe detectado rodando - uma sessao de debug ativa infla o "
            "consumo do nRF52840. Desconecte o cabo SWD para um run 'oficial' "
            "(ver risco R3 do plano). Continuando mesmo assim - marcado em run_meta.json.",
            file=sys.stderr,
        )

    stream = Ppk2Stream(cfg, out_dir)
    calib = stream.open()
    stream.dut_power(False)

    clock = Clock.start_now()
    log = EventLog()
    events_path = out_dir / "events.jsonl"
    events_written = 0

    def _flush_log():
        nonlocal events_written
        events_written = log.flush_new_to_jsonl(events_path, events_written)

    def on_ble_event(kind: str, detail: dict) -> None:
        log.add(f"ble.{kind}", t=clock.now(), source="host", unc_ms=15.0, **detail)
        _flush_log()

    ble = EmgBleClient(clock, on_ble_event)

    firmware_config = config.parse_firmware_config(
        Path("emg_nrf_ses/project/ble_peripheral/ble_app_blinky/main.c"),
        Path("emg_nrf_ses/project/ble_peripheral/ble_app_blinky/pca10056/s140/config/sdk_config.h"),
    )

    run_meta = {
        "schema_version": 1,
        "t0_iso": clock.t0_iso,
        "input_condition": input_condition,
        "jlink_attached": pf["jlink_attached"],
        "stream_config": asdict(cfg),
        "ppk2_calibration": calib,
        "firmware_config": firmware_config,
        "plan": [(s.value, d) for s, d in plan],
        "n_cycles": n_cycles,
    }
    (out_dir / "run_meta.json").write_text(json.dumps(run_meta, indent=2, default=str), encoding="utf-8")

    t0 = stream.start()
    log.add("stream.start", t=t0, source="ppk2_ctl", unc_ms=1.0)
    _flush_log()

    all_packets = []
    try:
        for cycle in range(n_cycles):
            for state, duration in plan:
                t_ppk2 = None
                if state in (config.State.OFF, config.State.OFF_FINAL):
                    t_ppk2 = stream.dut_power(False)
                elif state == config.State.BOOT:
                    t_ppk2 = stream.dut_power(True)

                t_enter = t_ppk2 if t_ppk2 is not None else clock.now()
                log.add(
                    "state.enter",
                    t=t_enter,
                    state=state.value,
                    source="ppk2_ctl" if t_ppk2 is not None else "host",
                    unc_ms=1.0 if t_ppk2 is not None else 15.0,
                    cycle=cycle,
                )
                _flush_log()

                if state == config.State.CONNECTING:
                    try:
                        dev = await ble.scan(timeout=15.0)
                        await ble.connect(dev, use_cached_services=False, timeout=20.0)
                        mtu = await ble.read_mtu(poll_s=2.0)
                        if mtu < config.MIN_REQUIRED_MTU:
                            log.add(
                                "error",
                                t=clock.now(),
                                detail={"reason": f"MTU {mtu} < {config.MIN_REQUIRED_MTU}"},
                            )
                            raise RuntimeError(
                                f"MTU negociado ({mtu}) menor que o necessario "
                                f"({config.MIN_REQUIRED_MTU}) para o payload de 120 bytes."
                            )
                    except Exception as e:  # noqa: BLE001
                        log.add("error", t=clock.now(), detail={"where": "connecting", "error": str(e)})
                        raise

                elif state == config.State.STREAMING:
                    await ble.subscribe()
                    stream_end = time.monotonic() + duration
                    if gain_sweep and cfg.mode == "source":
                        for level in config.GAIN_SWEEP_LEVELS:
                            await ble.write_gain(level)
                            await asyncio.sleep(config.GAIN_SWEEP_DWELL_S)
                    remaining = stream_end - time.monotonic()
                    if remaining > 0:
                        await asyncio.sleep(remaining)
                    all_packets.extend(ble.drain_packets())

                elif state == config.State.CONNECTED_IDLE_2:
                    await ble.unsubscribe()
                    all_packets.extend(ble.drain_packets())
                    await asyncio.sleep(duration)

                elif state == config.State.DISCONNECT:
                    await ble.disconnect()

                else:
                    await asyncio.sleep(max(0.0, duration))
                    if state == config.State.CONNECTED_IDLE:
                        all_packets.extend(ble.drain_packets())

        log.add("state.enter", t=clock.now(), state="RUN_END", source="host", unc_ms=15.0)
        _flush_log()

    finally:
        health = stream.stop()
        stream.close()
        (out_dir / "stream_health.json").write_text(
            json.dumps(asdict(health) if health.thread_error is None else {**asdict(health), "thread_error": str(health.thread_error)}, indent=2),
            encoding="utf-8",
        )

    import numpy as np

    if all_packets:
        np.savez(
            out_dir / "emg_packets.npz",
            t_os=np.array([p.t_os if p.t_os is not None else np.nan for p in all_packets]),
            t_host=np.array([p.t_host for p in all_packets]),
            n_bytes=np.array([p.n_bytes for p in all_packets]),
            gain_level=np.array([p.gain_level for p in all_packets]),
            samples=np.array([p.samples for p in all_packets if len(p.samples) == config.EMG_SAMPLES_PER_PACKET] or [[]]),
        )

    ble_stats = ble.stats()
    (out_dir / "ble_stats.json").write_text(json.dumps(asdict(ble_stats), indent=2), encoding="utf-8")

    print(f"Run gravado em {out_dir}")
    print(f"  amostras de corrente: ~{health.samples_est:,}")
    print(f"  pacotes EMG recebidos: {len(all_packets)}  (MTU={ble_stats.mtu})")
    if health.overflow_suspected:
        print("  AVISO: possivel overflow do buffer serial durante a captura - ver stream_health.json")

    return out_dir


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default=config.PPK2_PORT_DEFAULT)
    parser.add_argument("--voltage-mv", type=int, default=config.SOURCE_MV_DEFAULT)
    parser.add_argument("--out", default="power_profiling/runs")
    parser.add_argument("--quick", action="store_true", help="plano curto so p/ testar a sequenciacao")
    parser.add_argument("--cycles", type=int, default=1)
    parser.add_argument("--no-gain-sweep", action="store_true")
    parser.add_argument(
        "--input-condition",
        default="shorted_10k",
        choices=["shorted_10k", "injected_sine_100hz_1mvpp", "on_subject", "open"],
    )
    args = parser.parse_args()

    from datetime import datetime, timezone

    run_id = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H-%M-%SZ") + ("_quick" if args.quick else "")
    out_dir = Path(args.out) / run_id

    cfg = StreamConfig(port=args.port, source_mv=args.voltage_mv)
    plan = default_plan(quick=args.quick)

    asyncio.run(
        run_bench(
            cfg,
            plan,
            out_dir,
            input_condition=args.input_condition,
            n_cycles=args.cycles,
            gain_sweep=not args.no_gain_sweep,
        )
    )


if __name__ == "__main__":
    main()
