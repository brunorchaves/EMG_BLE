#!/usr/bin/env bash
# Flash the S140 SoftDevice via SEGGER J-Link Commander (JLink.exe).
# Only needed once per device, or after a full chip erase.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

JLINK="$(resolve_jlink)"

[[ -f "$SOFTDEVICE_HEX" ]] || {
  echo "ERROR: SoftDevice hex not found: $SOFTDEVICE_HEX" >&2
  echo "       Override with SOFTDEVICE_HEX=/path/to/s140_*.hex" >&2
  exit 1
}

echo "==> Flashing SoftDevice ($SOFTDEVICE_HEX) via JLink.exe"
run_jlink_commands "$JLINK" "$(cat <<EOF
r
h
loadfile "$SOFTDEVICE_HEX"
r
g
q
EOF
)"
echo "==> SoftDevice flashed"
