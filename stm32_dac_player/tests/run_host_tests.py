"""Compila e roda tests/test_host.c no PC com o zig cc (pip install ziglang).

    python stm32_dac_player/tests/run_host_tests.py
"""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ["tests/test_host.c", "common/player.c", "common/cli.c", "common/stimulus_table.c"]


def main() -> int:
    exe = Path(tempfile.gettempdir()) / ("stm32_dac_player_host_test" + (".exe" if sys.platform == "win32" else ""))
    cmd = [sys.executable, "-m", "ziglang", "cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Wno-date-time", "-Icommon",
           *SOURCES, "-lm", "-o", str(exe)]
    r = subprocess.run(cmd, cwd=ROOT)
    if r.returncode:
        print("falha na compilacao (pip install ziglang?)")
        return r.returncode
    return subprocess.run([str(exe)]).returncode


if __name__ == "__main__":
    sys.exit(main())
