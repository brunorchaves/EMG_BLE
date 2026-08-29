# Ensaio 2 — o que falta fazer quando o hardware voltar

**Estado:** todo o software está pronto e testado. O `relatorio_ensaio_2.pdf`
já existe, mas foi gerado com dados de um run **anterior** (44,8 s de janela
conectado+streaming). Falta rodar o run definitivo com janelas longas e o
firmware novo.

**Pré-requisito:** PPK2 e J-Link estavam ausentes do USB no momento em que isto
foi escrito (nenhum `VID_1915` nem `VID_1366`). Sem a PPK2 a placa não tem
alimentação nenhuma — nada pode ser gravado nem medido.

---

## Passo 0 — Conferir que o hardware voltou

```powershell
Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -match 'VID_1915|VID_1366' } |
  Select-Object FriendlyName, Status
```

Esperado: `PPK2` (VID_1915) e `J-Link` (VID_1366) presentes. Anote a porta COM
da PPK2 — foi `COM8` nesta máquina, mas ela muda ao reconectar.

```bash
python power_profiling/ppk2_check.py --port COM8
```

Fiação: **VOUT** e **GND** da PPK2 no conector J2 da placa, VIN aberto, modo
source meter. Ver `power_profiling/README.md`.

---

## Passo 1 — Gravar o firmware com power-down do ADC

O código já está commitado, **mas nunca foi gravado nem medido** — o hardware
saiu do ar antes. A configuração alvo do ensaio é **3,3 V + ADC a 1 kSPS**,
porque é de onde saiu o número que o Robert avaliou.

```bash
# 1 kSPS: mudar o default em ADS112C04.h
sed -i 's/^#define ADS_TURBO_MODE 1$/#define ADS_TURBO_MODE 0/' \
  emg_nrf_ses/project/ble_peripheral/ble_app_blinky/ADS112C04.h

export EMBUILD_EXE="/c/Program Files/SEGGER/SEGGER Embedded Studio 8.30a/bin/emBuild.exe"
export JLINK_EXE="/c/Program Files/SEGGER/JLink_V970/JLink.exe"
bash .claude/skills/build-flash-nrf52/scripts/build.sh Release

# a placa precisa estar alimentada para o J-Link conectar
python power_profiling/ppk2_hold_on.py --port COM8 --voltage-mv 3300 &
sleep 3
bash .claude/skills/build-flash-nrf52/scripts/flash_app.sh Release
```

Ao mudar `ADS_TURBO_MODE`, os coeficientes do filtro digital trocam
automaticamente (estão amarrados ao mesmo flag) — não há nada a ajustar à mão.

---

## Passo 2 — Validar o power-down, que é código não testado

**Este é o passo que mais pode dar errado.** O power-down do ADC nunca rodou
em hardware. Contadores a conferir:

```bash
python power_profiling/fw_counters.py --watch 8
```

| Situação | Esperado |
|---|---|
| Desconectado | `aquisicao: EM POWER-DOWN`, `g_acq_running` = 0 |
| `g_acq_error_count` | **0** — se subir, o I²C falhou na transição |
| `g_init_status` | `0x3F` (todos os bits de init) |

E com uma conexão BLE ativa, `g_acq_running` deve ir para 1 e a taxa de
amostragem voltar a ~1020 S/s:

```bash
python -c "
import sys, asyncio; sys.path.insert(0,'power_profiling')
from timeline import Clock; from ble_client import EmgBleClient
async def m():
    c = EmgBleClient(Clock.start_now(), lambda k,d: None)
    d = await c.scan(timeout=12); await c.connect(d, use_cached_services=False)
    await c.read_mtu(); await c.subscribe()
    print('subscrito, mantendo 30 s'); await asyncio.sleep(30)
    print('pacotes:', len(c.drain_packets()))
    await c.unsubscribe(); await c.disconnect()
asyncio.run(m())
" &
sleep 10 && python power_profiling/fw_counters.py --watch 8
```

**Se `g_acq_error_count` subir ou o STREAMING vier vazio:** o `ads112c04_start()`
não está acordando o ADC depois do POWERDOWN. O datasheet diz que POWERDOWN
preserva os registradores, mas se na prática não preservar, a correção é
reconfigurar no wake — chamar `ads112c04_configure_raw_mode()` em vez de só
`ads112c04_start()` em `main.c`, no bloco de reconciliação da aquisição.
Fallback rápido: desativar o power-down (`g_acq_should_run = true` fixo) e
rodar o ensaio sem ele — a janela que o Robert pediu não depende disso.

---

## Passo 3 — O run do ensaio, com janelas longas

```bash
python power_profiling/run_bench.py --port COM8 --voltage-mv 3300 \
  --out power_profiling/runs --no-gain-sweep --input-condition open \
  --duration CONNECTED_IDLE=60 --duration STREAMING=120
```

