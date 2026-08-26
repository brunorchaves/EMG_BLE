---
name: ppk2-power-profiling
description: Measure current/power consumption of the EMG_BLE nRF52840 sensor using the Nordic Power Profiler Kit II (board PCA63100), replacing the shunt-resistor + oscilloscope setup used in the original paper. Use whenever the user asks to measure consumo/corrente/energia/potência, redo the power study, use the PPK2/PCA63100 board, or reproduce Table 2 (OFF/IDLE/CONNECTED/TRANSMITTING states) from the article.
---

# Power profiling with the PPK2 (PCA63100)

## What the PCA63100 is

PCA63100 is the board reference for the **Nordic Power Profiler Kit II
(PPK2)** — a USB instrument that measures the current a circuit ("DUT")
draws, from ~200 nA to 1 A. It has two modes:

- **Source Meter**: the PPK2 *supplies* a regulated voltage (0.8–5.0 V) to
  the DUT and measures the current itself — one instrument replaces a bench
  supply + shunt/oscilloscope.
- **Ampere Meter**: the DUT is powered by another source; the PPK2 sits in
  series and only measures the current flowing through.

The PPK2 talks to the PC over USB (control + data) via a virtual serial
port. The `VIN`/`VOUT`/`GND` screw terminals are for the **measured
circuit**, not for the PC connection.

## Why this exists

The article ("A Low-Power Bluetooth LE Surface EMG Sensor", SEB 2025)
reports current/power per operating state at a fixed 5.0 V supply
(README.md, Tabela 2):

| Estado | Corrente (mA) | Potência (mW) |
|---|---|---|
| OFF (0 V) | 0 | 0 |
| IDLE | 2.922 | 14.61 |
| CONNECTED | 8.497 | 42.49 |
| TRANSMITTING | 9.063 | 45.32 |

That measurement used a shunt resistor read on an oscilloscope. This skill
redoes it with the PPK2 instead, in `power_profiling/` at the repo root.

## EMG_BLE power path and where to tap in

The board's own supply chain is: **Li-Ion battery (3.7 V) → boost converter
→ 5 V rail** feeding the analog front-end (INA317, ADS112C04) and the
nRF52840.

**To reproduce Table 2 (fixed 5.0 V) — recommended:** disconnect the
battery/boost entirely and let the PPK2 be the 5 V source, in **Source
Meter** mode:
- PPK2 `VOUT` → the point where the boost's output used to feed the board
- PPK2 `GND` → board GND
- Software: source meter, 5000 mV

**To measure real consumption with the battery (autonomy estimate) —
complementary:** break the wire **between the boost output and the board**
(not between the battery and the boost) and insert the PPK2 in **Ampere
Meter** mode there:
- boost output (+) → PPK2 `VIN`
- PPK2 `VOUT` → board's 5 V input
- GND common between boost, PPK2, and board

**Do not tap the battery-to-boost wire** (3.7 V side) expecting Table-2-
comparable numbers — the boost changes both voltage and (via its
efficiency) current, so that reading isn't the same quantity. Only useful
for a battery-life-in-mAh estimate, and even then prefer measuring
post-boost first.

## Machine-specific facts (verified on this dev machine)

- PPK2 enumerates as `USB\VID_1915&PID_C00A` with two interfaces: a CDC-ACM
  serial port (used for everything) and a second interface that shows
  "Error"/no class in Device Manager — harmless, ignore it unless installing
  the official Nordic USB driver.
- On this machine the serial port is **COM8**. On Windows, without Nordic's
  own USB driver installed, the port's description is generic ("Dispositivo
  Serial USB"), so `PPK2_API.list_devices()` (which matches on the driver
  name `nRF Connect USB CDC ACM`) **will not find it** — always pass
  `--port COM8` explicitly (or the current port from Device Manager if it
  changes).
- Python package `ppk2-api` (PyPI, module `ppk2_api.ppk2_api`, class
  `PPK2_API`) is installed in
  `C:\Users\RIBB\AppData\Local\Programs\Python\Python312`. No Nordic
  desktop software is installed.
- Communication was verified end-to-end with nothing wired to
  VIN/VOUT/GND: `get_modifiers()` → `True`, source meter at 5000 mV,
  `start_measuring()`/`get_data()`/`stop_measuring()` all returned data
  (4096 samples in 0.5 s). `Calibrated: 0` in the metadata is normal —
  it just means factory calibration constants are used, not a fault.
- The very first sample after `start_measuring()` is often an outlier
  (startup transient) — the capture script doesn't filter it out
  explicitly; for short captures (a few seconds or more) it's negligible.
- **Gotcha confirmed by hand:** `set_source_voltage()` only arms the
  regulator — it does **not** enable the VOUT rail. Without an explicit
  `toggle_DUT_power("ON")` call afterward, VOUT reads 0 V on a multimeter
  and the DUT draws 0 mA, even though the mode/voltage look correctly
  configured. This is the equivalent of the "Enable power output" toggle in
  Nordic's own Power Profiler app. `ppk2_capture.py` already calls this for
  every non-`OFF` state; any ad-hoc/one-off script must call it too, right
  after `set_source_voltage()`.

## Tools already in the repo

Everything lives in `power_profiling/` (not under `.claude/`, so the user
can run it directly too):

| File | Purpose |
|---|---|
| `power_profiling/README.md` | Full wiring + usage instructions (Portuguese) |
| `power_profiling/ppk2_check.py` | Connectivity smoke test — no wiring needed |
| `power_profiling/ppk2_capture.py` | Walks through OFF/IDLE/CONNECTED/TRANSMITTING, captures N seconds per state, writes per-state raw CSVs and a `resumo_tabela2.csv` summary (current RMS in mA, power in mW) |

Typical invocations:

```bash
python power_profiling/ppk2_check.py --port COM8

python power_profiling/ppk2_capture.py --port COM8 --mode source --voltage-mv 5000

python power_profiling/ppk2_capture.py --port COM8 --mode ampere --voltage-mv 5000 \
    --states IDLE,CONNECTED,TRANSMITTING
```

`ppk2_capture.py` is interactive: it pauses before each state so the user
can put the sensor in that state (via the BLE app / observing LEDs) before
pressing Enter to start that state's capture.

## Diagnosing failures

- `PPK2_API.list_devices()` returns nothing → expected on this machine
  without the Nordic driver; pass `--port COMx` directly (check Device
  Manager for `VID_1915&PID_C00A`).
- `get_modifiers()` returns `False`/falsy → wrong port, port in use by
  another program (e.g. nRF Connect for Desktop open elsewhere), or a bad
  USB cable/hub. Close other serial monitors and retry.
- `start_measuring()` raises "Output voltage not set!" → call
  `set_source_voltage()` before `start_measuring()` in source mode (or
  `use_ampere_meter()` without setting voltage, in ampere mode).
- Readings near-zero with nothing wired to VOUT/GND is expected (open
  circuit draws no current) — that's what a healthy connectivity test looks
  like before physical wiring is done.
- For automatic state-boundary detection instead of manual timing, the PPK2
  has 8 digital trace channels (D0–D7); using them would require toggling a
  spare nRF52840 GPIO on each BLE state transition in firmware — not
  implemented yet, treat as a separate firmware task if requested.
