# Passa o ST-LINK da Nucleo para o WSL via usbipd-win, para rodar pyocd la
# dentro (o drive USB de arrastar-e-soltar do ST-LINK fica bloqueado por
# politica corporativa de armazenamento removivel nesta maquina - ver
# stm32_dac_player/README.md, secao "Gravacao via WSL").
#
#   powershell -File stm32_dac_player\tools\wsl_attach_stlink.ps1
#
# "usbipd bind" (uma vez, precisa de admin - ja feito nesta maquina)
# compartilha o dispositivo; "attach" (sem admin, roda este script) conecta
# o ST-LINK a esta sessao do WSL. Rode de novo so se o WSL parar de ver
# /dev/ttyACM0 ou /dev/bus/usb/001/* (depois de um reboot, sleep ou
# desconectar/reconectar o cabo). --auto-attach fica observando e
# reconecta sozinho enquanto este script roda.
$busid = (usbipd list | Select-String "0483:374e").ToString().Trim().Split(" ")[0]
if (-not $busid) { Write-Host "ST-LINK nao encontrado no USB (placa desligada?)"; exit 1 }
usbipd attach --wsl --busid $busid --auto-attach
