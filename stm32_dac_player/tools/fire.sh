#!/usr/bin/env bash
# Dispara UM laco de uma acao e para, para uma captura single-shot no
# osciloscopio. Rode de dentro do WSL (onde fica o /dev/ttyACM0 do usbipd).
#
#   bash tools/fire.sh 3          # dispara o segmento 3
#   bash tools/fire.sh Walking    # ou pelo nome
#
# "seg <n>" ja reinicia o laco na amostra 0 com (indice+1) pulsos de sync, entao
# ele mesmo e o disparo: o osciloscopio em single-shot pega o laco completo. Ao
# fim dos 7,5 s o script manda "silence", para a placa nao ficar repetindo
# enquanto voce salva a captura.
set -euo pipefail

SEG="${1:?uso: fire.sh <indice|nome>}"
PORT="${PORT:-/dev/ttyACM0}"
LOOP_S=7.5

[[ -c "$PORT" ]] || { echo "porta $PORT nao existe - o ST-LINK esta anexado ao WSL (usbipd attach)?" >&2; exit 1; }
stty -F "$PORT" 115200 raw -echo

exec 3<>"$PORT"
printf 'silence\r' >&3; sleep 0.3; timeout 0.5 cat <&3 >/dev/null || true

T0=$(date +%s.%N)
printf 'seg %s\r' "$SEG" >&3
sleep 0.4
RESP=$(timeout 0.8 cat <&3 | tr -d '\r' | grep -a '^mode' || true)
[[ -n "$RESP" ]] || { echo "a placa nao respondeu ao 'seg $SEG'" >&2; exit 1; }
case "$RESP" in *erro*) echo "$RESP" >&2; exit 1;; esac

echo "disparado $(date -d "@$T0" +%H:%M:%S.%3N)   epoch $T0"
echo "  $RESP"
echo "  a amostra abre em +2,000 s e fecha em +6,000 s do 1o pulso de sync"

sleep "$LOOP_S"
printf 'silence\r' >&3; sleep 0.3; timeout 0.5 cat <&3 >/dev/null || true
exec 3<&-
echo "  laco completo, placa em silencio"
