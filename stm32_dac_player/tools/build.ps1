# Compila o firmware do player para uma placa.
#   powershell -File stm32_dac_player\tools\build.ps1 -Board h563zi
# Saida: stm32_dac_player\build\<placa>\stm32_dac_player_<placa>.bin/.hex/.elf
param(
    [ValidateSet("h563zi", "f767zi")] [string] $Board = "h563zi"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

# cmake/ninja instalados por "pip install --user cmake ninja" ficam fora do PATH
$userScripts = & python -c "import sysconfig; print(sysconfig.get_path('scripts', 'nt_user'))"
if ($userScripts -and (Test-Path $userScripts)) { $env:PATH = "$userScripts;$env:PATH" }
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { throw "cmake nao encontrado: pip install --user cmake ninja" }
if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) { throw "ninja nao encontrado: pip install --user cmake ninja" }

Push-Location $root
try {
    cmake --preset $Board
    if ($LASTEXITCODE) { throw "cmake --preset $Board falhou" }
    cmake --build --preset $Board
    if ($LASTEXITCODE) { throw "build $Board falhou" }
} finally {
    Pop-Location
}
Join-Path $root "build\$Board\stm32_dac_player_$Board.bin"
