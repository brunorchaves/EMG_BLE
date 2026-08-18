#!/usr/bin/env bash
# Flash the already-built application hex via SEGGER J-Link Commander (JLink.exe).
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

CONFIG="${1:-Release}"
require_config "$CONFIG"

JLINK="$(resolve_jlink)"
APP_HEX="$(app_hex_path "$CONFIG")"

[[ -f "$APP_HEX" ]] || {
  echo "ERROR: app hex not found: $APP_HEX" >&2
  echo "       Run build.sh $CONFIG first." >&2
  exit 1
}

echo "==> Flashing application ($CONFIG) via JLink.exe"
run_jlink_commands "$JLINK" "$(cat <<EOF
r
h
loadfile "$APP_HEX"
r
g
q
EOF
)"
echo "==> Application flashed and running"
