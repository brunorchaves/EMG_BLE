#!/usr/bin/env bash
# Capture SEGGER RTT debug output to a file via JLinkRTTLogger.exe (SEGGER CLI).
# Usage: rtt_log.sh [output_file] [seconds]
#   output_file defaults to rtt_log.txt in the current directory.
#   seconds, if given, stops the capture automatically after N seconds.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

RTT_LOGGER="$(resolve_rtt_logger)"
OUT_FILE="${1:-rtt_log.txt}"
DURATION="${2:-}"

echo "==> Capturing RTT log to $OUT_FILE (Ctrl+C to stop)"
if [[ -n "$DURATION" ]]; then
  timeout "$DURATION" "$RTT_LOGGER" -Device "$DEVICE" -If "$JLINK_IF" -Speed "$JLINK_SPEED" -RTTChannel 0 "$OUT_FILE" || true
else
  "$RTT_LOGGER" -Device "$DEVICE" -If "$JLINK_IF" -Speed "$JLINK_SPEED" -RTTChannel 0 "$OUT_FILE"
fi
