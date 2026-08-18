#!/usr/bin/env bash
# Full chip erase via SEGGER J-Link Commander (JLink.exe). Required before the
# very first SoftDevice flash, or to recover a device stuck with readback
# protection enabled.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

JLINK="$(resolve_jlink)"

echo "==> Erasing chip via JLink.exe (SEGGER J-Link Commander)"
run_jlink_commands "$JLINK" "$(cat <<'EOF'
r
h
erase
r
q
EOF
)"
echo "==> Erase complete"
