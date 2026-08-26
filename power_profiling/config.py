"""Constantes e configuração compartilhada da bancada de caracterização de energia.

Nada de hardware aqui - só constantes, o enum de estados e o parser que lê os
parâmetros reais do firmware (main.c / sdk_config.h) para embutir no
run_meta.json de cada run, tornando cada run auto-documentado.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Any

# --- PPK2 -------------------------------------------------------------

PPK2_PORT_DEFAULT = "COM8"
PPK2_FS_NOMINAL = 100_000  # amostras/s nominal da PPK2
PPK2_SAMPLE_BYTES = 4  # cada amostra de corrente é uma palavra de 32 bits
SOURCE_MV_DEFAULT = 5000

# Layout de bits da palavra de 32 bits (confirmado contra o parser oficial da
# Nordic, pc-nrfconnect-ppk/src/device/serialDevice.ts). ppk2-api só decodifica
# MEAS_ADC/MEAS_RANGE/MEAS_LOGIC e descarta o contador - por isso decodificamos
# a palavra bruta nós mesmos em ppk2_decode.py.
MASK_ADC = 0x3FFF          # bits 0-13
POS_RANGE, MASK_RANGE = 14, 0x7        # bits 14-16
POS_COUNTER, MASK_COUNTER = 18, 0x3F   # bits 18-23 (contador de 6 bits, module 64)
POS_LOGIC, MASK_LOGIC = 24, 0xFF       # bits 24-31 (D0-D7)

ADC_MULT = 1.8 / 163840  # mesma constante usada pelo ppk2-api / firmware oficial

# --- BLE / sensor -------------------------------------------------------

DEVICE_NAME = "EMG_BLE"
SVC_UUID = "19b10001-1000-e8f2-537e-4f6cd168a114"
EMG_UUID = "19b10002-1000-e8f2-537e-4f6cd168a114"
GAIN_UUID = "19b10003-1000-e8f2-537e-4f6cd168a114"

EMG_SAMPLES_PER_PACKET = 60
EMG_PAYLOAD_BYTES = EMG_SAMPLES_PER_PACKET * 2  # int16 LE, 120 bytes
MIN_REQUIRED_MTU = EMG_PAYLOAD_BYTES + 3  # ATT header = 3 bytes -> 123

# --- Bateria / autonomia -------------------------------------------------

BATTERY_MAH = 400.0
BATTERY_V = 3.7
BOOST_ETA_DEFAULT = 0.85


class State(str, Enum):
    """Sequência de estados percorrida pelo orquestrador.

    Mapeamento para os 4 estados do artigo:
      ADVERTISING    -> IDLE
      CONNECTED_IDLE -> CONNECTED (conectado, CCCD NAO habilitado)
      STREAMING      -> TRANSMITTING (CCCD habilitado, notificando)
      OFF / OFF_FINAL -> OFF
    Os demais (BOOT, CONNECTING, CONNECTED_IDLE_2, DISCONNECT,
    RE_ADVERTISING) não existem no artigo - são a parte "muito mais
    detalhamento" do pedido original.
    """

    OFF = "OFF"
    BOOT = "BOOT"
    ADVERTISING = "ADVERTISING"
    CONNECTING = "CONNECTING"
    CONNECTED_IDLE = "CONNECTED_IDLE"
    STREAMING = "STREAMING"
    CONNECTED_IDLE_2 = "CONNECTED_IDLE_2"
    DISCONNECT = "DISCONNECT"
    RE_ADVERTISING = "RE_ADVERTISING"
    OFF_FINAL = "OFF_FINAL"


# Código de 1..10 para cada estado, usado na codificação de marcadores GPIO
# (Fase 2 - build instrumentado). Mantido aqui mesmo sem firmware instrumentado
# ainda, para já fixar o mapeamento.
STATE_CODE: dict[State, int] = {s: i + 1 for i, s in enumerate(State)}

# Mapeamento para a nomenclatura da Tabela 2 do artigo (usado por compat.py)
LEGACY_STATE_MAP: dict[str, str] = {
    State.OFF.value: "off",
    State.ADVERTISING.value: "idle",
    State.CONNECTED_IDLE.value: "connected",
    State.STREAMING.value: "transmitting",
}

# Duração default de cada banda, em segundos. Deliberadamente um número PAR
# de segundos em todo lugar (mitigação de graça para o confound do LED de
# 1 Hz / 2 s de período - main.c:102-105,706): um número inteiro de períodos
# completos de LED torna o RMS da banda insensível à fase do LED.
DEFAULT_BAND_DURATIONS_S: dict[State, float] = {
    State.OFF: 14.0,
    State.BOOT: 4.0,
    State.ADVERTISING: 30.0,
    State.CONNECTING: 0.0,  # duração real = o que a conexão levar
    State.CONNECTED_IDLE: 10.0,  # curto de propósito - ver risco R7 (bug de overflow)
    State.STREAMING: 30.0,
    State.CONNECTED_IDLE_2: 6.0,
    State.DISCONNECT: 0.0,
    State.RE_ADVERTISING: 20.0,
    State.OFF_FINAL: 10.0,
}

GAIN_SWEEP_LEVELS: list[int] = [10, 1, 3, 5, 8, 10]
GAIN_SWEEP_DWELL_S = 4.0

# --- Firmware: parser dos parâmetros reais -------------------------------

_FW_PATTERNS: dict[str, re.Pattern[str]] = {
    "app_adv_interval": re.compile(r"#define\s+APP_ADV_INTERVAL\s+(\d+)"),
    "min_conn_interval_units": re.compile(
        r"MIN_CONN_INTERVAL\s+MSEC_TO_UNITS\(([\d.]+)"
    ),
    "max_conn_interval_units": re.compile(
        r"MAX_CONN_INTERVAL\s+MSEC_TO_UNITS\(([\d.]+)"
    ),
    "emg_packet_size": re.compile(r"#define\s+EMG_PACKET_SIZE\s+(\d+)"),
}

_SDK_PATTERNS: dict[str, re.Pattern[str]] = {
    "nrf_log_enabled": re.compile(r"#define\s+NRF_LOG_ENABLED\s+(\d+)"),
    "gatt_max_mtu_size": re.compile(r"#define\s+NRF_SDH_BLE_GATT_MAX_MTU_SIZE\s+(\d+)"),
    "gap_data_length": re.compile(r"#define\s+NRF_SDH_BLE_GAP_DATA_LENGTH\s+(\d+)"),
    "dcdc_default": re.compile(r"#define\s+POWER_CONFIG_DEFAULT_DCDCEN\s+(\d+)"),
    "gatts_attr_tab_size": re.compile(r"#define\s+NRF_SDH_BLE_GATTS_ATTR_TAB_SIZE\s+(\d+)"),
}


def _grep_first(pattern: re.Pattern[str], text: str) -> str | None:
    m = pattern.search(text)
    return m.group(1) if m else None


def parse_firmware_config(main_c: Path, sdk_config_h: Path) -> dict[str, Any]:
    """Extrai os parâmetros reais do firmware (não os documentados no README).

    Devolve um dict simples e serializável, pronto pra ir dentro do
    run_meta.json de cada run - assim cada run é auto-documentado e
    ``analyze.compare_runs`` consegue apontar qual parâmetro mudou entre
    "antes" e "depois" de uma otimização.
    """
    result: dict[str, Any] = {"source": {"main_c": str(main_c), "sdk_config_h": str(sdk_config_h)}}

    if main_c.exists():
        text = main_c.read_text(encoding="utf-8", errors="replace")
        for key, pat in _FW_PATTERNS.items():
            result[key] = _grep_first(pat, text)
        if result.get("app_adv_interval"):
            result["adv_interval_ms"] = round(int(result["app_adv_interval"]) * 0.625, 2)
    else:
        result["main_c_missing"] = True

    if sdk_config_h.exists():
        text = sdk_config_h.read_text(encoding="utf-8", errors="replace")
        for key, pat in _SDK_PATTERNS.items():
            result[key] = _grep_first(pat, text)
    else:
        result["sdk_config_h_missing"] = True

    return result


@dataclass
class RunPlanItem:
    state: State
    duration_s: float
    notes: str = ""


@dataclass
class RunPlan:
    """Sequência de estados a percorrer num run. Ver run_bench.default_plan()."""

    items: list[RunPlanItem] = field(default_factory=list)
    n_cycles: int = 1
    gain_sweep_levels: list[int] = field(default_factory=lambda: list(GAIN_SWEEP_LEVELS))
    gain_sweep_dwell_s: float = GAIN_SWEEP_DWELL_S
