---
name: build-flash-nrf52
description: Build and flash (gravar) the EMG_BLE nRF52840 firmware using SEGGER's own CLI tools — emBuild.exe (SEGGER Embedded Studio) and JLink.exe (J-Link Commander). Use whenever the user asks to build, compile, program, flash, gravar, or "subir" the firmware onto the nRF52840 board, provision a new/blank board, or capture RTT debug logs. Do not use nrfjprog or the SES GUI for these tasks — this project standardizes on the SEGGER CLI.
---

# Build & Flash — nRF52840 (EMG_BLE)

## Rule: always use the SEGGER CLI

This project is built with **SEGGER Embedded Studio** and programmed with a
**J-Link** probe. All build and flash automation in this repo goes through
SEGGER's own command-line tools, never `nrfjprog` and never the SES GUI:

- **Build** → `emBuild.exe` (SEGGER Embedded Studio's CLI build tool)
- **Flash / erase / reset** → `JLink.exe` (J-Link Commander), driven with
  generated command files
- **Debug logs** → `JLinkRTTLogger.exe` (SEGGER RTT, over the same J-Link link)

If a tool isn't found at its default install path, set the matching env var
(`EMBUILD_EXE`, `JLINK_EXE`, `JLINK_RTT_LOGGER_EXE`) rather than falling back
to a different toolchain.

## Project layout

- SES project: `emg_nrf_ses/project/ble_peripheral/ble_app_blinky/pca10056/s140/ses/ble_app_blinky_pca10056_s140.emProject`
- Configs: `Debug`, `Release`
- Built app hex: `.../ses/Output/<Config>/Exe/ble_app_blinky_pca10056_s140.hex`
- SoftDevice hex (repo root): `s140_nrf52_7.2.0_softdevice.hex`
- Target device: `NRF52840_XXAA`, interface `SWD`

## Scripts (`scripts/`)

All scripts live in `.claude/skills/build-flash-nrf52/scripts/` and are
runnable directly (`bash scripts/<name>.sh ...`) since they resolve the repo
root relative to their own location.

| Script | Purpose |
|---|---|
| `build.sh [Debug\|Release]` | Rebuild the app via `emBuild.exe` (defaults to `Release`) |
| `flash_app.sh [Debug\|Release]` | Flash the already-built app hex via `JLink.exe` |
| `build_and_flash.sh [Debug\|Release]` | Day-to-day loop: build then flash the app |
| `flash_softdevice.sh` | Flash the S140 SoftDevice (once per device, or after erase) |
| `erase.sh` | Full chip erase via `JLink.exe` |
| `flash_all.sh [Debug\|Release]` | First-time provisioning: erase → SoftDevice → build → flash app |
| `rtt_log.sh [output_file] [seconds]` | Capture RTT debug output via `JLinkRTTLogger.exe` |

## Typical workflows

**Iterating on firmware changes (SoftDevice already on the board):**
```bash
bash .claude/skills/build-flash-nrf52/scripts/build_and_flash.sh Debug
```

**Brand-new or freshly erased board:**
```bash
bash .claude/skills/build-flash-nrf52/scripts/flash_all.sh Release
```

**Just watching logs after flashing:**
```bash
bash .claude/skills/build-flash-nrf52/scripts/rtt_log.sh rtt_log.txt
```

## Diagnosing failures

- `emBuild.exe` not found → SES isn't installed at the default path; ask the
  user for their install path or set `EMBUILD_EXE`.
- `JLink.exe` not found → the J-Link Software pack isn't installed or isn't
  on PATH; set `JLINK_EXE`. Do not suggest `nrfjprog` as a substitute.
- Flash fails with a connect/handshake error → the board may have readback
  protection or a corrupted image; run `erase.sh` first, then `flash_all.sh`.
- App runs but never advertises/connects → re-check the SoftDevice is
  actually present (`flash_softdevice.sh`) before assuming an app bug; see
  the RAM allocation and connection-parameter notes in the root `README.md`.
