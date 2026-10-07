# Compila e grava o player na Nucleo, sem instalar nada da ST.
#   powershell -File stm32_dac_player\tools\flash.ps1 -Board h563zi
#
# Grava copiando o .bin para o drive USB que o ST-LINK da Nucleo monta
# (NOD_H563ZI / NODE_F767ZI). Se o STM32_Programmer_CLI estiver no PATH,
# -Programmer usa ele no lugar (grava, verifica e reseta).
param(
    [ValidateSet("h563zi", "f767zi")] [string] $Board = "h563zi",
    [switch] $NoBuild,
    [switch] $Programmer
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$bin = Join-Path $root "build\$Board\stm32_dac_player_$Board.bin"

if (-not $NoBuild) { & (Join-Path $PSScriptRoot "build.ps1") -Board $Board | Out-Null }
if (-not (Test-Path $bin)) { throw "$bin nao existe - rode sem -NoBuild" }

if ($Programmer) {
    $cli = Get-Command STM32_Programmer_CLI -ErrorAction SilentlyContinue
    if (-not $cli) { throw "STM32_Programmer_CLI nao esta no PATH" }
    & $cli.Source -c port=SWD -w $bin 0x08000000 -v -rst
    if ($LASTEXITCODE) { throw "STM32_Programmer_CLI falhou" }
    return
}

$label = "^NODE?_" + $Board.ToUpper() + "$"
$vol = Get-Volume | Where-Object { $_.FileSystemLabel -match $label -and $_.DriveLetter } | Select-Object -First 1
if (-not $vol) {
    $blocked = Get-Volume | Where-Object { $_.DriveType -eq "Removable" -and -not $_.FileSystemLabel }
    if ($blocked) {
        throw ("drive do ST-LINK sem rotulo/sistema de arquivos (provavel bloqueio de politica de " +
               "armazenamento removivel - acesso negado mesmo ao Get-ChildItem). Use a gravacao via WSL " +
               "(README.md, secao 'Gravacao via WSL') em vez deste script.")
    }
    throw "drive do ST-LINK ($label) nao encontrado - a Nucleo esta no USB do ST-LINK (CN1)?"
}
$drive = "$($vol.DriveLetter):\"
Write-Host "gravando $bin -> $drive"
try {
    Remove-Item (Join-Path $drive "FAIL.TXT") -ErrorAction SilentlyContinue
    Copy-Item $bin $drive -ErrorAction Stop
} catch {
    throw ("acesso negado ao copiar para $drive - provavel bloqueio de politica de armazenamento removivel. " +
           "Use a gravacao via WSL (README.md, secao 'Gravacao via WSL'). Erro original: $_")
}

# o ST-LINK grava, desmonta e remonta o drive; FAIL.TXT aparece se der errado
$deadline = (Get-Date).AddSeconds(30)
do {
    Start-Sleep -Milliseconds 500
    $v = Get-Volume | Where-Object { $_.FileSystemLabel -match $label -and $_.DriveLetter } | Select-Object -First 1
} while (-not $v -and (Get-Date) -lt $deadline)
if (-not $v) { throw "o drive do ST-LINK nao voltou em 30 s" }
$fail = Join-Path "$($v.DriveLetter):\" "FAIL.TXT"
if (Test-Path $fail) { throw "ST-LINK recusou a gravacao: $(Get-Content $fail -Raw)" }
Write-Host "ok - a placa reinicia sozinha e ja toca o laco. Serial: 115200 8N1 na porta do ST-LINK"
