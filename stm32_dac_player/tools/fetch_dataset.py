"""Baixa e extrai o UCI EMG Physical Action Data Set em data/uci_emg_physical_action/.

O zip da UCI traz um .rar dentro. O tar do Windows (bsdtar/libarchive)
extrai RAR; se nao der, tenta o 7-Zip.

    python stm32_dac_player/tools/fetch_dataset.py
"""

from __future__ import annotations

import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

from emg_dataset import DATA_DIR, DATASET_DIR, ZIP_PATH, ZIP_SHA256, ZIP_URL, sha256_file


def extract_rar(rar: Path, dest: Path) -> None:
    tar = Path(r"C:\Windows\System32\tar.exe")
    candidates = [[str(tar), "-xf", str(rar)]] if tar.exists() else []
    candidates.append(["tar", "-xf", str(rar)])
    for exe in (shutil.which("7z"), r"C:\Program Files\7-Zip\7z.exe"):
        if exe and Path(exe).exists():
            candidates.append([exe, "x", "-y", str(rar)])
    for cmd in candidates:
        r = subprocess.run(cmd, cwd=dest, capture_output=True, text=True)
        if r.returncode == 0 and DATASET_DIR.exists():
            return
    sys.exit(f"nao consegui extrair {rar}: instale o 7-Zip ou extraia a mao em {dest}")


def main() -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    if not ZIP_PATH.exists():
        print(f"baixando {ZIP_URL}")
        urllib.request.urlretrieve(ZIP_URL, ZIP_PATH)
    digest = sha256_file(ZIP_PATH)
    if digest != ZIP_SHA256:
        sys.exit(f"SHA-256 do zip nao confere:\n  esperado {ZIP_SHA256}\n  obtido   {digest}")
    print(f"zip ok  sha256 {digest}")

    if not DATASET_DIR.exists():
        with zipfile.ZipFile(ZIP_PATH) as z:
            rars = [n for n in z.namelist() if n.lower().endswith(".rar")]
            z.extractall(DATA_DIR)
        for name in rars:
            extract_rar(DATA_DIR / name, DATA_DIR)
    n = len(list(DATASET_DIR.glob("sub*/*/txt/*.txt")))
    print(f"dataset em {DATASET_DIR} ({n} arquivos .txt)")


if __name__ == "__main__":
    main()
