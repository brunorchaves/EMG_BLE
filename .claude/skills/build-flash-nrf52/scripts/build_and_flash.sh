#!/usr/bin/env bash
# Day-to-day workflow: rebuild the app and flash it. Assumes the SoftDevice
# is already on the device (use flash_all.sh for a brand-new/erased board).
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

CONFIG="${1:-Release}"
require_config "$CONFIG"

"$SCRIPT_DIR/build.sh" "$CONFIG"
"$SCRIPT_DIR/flash_app.sh" "$CONFIG"
