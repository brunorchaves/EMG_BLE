#!/usr/bin/env bash
# Build the nRF52840 firmware via SEGGER Embedded Studio's CLI build tool (emBuild.exe).
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

CONFIG="${1:-Release}"
require_config "$CONFIG"

EMBUILD="$(resolve_embuild)"

echo "==> Building '$CONFIG' with emBuild.exe (SEGGER Embedded Studio CLI)"
"$EMBUILD" -config "$CONFIG" -rebuild "$(to_win_path "$SES_PROJECT_FILE")"

OUT_HEX="$(app_hex_path "$CONFIG")"
if [[ -f "$OUT_HEX" ]]; then
  echo "==> Build OK: $OUT_HEX"
else
  echo "ERROR: expected output hex not found at $OUT_HEX" >&2
  exit 1
fi