Por que essas durações: o RMS do pior caso merece amostra estatística maior, e
o `CONNECTED_IDLE` estava em 10 s **só** por causa do bug de overflow de
`packet_index`, que já foi corrigido. Total ~290 s, ~115 MB de captura.

Conferir na saída:
- `pre-run: aquisicao do ADC = ~1020 S/s` com `init_status = 0x3F`
- `pacotes EMG recebidos` > 1900 (120 s a ~16 pkt/s)

---

## Passo 4 — Exportar os dados para o Robert

```bash
python power_profiling/export_data.py power_profiling/runs/<NOVO_RUN>
```

Gera em `runs/<run>/export/`:

| Arquivo | Para quê |
|---|---|
| `conectado_streaming_1000Hz.csv` | **o principal pedido dele** — abre em planilha |
| `todas_etapas_1000Hz.csv` | a outra janela pedida |
| `*_full_uA.npy` | resolução cheia (100 kS/s), uso programático |
| `*_full.json` | metadados: fs, bandas, tensão |
| `resumo_janelas.json` | estatísticas já calculadas |

**A auto-checagem tem de passar.** O script recalcula média e RMS a partir do
CSV decimado e compara com a resolução cheia; erro esperado ~5e-6. Se imprimir
`DIVERGE`, ele aborta de propósito — não entregue um CSV que dê RMS errado.

---

## Passo 5 — Gerar o relatório definitivo

```bash
python power_profiling/report_ensaio2.py \
  --run power_profiling/runs/<NOVO_RUN> \
  -o power_profiling/relatorio_ensaio_2.pdf
```

Conferir visualmente: nas páginas de janela, a duração deve refletir os 180 s
(60 + 120) em vez dos 44,8 s do run antigo.

---

## Passo 6 — Regenerar o relatório de consumo com os picos corrigidos

`analyze_run()` agora usa `spike_filter=True` por default. Isso **corrige os
picos** que estavam subestimados no `relatorio_consumo.pdf` e no README.

```bash
python power_profiling/report_pdf.py -o power_profiling/relatorio_consumo.pdf \
  --run "5V=power_profiling/runs/2026-08-26T11-49-35Z" \
  --run "3V3=power_profiling/runs/2026-08-26T11-52-01Z" \
  --run "3V3_1kSPS=power_profiling/runs/2026-08-26T12-14-02Z"
```

Depois atualizar no `README.md` a coluna de picos da tabela de consumo por
estado. **As médias não mudam** — só os picos sobem. Se alguma média mudar
mais que ~8%, algo está errado.

> Feche o PDF no editor antes: o Windows trava o arquivo e a geração falha com
> `PermissionError`.

---

## Passo 7 — Commitar

```bash
git add power_profiling/ README.md
# os arquivos de export sao pequenos (CSV ~2-7 MB) e podem ser versionados;
# runs/ inteiro esta no .gitignore
```

---

## O que já está pronto (não precisa refazer)

| Item | Estado |
|---|---|
| `export_data.py` | testado com dados reais, auto-checagem com erro 5e-6 |
| `report_ensaio2.py` | gera 6 páginas, revisado visualmente |
| `--duration ESTADO=S` no `run_bench.py` | testado (`--help`) |
| `_apply_spike_filter` fiel ao algoritmo da Nordic | implementado e comparado |
| `analyze_run(spike_filter=True)` | default trocado |
| Contadores de power-down no `fw_counters.py` | implementados |
| Firmware com power-down | compila; **não gravado, não medido** |

---

## Respostas ao Robert que já estão medidas

Estas não dependem do run novo — saíram dos dados existentes:

**"Esse valor de 2,75 é RMS ou média?"** Era a **média da banda ADVERTISING**.
Não era RMS e não era o run inteiro.

**"Faça a medição só de streaming e conectado."** Feito (3,3 V, 1 kSPS,
janela de 44,8 s; o run novo estende para 180 s):

| Janela | Média | RMS |
|---|---|---|
| Todas as etapas | 2,113 mA | 3,304 mA |
| **Conectado + streaming** | **2,690 mA** | **3,854 mA** |

A intuição dele estava certa: a janela completa **subestima em 21%** a
corrente de operação.

**Ressalva importante a comunicar.** O RMS depende do tratamento dos artefatos
de troca de faixa da PPK2 — varia de **3,85 mA** (método do fabricante) a
**5,60 mA** (bruto), porque o RMS pondera os extremos ao quadrado e é
justamente ali que o artefato vive. A **média é robusta** (~7,7% de
espalhamento). Para dimensionar energia e autonomia, use a média; o RMS
importa para perdas I²R na ESR. Detalhes na página 2 do relatório.
