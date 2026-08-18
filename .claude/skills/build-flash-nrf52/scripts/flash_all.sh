#!/usr/bin/env bash
# First-time device provisioning: erase, flash the SoftDevice, build the app,
# then flash the app. Use this once per new/blank board.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

CONFIG="${1:-Release}"
require_config "$CONFIG"

"$SCRIPT_DIR/erase.sh"
"$SCRIPT_DIR/flash_softdevice.sh"
"$SCRIPT_DIR/build.sh" "$CONFIG"
"$SCRIPT_DIR/flash_app.sh" "$CONFIG"
