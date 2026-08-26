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
  user for their install path or set `EMBUILD_EXE`. SES **is installed** on
  this dev machine (`SEGGER Embedded Studio 8.30a`, licensed for Nordic
  devices) — `resolve_embuild` globs `SEGGER/SEGGER Embedded Studio */bin/`
  so any installed version resolves without an env var.
- `JLink.exe` not found → the J-Link Software pack isn't installed or isn't
  on PATH; set `JLINK_EXE`. Do not suggest `nrfjprog` as a substitute.
  `resolve_jlink`/`resolve_rtt_logger` in `common.sh` also glob for the
  version-suffixed install folder Windows creates by default
  (`SEGGER/JLink_V970`, `JLink_V978`, ...), not just the unversioned
  `SEGGER/JLink` path — no env var needed once that install exists.
- J-Link Commander connects to the probe ("Connecting to J-Link ...O.K.")
  but fails to attach to the target CPU, often with repeated
  `WARNING: RESET (pin 15) high, but should be low` — on this project the
  actual cause was simply **the EMG board had no power**. The board's only
  supply right now is the PPK2 (see the `ppk2-power-profiling` skill); with
  the PPK2 VOUT enabled and the board's power LEDs lit, the exact same
  `JLink.exe -device NRF52840_XXAA -if SWD -autoconnect 1` connected cleanly
  (found SW-DP, AHB-AP, Cortex-M4). Before assuming readback protection or a
  wiring fault on the debug header, confirm the board is actually powered.
- Flash fails with a connect/handshake error (board confirmed powered) → the
  board may have readback protection or a corrupted image; run `erase.sh`
  first, then `flash_all.sh`.
- App runs but never advertises/connects → re-check the SoftDevice is
  actually present (`flash_softdevice.sh`) before assuming an app bug; see
  the RAM allocation and connection-parameter notes in the root `README.md`.
- Any `.exe` invoked from these scripts reports "Failed to open file" on a
  path that clearly exists → this repo is driven from git-bash/MSYS, so
  `pwd`-derived paths look like `/c/Users/...`. `emBuild.exe`, `JLink.exe`
  and `JLinkRTTLogger.exe` are native Windows binaries and can't resolve
  that form. Every script routes file paths (project file, hex, RTT output)
  through `to_win_path()` in `common.sh` (uses `cygpath -w`) before handing
  them to a `.exe` — if you add a new script or an inline one-off command,
  do the same, or it will silently fail to load the file.

## Installing the SEGGER tools from scratch

Neither SES nor the J-Link Software pack ship with a plain installer
download — both gate the real binary behind an HTML "accept license" page.

- **J-Link Software pack**: works via script. `GET` the installer URL
  (e.g. `https://www.segger.com/downloads/jlink/JLink_Windows_x86_64.exe`)
  returns the gate page; scrape its `<form method="post" action="...">` and
  `POST accept_license_agreement=accepted&submit=Download+software` to that
  same URL to get the real (~70+ MB) NSIS installer. Run it with `/S` for a
  silent install (`Start-Process -ArgumentList "/S" -Wait`) — this installs
  fine as a non-admin user. **The USB driver for the probe does not get
  installed this way** — Device Manager shows the J-Link
  (`USB\VID_1366&PID_0101`) with a driver error until you separately run
  `<install dir>\USBDriver\InstDrivers.exe`, which requires a UAC
  (administrator) prompt a human has to click through; it cannot be
  automated headlessly.
- **SEGGER Embedded Studio**: the same POST-accept trick does **not** work
  — `segger.com/downloads/embedded-studio/...exe` sits behind a CloudFront
  distribution that rejects POST outright (`403`, "distribution ... supports
  only cachable requests"), for both the current and version-pinned (e.g.
  `v824`) URLs. This part genuinely needs a human: download the installer
  manually from a browser (the user did, getting `v830a`), then run it
  (its NSIS installer also supports `/S` for silent install if scripting it).
  Separately, using SES for this Nordic project needs a free per-user
  license, requested at https://license.segger.com/Nordic.cgi with the
  machine's MAC address (`Get-NetAdapter` lists candidates — prefer a
  physical adapter, not a virtual Hyper-V/WSL one) and the user's email.
  Nordic emails back a `License_SES_...` activation-key string. **Installing
  that key does not require the SES GUI**: run
  `<SES install dir>/bin/emLicense.exe install '<the License_SES_... string>'`
  (quote it — the key contains `%`, `/`, `+`, `=`) and `emLicense.exe list`
  to confirm (`MAC ... (OK)`, expiry date, licensee). This is how it was
  activated on this dev machine, entirely from the CLI.
