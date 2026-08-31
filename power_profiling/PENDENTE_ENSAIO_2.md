# Ensaio 2 — executado em 2026-08-31

Run definitivo: **`runs/2026-08-31T14-11-34Z`** — 3,3 V, ADC a 1 kSPS, com
power-down do ADC. 262 s de captura, 26,2 M amostras, janela de operação de
184,8 s (60 s conectado + 120 s streaming + 6 s conectado).

Entregáveis: [`relatorio_ensaio_2.pdf`](relatorio_ensaio_2.pdf) e
`runs/2026-08-31T14-11-34Z/export/`.

---

## Resultado

| Janela | Média | RMS |
|---|---|---|
| Todas as etapas | 2,262 mA | 3,405 mA |
| **Conectado + streaming** | **2,647 mA** | **3,808 mA** |
| Somente streaming | 2,734 mA | 3,989 mA |

O valor de 2,75 mA reportado antes era a **média da banda ADVERTISING** — não
era RMS e não era o run inteiro. A janela completa **subestima em 15%** a
corrente de operação.

**O RMS é ambíguo por um fator de ~1,5** dependendo do tratamento dos artefatos
de troca de faixa da PPK2: 3,81 mA pelo método do fabricante, 5,58 mA no bruto.
A média varia só 8,7%. Para energia e autonomia use a média; o RMS importa para
perdas I²R na ESR.

## Power-down do ADC: validado em hardware

| Situação | Contadores |
|---|---|
| Desconectado | `g_acq_running` = 0, `g_acq_stop_count` = 1, `g_loop_count` **congelado** (CPU dormindo) |
| Conectado | 1014,7 S/s, **0% de conversões perdidas**, **0 blocos descartados** |
| Após desconectar | `g_acq_stop_count` = 2, `g_acq_running` = 0 |
| Sempre | `g_acq_error_count` = 0, `g_init_status` = 0x3F |

Efeito: ADVERTISING caiu de 2,52 para 1,54 mA (−39%); autonomia projetada de
151 para 238 h. **A meta de 2 mA médios foi atingida** (1,60 mA na mistura
94/3/3). A de 5 mA de pico não — p99,9 = 18,7 mA.

---

## O que ficou aberto

### 1. A banda RE_ADVERTISING não mede advertising

O central no Windows **não derruba o link** ao chamar `disconnect()`: a sessão
WinRT só é liberada quando o **processo** do central termina, e o `run_bench`
não pode terminar porque é ele que mantém o stream da PPK2 vivo.

Diagnóstico que fecha o caso: a cadência dos picos de rádio na banda suspeita é
de **97 ms** (intervalo de conexão), contra 197 ms (intervalo de anúncio) na
banda ADVERTISING. E os contadores provam que o firmware está correto — depois
de um disconnect limpo, `g_acq_stop_count` incrementa.

Enquanto a aquisição rodava sempre isso era invisível: advertising e conectado
custavam o mesmo. `analyze.py` agora detecta e avisa quando as duas bandas
divergem mais de 15%.

**Impacto:** +0,072 mA (3,2%) na janela *todas as etapas*. A janela
conectado+streaming **não** é afetada. A banda ADVERTISING é válida.

**Correção possível:** rodar o central BLE num processo filho que o bench mata
a cada fase — o único mecanismo que comprovadamente libera a sessão. É mudança
de arquitetura do orquestrador, não um ajuste.

### 2. A leitura pré-run da taxa de aquisição virou inútil

Ela é feita com o dispositivo desconectado — exatamente quando o ADC está
dormindo. O valor correto passou a ser 0 S/s. Consequência: a análise espectral
cai para a taxa de **entrega**, ~3% abaixo da de aquisição.

Contornado neste run medindo à parte, com uma conexão dedicada, e gravando em
`fw_counters.json` com o rótulo `POS-run`. A correção estrutural é o bench
medir isso dentro de uma conexão (antes da janela medida, para não perturbar).

