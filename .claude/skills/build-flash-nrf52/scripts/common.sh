#!/usr/bin/env bash
# Shared paths and tool-resolution helpers for the nRF52840 build/flash scripts.
#
# RULE: this project is built with SEGGER Embedded Studio and flashed with a
# J-Link probe, so all build/flash automation MUST go through SEGGER's own
# CLI tools (emBuild.exe, JLink.exe, JLinkRTTLogger.exe). Do NOT use nrfjprog
# or any GUI as the default path — those are fallbacks a human may reach for,
# not what scripts/agents here should invoke.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"

SES_PROJECT_DIR="$REPO_ROOT/emg_nrf_ses/project/ble_peripheral/ble_app_blinky/pca10056/s140/ses"
SES_PROJECT_FILE="$SES_PROJECT_DIR/ble_app_blinky_pca10056_s140.emProject"
APP_HEX_NAME="ble_app_blinky_pca10056_s140.hex"

SOFTDEVICE_HEX="${SOFTDEVICE_HEX:-$REPO_ROOT/s140_nrf52_7.2.0_softdevice.hex}"
DEVICE="${DEVICE:-NRF52840_XXAA}"
JLINK_IF="${JLINK_IF:-SWD}"
JLINK_SPEED="${JLINK_SPEED:-4000}"

app_hex_path() {
  # $1 = config (Debug|Release)
  echo "$SES_PROJECT_DIR/Output/$1/Exe/$APP_HEX_NAME"
}

to_win_path() {
  # Convert a git-bash/MSYS POSIX path (e.g. /c/Users/...) to a native Windows
  # path (C:\Users\...). emBuild.exe, JLink.exe and JLinkRTTLogger.exe are
  # native Windows binaries and cannot resolve /c/... paths -- every path
  # handed to them (project file, hex file, output file) MUST go through
  # this first, or they fail with "Failed to open file" / similar.
  if command -v cygpath >/dev/null 2>&1; then
    cygpath -w "$1"
  else
    echo "$1"
  fi
}

require_config() {
  case "${1:-}" in
    Debug|Release) ;;
    *) echo "Usage: $(basename "$0") [Debug|Release]" >&2; exit 1 ;;
  esac
}

resolve_embuild() {
  if [[ -n "${EMBUILD_EXE:-}" ]]; then echo "$EMBUILD_EXE"; return 0; fi
  local candidates=(
    "/c/Program Files/Segger/SEGGER Embedded Studio 8.24/bin/emBuild.exe"
    "/c/Program Files (x86)/SEGGER/SEGGER Embedded Studio 8.24/bin/emBuild.exe"
  )
  local c
  for c in "${candidates[@]}"; do
    [[ -f "$c" ]] && { echo "$c"; return 0; }
  done
  # The version in the folder name varies with whatever was installed
  # (8.24, 8.30a, ...) -- glob instead of pinning one version.
  for c in "/c/Program Files/SEGGER/SEGGER Embedded Studio "*"/bin/emBuild.exe" "/c/Program Files (x86)/SEGGER/SEGGER Embedded Studio "*"/bin/emBuild.exe"; do
    [[ -f "$c" ]] && { echo "$c"; return 0; }
  done
  if command -v emBuild.exe >/dev/null 2>&1; then
    command -v emBuild.exe
    return 0
  fi
  echo "ERROR: emBuild.exe (SEGGER Embedded Studio CLI build tool) not found." >&2
  echo "       Set EMBUILD_EXE=/path/to/emBuild.exe or install SES." >&2
  return 1
}

resolve_jlink() {
  if [[ -n "${JLINK_EXE:-}" ]]; then echo "$JLINK_EXE"; return 0; fi
  local candidates=(
    "/c/Program Files/SEGGER/JLink/JLink.exe"
    "/c/Program Files (x86)/SEGGER/JLink/JLink.exe"
  )
  local c
  for c in "${candidates[@]}"; do
    [[ -f "$c" ]] && { echo "$c"; return 0; }
  done
  # The Windows installer defaults to a version-suffixed folder
  # (SEGGER/JLink_V970, JLink_V978, ...) instead of plain SEGGER/JLink unless
  # a prior unversioned install already exists. Glob for that too.
  for c in "/c/Program Files/SEGGER/JLink_V"*"/JLink.exe" "/c/Program Files (x86)/SEGGER/JLink_V"*"/JLink.exe"; do
    [[ -f "$c" ]] && { echo "$c"; return 0; }
  done
  if command -v JLink.exe >/dev/null 2>&1; then
    command -v JLink.exe
    return 0
  fi
  echo "ERROR: JLink.exe (SEGGER J-Link Commander) not found." >&2
  echo "       Set JLINK_EXE=/path/to/JLink.exe or install the J-Link Software pack." >&2
  echo "       Do not substitute nrfjprog here -- this project standardizes on the SEGGER CLI." >&2
  return 1
}

resolve_rtt_logger() {
  if [[ -n "${JLINK_RTT_LOGGER_EXE:-}" ]]; then echo "$JLINK_RTT_LOGGER_EXE"; return 0; fi
  local candidates=(
    "/c/Program Files/SEGGER/JLink/JLinkRTTLogger.exe"
    "/c/Program Files (x86)/SEGGER/JLink/JLinkRTTLogger.exe"
  )
  local c
  for c in "${candidates[@]}"; do
    [[ -f "$c" ]] && { echo "$c"; return 0; }
  done
  for c in "/c/Program Files/SEGGER/JLink_V"*"/JLinkRTTLogger.exe" "/c/Program Files (x86)/SEGGER/JLink_V"*"/JLinkRTTLogger.exe"; do
    [[ -f "$c" ]] && { echo "$c"; return 0; }
  done
  if command -v JLinkRTTLogger.exe >/dev/null 2>&1; then
    command -v JLinkRTTLogger.exe
    return 0
  fi
  echo "ERROR: JLinkRTTLogger.exe not found. Set JLINK_RTT_LOGGER_EXE=/path/to/JLinkRTTLogger.exe" >&2
  return 1
}

run_jlink_commands() {
  # $1 = JLink.exe path, $2 = here-doc content (command script)
  local jlink="$1"
  local script="$2"
  local cmd_file
  cmd_file="$(mktemp)"
  printf '%s\n' "$script" > "$cmd_file"
  "$jlink" -device "$DEVICE" -if "$JLINK_IF" -speed "$JLINK_SPEED" -autoconnect 1 -NoGui 1 -ExitOnError 1 -CommandFile "$cmd_file"
  rm -f "$cmd_file"
}
