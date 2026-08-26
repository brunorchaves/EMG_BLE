# EMG_BLE — Project Instructions

nRF52840 sEMG sensor firmware (BLE) — see `README.md` for hardware, BLE
protocol, and signal-processing details.

## Build & flash: always use the SEGGER CLI

This project is built in **SEGGER Embedded Studio** and programmed with a
**J-Link** probe. Build and flash automation must go through SEGGER's own
CLI tools — `emBuild.exe` and `JLink.exe` (J-Link Commander) — never
`nrfjprog` and never the SES GUI. This is a firm project convention, not a
suggestion: if a SEGGER tool can't be found, ask the user for its install
path instead of substituting a different toolchain.

Use the `build-flash-nrf52` skill for any build/flash/erase/RTT-log task —
it wraps this in ready-made scripts under
`.claude/skills/build-flash-nrf52/scripts/`. The `firmware-builder` agent
(`.claude/agents/firmware-builder.md`) drives this skill end-to-end; the
`ble-log-analyst` agent (`.claude/agents/ble-log-analyst.md`) diagnoses
device behavior from RTT/UART logs afterward.

Two other firmware trees in this repo (`esp_dongle_ble/` — ESP-IDF for the
ESP32-C3 BLE dongle, `emgBLE/` — an Arduino sketch) use their own native
toolchains (`idf.py`, Arduino) and are out of scope for the SEGGER-CLI rule.

## Power/current measurement: use the PPK2 (PCA63100)

Redoing the energy-consumption study (README.md, Tabela 2) or any other
current/power measurement on the EMG_BLE board is done with the **Nordic
Power Profiler Kit II** (board PCA63100), not a shunt + oscilloscope. Use
the `ppk2-power-profiling` skill — it documents the wiring (source vs.
ampere meter mode, where to tap the battery/boost/5V rail) and wraps the
capture in ready-made scripts under `power_profiling/`.