### 3. O máximo de corrente não é um pico da placa

Mesmo após o spike filter sobram ~0,001% das amostras acima de 25 mA (174 de
18,5 M na janela de operação), **todas na mesma faixa de medição da PPK2** —
resíduo de artefato que a detecção não pegou, porque ela olha as 3 amostras
seguintes à troca e alguns artefatos caem fora dessa janela.

O relatório publica o **p99,99** como pico representativo e marca o máximo com
asterisco. Para um número definitivo: travar a faixa da PPK2 (o opcode
`RANGE_SET` existe no protocolo) ou medir com shunt + osciloscópio como
referência independente.

### 4. Perda de captura

37 gaps de contador não explicados por troca de faixa, num registro de 26,6 M
amostras. Limite superior da perda: 37 × 63 = 2331 amostras, **≤ 0,01%**.
Não invalida nada, mas `capture_lossless` fica `False`.

### 5. Itens de otimização não mexidos

- **DC/DC do nRF52840** — ainda `POWER_CONFIG_DEFAULT_DCDCEN 0`. Antes de
  habilitar, confirmar que o módulo XIAO tem o indutor no pino DCC.
- **TWIM com EasyDMA por PPI** — hoje o driver legado `nrfx_twi`, bloqueante.
  `NRFX_TWIM_ENABLED 1` já está posto; a migração exige reverter dois patches
  feitos à mão no SDK vendorizado.
- **Escrita BLE de ganho é no-op** — `on_write` valida e loga mas nunca atribui
  `gain_level`.
- **Sem número de sequência no pacote** — impede medir perda de ar diretamente.
- **Marcadores de estado em GPIO** para a porta lógica da PPK2 (pads livres
  P0.02 / P0.03 / P1.14 / P1.15), que dariam fronteiras de banda com precisão
  de microssegundos no mesmo stream da corrente.

---

## Como reproduzir

```bash
# 1 kSPS
sed -i 's/^#define ADS_TURBO_MODE 1$/#define ADS_TURBO_MODE 0/' \
  emg_nrf_ses/project/ble_peripheral/ble_app_blinky/ADS112C04.h

export EMBUILD_EXE="/c/Program Files/SEGGER/SEGGER Embedded Studio 8.30a/bin/emBuild.exe"
export JLINK_EXE="/c/Program Files/SEGGER/JLink_V970/JLink.exe"
bash .claude/skills/build-flash-nrf52/scripts/build.sh Release

# a placa precisa estar alimentada para o J-Link conectar (a PPK2 e a unica fonte)
python power_profiling/ppk2_hold_on.py --port COM8 --voltage-mv 3300 &
bash .claude/skills/build-flash-nrf52/scripts/flash_app.sh Release
python power_profiling/fw_counters.py --watch 8      # esperado: EM POWER-DOWN

# o ensaio (mate o ppk2_hold_on antes: ele detem a COM8)
python power_profiling/run_bench.py --port COM8 --voltage-mv 3300 \
  --out power_profiling/runs --no-gain-sweep --input-condition open \
  --duration CONNECTED_IDLE=60 --duration STREAMING=120

python power_profiling/export_data.py power_profiling/runs/<RUN>
python power_profiling/report_ensaio2.py --run power_profiling/runs/<RUN> \
  -o power_profiling/relatorio_ensaio_2.pdf
```

### Duas armadilhas que custaram tempo

**Não mate o processo do central BLE no meio de uma conexão.** O Windows fica
com uma sessão órfã e reconecta ao dispositivo sozinho, contaminando a banda
ADVERTISING do run seguinte. Deixe o script desconectar e sair.

**A falha do J-Link derrubava o ensaio inteiro.** `fw_counters` levantava
`SystemExit`, que não é capturado por `except Exception` (deriva de
`BaseException`), então a leitura dos contadores — um dado *auxiliar* — abortava
um run de 5 minutos antes de medir qualquer coisa, e sem traceback. Corrigido
com uma exceção própria (`FwCountersError`).
