# Caracterização de consumo com PPK2 + otimização de firmware para supercapacitor

## Status desta rodada (2026-08-26)

**Implementado e validado com hardware real:**
- `config.py`, `ppk2_decode.py`, `dsp.py` — testados com dados sintéticos (contador
  com/sem gaps, decode_words, PSD, envelope RMS, t-test de Welch).
- `ppk2_stream.py` — captura de **10s e depois de 2,13 milhões de amostras reais
  sem nenhum gap de contador** (a validação mais crítica do design: prova que a
  captura não perde amostra). `set_buffer_size(1MB)` não elevou `max_in_waiting`
  acima de 4096 na prática (o driver Windows parece ignorar o pedido), mas mesmo
  assim não perdeu amostra nas janelas testadas.
- `timeline.py` — `fit_stream_clock` valida com dados sintéticos E reais (drift
  ~14-130 ppm, latência p50 ~9ms/p95 ~16ms, dentro do esperado). Corrigido um bug
  real: `build_bands` fechava a última banda só com eventos cujo estado estava na
  sequência oficial, então a banda final (`OFF_FINAL`) sempre ficava com duração
  zero — corrigido para fechar com QUALQUER `state.enter` seguinte.
- `ble_client.py` — conecta, negocia MTU (247 na prática, de forma consistente),
  subscreve/cancela/desconecta, tudo confirmado contra o sensor real. Corrigido um
  bug real: a lógica original só processava pacotes pelo handler bruto do WinRT
  OU pelo callback do bleak (nunca os dois), e se o handler bruto não disparasse
  (o que aconteceu na prática), **nenhum pacote era processado**. Agora o
  callback do bleak sempre processa (caminho testado e confiável); o handler bruto
  só enriquece o timestamp quando funciona.
- `run_bench.py`, `analyze.py`, `figures.py`, `compat.py` — um run `--quick`
  completo rodou de ponta a ponta contra o hardware real: 9 estados capturados
  corretamente, `report.json`/`report.md` gerados, 5 figuras geradas, saída legada
  (`resumo_tabela2.csv` etc.) gerada e compatível com o formato antigo.
- `emg_validate.py` — testado com 3 cenários sintéticos (sinal bom, pacotes
  truncados em 2 bytes replicando o bug real do firmware, sinal constante) —
  detecta corretamente o `n_bad_length` e recusa dar veredito "real" sem dados
  suficientes. A heurística de detecção de corner espectral é sensível a ruído
  alto no sinal sintético (deu "suspect" em vez de "real" no caso limpo) — vale
  recalibrar contra dados reais de EMG na Fase 1, não é um bug de lógica.

## Firmware corrigido e aquisição validada (2026-08-26, rodada 2)

O bloqueador abaixo **foi resolvido**, e o diagnóstico revelou que o problema
real era muito mais grave do que "não chega dado no BLE": **a aquisição do ADC
nunca funcionou neste firmware.**

### A causa raiz: não havia nenhuma fonte de tempo para a amostragem

O loop principal chamava `ads112c04_read_data()` e depois `sd_app_evt_wait()`,
que dorme até o próximo evento da aplicação. Sem conexão BLE, o **único** evento
recorrente era o timer do LED de 1 Hz. Medido com contador no firmware:
**1,0 amostra/s**. Os "2 kS/s" do artigo nunca foram implementados — a taxa era
puramente acidental, um efeito colateral de quem acordava o `WFE`.

Isso também explica retroativamente as observações anteriores que me confundiram:
- Os "726 pacotes de 2 bytes em 8,2 s" do firmware original eram sintoma da
  **corrupção de memória** do overflow (que deixava o loop erraticamente rápido),
  não amostragem real.
- O "0 pacotes" da primeira tentativa de `is_var_len=1` era simplesmente porque a
  1 Hz encher um bloco de 60 amostras leva **60 segundos**, e o teste durava 8. O
  `is_var_len` nunca foi o culpado.

### Correções aplicadas

| # | Correção | Arquivo | Efeito medido |
|---|---|---|---|
| 1 | **Aquisição governada pelo DRDY# do ADC** (P0.29, roteado na PCB e ignorado pelo firmware) via interrupção GPIOTE | `main.c` | **1,0 → 1772 S/s** |
| 2 | I²C de 100 kHz → **400 kHz** (o read bloqueante não acompanhava o período de 500 µs) | `main.c` | 1772 → **~2050 S/s, 0% de conversões perdidas** (era 11%) |
| 3 | **Overflow de `packet_index`**: `uint8_t` escrevendo num `int16_t[60]` sem bound-check, só zerado em SUCCESS/BUSY — no estado conectado-sem-CCCD (`INVALID_STATE`) corrompia `.bss` a ~2 kHz | `main.c` | fim dos travamentos |
| 4 | **`is_var_len = 1`** na característica de dados: sem isso o atributo GATT tem tamanho FIXO em `init_len` e a SoftDevice truncava toda notificação para 2 bytes | `ble_emg_service.c` | **2 → 120 bytes/pacote** |
| 5 | `APP_ERROR_CHECK(err_code)` com `err_code` **não inicializado** no disconnect | `main.c` | fim dos resets espúrios na transição CONNECTED→IDLE |
| 6 | Armadilhas `while(1);` na falha de init do ADC → retry + status legível | `main.c` | falha passa a ser diagnosticável |
| 7 | **DLE 27 → 251**: cada notificação de 123 B fragmentava em ~6 PDUs de link | `sdk_config.h` | throughput e energia/pacote |
| 8 | Fila de notificações (HVN) 1 → **6**, e remoção do gate `tx_in_progress` que limitava a 1 notificação por evento de conexão | `sdk_config.h`, `ble_emg_service.c` | 6,6 → 19 pkt/s |
| 9 | `GAP_EVENT_LENGTH` 6 → **24** (7,5 → 30 ms), com o ajuste de RAM que a SoftDevice exige (`RAM_START` 0x20002B78 → 0x20002C80 no `.emProject`) | `sdk_config.h`, `.emProject` | 19 → **31,2 pkt/s** |
| 10 | **Contadores de diagnóstico** (`g_loop_count`, `g_adc_ok_count`, `g_drdy_count`, ...) legíveis via J-Link | `main.c` + `power_profiling/fw_counters.py` | tornou todo o diagnóstico acima possível |

### Estado final validado

| Métrica | Antes | Depois |
|---|---|---|
| Taxa de aquisição | 1,0 S/s | **2055 S/s** |
| Conversões do ADC perdidas | — | **0%** |
| Tamanho do pacote BLE | 2 bytes | **120 bytes** |
| Taxa entregue via BLE | ~0 | **1873 S/s** (31,2 pkt/s) |
| Blocos descartados | ~100% | **8,8%** |
| Continuidade entre pacotes | — | **0,989** |

**Prova de que o dado é real:** com a taxa de aquisição correta, o espectro mostra
pico em **62,2 Hz** e terceiro harmônico em **190,6 Hz** (≈ 3 × 62,2) — rede
elétrica de 60 Hz captada pelos eletrodos abertos e amplificada ~900× pela cadeia
analógica. Lixo, buffer parado ou decodificação desalinhada não produziriam
fundamental limpa com harmônico correto. Isso valida o caminho completo:
front-end analógico → ADS112C04 → I²C → nRF52840 → filtro → FIFO → notificação
BLE → decodificação no PC.

**Armadilha metodológica encontrada (e corrigida na ferramenta):** o espectro
tem de ser calculado com a taxa de **aquisição**, não de **entrega**. Com blocos
descartados as duas divergem, e usar a de entrega escalou o eixo de frequência
por 4,5× — a mesma captura aparentava pico em 14 Hz em vez de 62 Hz. Um erro
desses inverteria qualquer conclusão sobre banda mioelétrica.
`emg_validate.validate_stream()` agora aceita `fs_acquisition` explicitamente.

**Veredito da validação: `suspect`** — e isso está correto. As duas ressalvas são
honestas: 8,8% de blocos ainda descartados, e o espectro dominado por rede
elétrica (55%), o que impede medir a banda do filtro e impede chamar de "EMG
real" sem eletrodos num sujeito. Para um veredito `real` é preciso terminar as
entradas (10 kΩ) ou injetar senoide conhecida.

### Pendências conhecidas

- **8,8% de blocos ainda descartados.** Produção 34,2 blocos/s vs entrega
  31,2/s. Reduzir mais exige encurtar o intervalo de conexão (custo de energia)
  ou reduzir a taxa/agregar no dispositivo — decisão de trade-off da Fase 3.
- **`GAP_EVENT_LENGTH 24` é trade-off consciente**: aumenta o tempo máximo de
  rádio ligado por evento. Precisa ser medido na caracterização de energia, não
  assumido como grátis.
- O comentário `// 1000 SPS` em `ADS112C04.c:11` continua errado (é 2000 SPS em
  turbo) — não mexi para não misturar com as correções funcionais.

---

**Bloqueador ORIGINAL (resolvido, mantido como registro do diagnóstico):**

Durante os testes, o firmware **intermitentemente não avança além da
inicialização** (a pilha BLE responde normalmente — conecta, negocia MTU,
processa CCCD — porque isso é tratado por interrupção da SoftDevice
independente do loop principal, mas o loop principal do `main()` fica parado,
então zero amostras de EMG chegam por BLE). Confirmado via halt+registradores do
J-Link: o PC ficou preso no mesmo endereço em leituras sucessivas, com o
`CycleCnt` avançando (não é um reset, é uma trava real). Em uma ocasião o
endereço caiu dentro de `main()` (~0x27e74, próximo aos dois `while(1);` de
falha de init do ADS112C04 achados na exploração, `main.c:676` e `:686`); em
outra, dentro do código da própria SoftDevice (abaixo de `0x273e0`, onde o
`.text` do app começa).

Duas hipóteses, nenhuma confirmada:
1. Falha intermitente de I²C na inicialização do ADS112C04, possivelmente
   sensível a como a PPK2 energiza a placa (transiente de subida diferente de
   uma bateria).
2. Algo residual do experimento com `is_var_len=1` (que causou um hang
   confirmado dentro da SoftDevice) — mesmo revertido e regravado, pode haver
   uma interação não percebida.

**Isso é uma pendência de firmware, não do código Python** — toda a
arquitetura Python foi validada de ponta a ponta com o board real, exceto o
trecho que depende de o firmware efetivamente notificar (validação de dados
reais de EMG, Fase 1 completa). Próximo passo: reproduzir isso de forma
controlada (múltiplos reset+boot, olhando o RTT a cada vez) antes de confiar em
qualquer captura "oficial" de EMG.

## Context

O artigo (SEB 2025) publicou o estudo de energia deste sensor sEMG na **Tabela 2**
(corrente RMS e potência por estado, a 5,0 V) e na **Fig. 7** (traço de corrente vs.
tempo com faixas sombreadas por estado, ~44 s, eixo Y cortado em 15 mA, um único
número RMS por estado). A instrumentação original **não é documentada em lugar
nenhum** do artigo — a história do "shunt + osciloscópio" só existe nos docs deste
repo.

Agora temos a PPK2 (PCA63100) funcionando, alimentando a placa a 5 V e medindo a
100 kS/s, mais o toolchain SEGGER completo (build + flash + RTT já validados nesta
máquina). O objetivo é: **(1)** refazer a caracterização com muito mais profundidade,
puxando dados reais de EMG via BLE de verdade; **(2)** salvar dados, figuras e
relatório no repo; **(3)** propor e implementar otimizações visando **~2 mA médios,
picos ≤ 5 mA**, pensando em alimentação futura por supercapacitor.

## O que a exploração revelou (e que muda o plano)

A documentação do projeto está substancialmente divergente do código. **Confiar em
`main.c`/`sdk_config.h`, não no README.** As divergências mais relevantes:

| Parâmetro | README/artigo dizem | Código realmente faz |
|---|---|---|
| Potência de TX | −20 dBm (artigo) | **`sd_ble_gap_tx_power_set` nunca é chamado** → default 0 dBm |
| Intervalo de advertising | 500 ms (artigo) / 40 ms (README) | **200 ms** (`APP_ADV_INTERVAL 320`) |
| Intervalo de conexão | 7,5–15 ms | **75–100 ms** |
| PHY | 2M | **1M** |
| Taxa de amostragem | 2 kS/s (Tabela 1) / 1 kS/s (abstract) | ADC em **turbo + 2000 SPS**, mas a taxa efetiva é ditada pelo polling de I²C, não por clock |
| Data Length Extension | 251 | **27** → cada notificação de 123 B fragmenta em ~6 PDUs de link |
| Endereço do ADC | 0x45 | **0x40** |

Ou seja: **duas das quatro "estratégias de baixo consumo" do artigo não estão
implementadas** (TX −20 dBm e advertising 500 ms). Isso já é um resultado a reportar.

### O maior consumidor não é o rádio — é a CPU em busy-wait

`twi_init()` passa event handler `NULL` (`main.c:593`), então todo transfer I²C é
**bloqueante e busy-polled** (`nrfx_twi.c:467-470`), a 100 kHz de SCL. Cada leitura
do ADC gasta ~0,4–0,5 ms girando a CPU. O loop roda a ~2 kHz e
`sd_app_evt_wait()` (`main.c:771`) retorna quase imediatamente toda iteração — **a
CPU praticamente nunca dorme**. O DRDY# do ADC está roteado para P0.29 e **nunca é
lido** (nenhum GPIOTE no app).

Consequência: o "IDLE" do artigo não é um estado idle — é aquisição a taxa plena com
o rádio anunciando. E a amostragem/filtragem/FIFO rodam **idênticas** desconectado,
conectado e transmitindo; só o `sd_ble_gatts_hvx` é condicionado à conexão. Não
existe standby de ADC (`ads112c04_powerdown()` existe e nunca é chamado).

### Três bugs que bloqueiam a medição

1. **Buffer overflow exatamente no estado CONNECTED do artigo.** Conectado sem CCCD,
   `notify_packet` retorna `INVALID_STATE` e `main.c:761` **não** reseta
   `packet_index`; com `ble_packet_buffer` sendo `int16_t[60]` e `packet_index` um
   `uint8_t`, escreve fora dos limites até o índice 255, corrompendo `.bss`
   adjacente. Medir esse estado sem corrigir isso mede comportamento corrompido.
2. **`err_code` não inicializado em `APP_ERROR_CHECK`** no disconnect
   (`main.c:304` + `main.c:332`) → possível reset espúrio exatamente na transição
   CONNECTED→IDLE.
3. **A escrita BLE de ganho é no-op**: `on_write` loga e **nunca atribui
   `gain_level`** (`ble_emg_service.c:9-23`). Meu teste de validação planejado
   ("escrever ganho e ver a amplitude escalar") **não funciona** no firmware atual.

Por isso o baseline será feito em duas versões: **Baseline-A** = HEAD como está
(o que o firmware realmente faz hoje, bugs incluídos) e **Baseline-B** = só as
correções desses três bugs, que é a referência honesta para o trabalho de otimização.

### A meta de 2 mA provavelmente não é alcançável só em firmware

Estimativa dos termos dos ~14 mA medidos: CPU em busy-wait a 64 MHz com filtro float
(o maior termo), LED1 piscando em 50% de duty (~1,5 mA médios — o esquemático puxa
L1 de VCC por R16=1 kΩ para o pino), LED2 parasita no pino de RX da UART (1 kΩ para
5 V num pino configurado como entrada), front-end analógico a 5 V (MCP609 quad +
INA317 + ADS112C04 em turbo + MAX6106 + DS3502), e três pull-ups de 4k7 **para 5 V**
com I²C quase continuamente ativo (~1 mA cada enquanto a linha está baixa).

A Fase 0 mede o piso irredutível objetivamente. Minha expectativa honesta: dá para
sair de ~14 mA para **~4–5 mA** só em firmware; chegar a 2 mA provavelmente exige as
mudanças de hardware listadas na Fase 4. Vou reportar o número real, não o desejado.

## Fatos verificados que definem o desenho

**PPK2** — `ppk2-api` 0.9.2, porta `COM8` (autodetecção falha sem o driver Nordic;
sempre passar `--port`). Source Meter a 5000 mV; **`toggle_DUT_power("ON")` é
obrigatório** além de `set_source_voltage()`, senão VOUT fica em 0 V. Sem timestamps
de hardware — o tempo vem de `índice/100000`, a validar contra relógio de parede.
`get_samples()` retorna `(correntes_uA, bits_digitais)`; `digital_channels(bits)`
decodifica D0–D7, amostrados a 100 kHz **no mesmo stream** da corrente.

**Sensor BLE** — nome `EMG_BLE`; serviço `19b10001-1000-e8f2-537e-4f6cd168a114`;
dados EMG (NOTIFY, sem read/write) `19b10002-…`, payload **60 × int16 LE = 120 B**,
reinterpret-cast cru do buffer: sem header, sem sequência, sem timestamp; ganho
(WRITE) `19b10003-…`, `uint8` 1–10. CCCD `SEC_OPEN`, pareamento não suportado. A
UART do sensor é decimada 100:1 **e sai em pino não conectado na PCB** — BLE é o
único caminho de dados.

Subscrever **não altera o comportamento do firmware** além de permitir o `hvx`
funcionar; logo o delta CONNECTED→TRANSMITTING é quase puro custo de rádio.

**GPIOs livres para marcadores** (pads não conectados, confirmados contra o
esquemático — o módulo é um Seeed XIAO nRF52840): **P0.02 (D0), P0.03 (D1),
P1.14 (D9), P1.15 (D10)**. Não usar P0.18 (`CONFIG_GPIO_AS_PINRESET`).

**Ganho grátis imediato:** o **DRDY# em P0.29 é fisicamente pulsado pelo ADC** e
ignorado pelo firmware — dá para plugar um canal digital da PPK2 nele **agora, com
zero alteração de firmware**, e ver os pulsos de conversão.

**Host** — `bleak` 3.0.2, `numpy`, `matplotlib`, `pyserial`, `ppk2-api` instalados.
**`scipy` e `pandas` NÃO estão** (e o código DSP existente os importa). Não existe
cliente BLE de PC no repo; o `esp_dongle_ble/` está obsoleto (nome e UUIDs do
protótipo Arduino antigo, decodifica 1 amostra em vez de 60) — **descartar**.

**Builds** — `Debug` = `-O0` (é o que está gravado agora), `Release` =
`Optimize For Size`. `NRF_LOG` fica **habilitado nos dois** (backend RTT,
NO_BLOCK_SKIP → sem stall sem debugger). Há um **`ADS112C04.c` duplicado e obsoleto**
em `pca10056/s140/ses/` que **não** está no build — não editar esse.

## Decisões já tomadas

| Decisão | Escolha |
|---|---|
| Ponto de medição | Só o trilho de 5 V (J2), **nada invasivo** |
| Instrumentação | Baseline com firmware intocado primeiro; depois build instrumentado com GPIO markers |
| Taxa vs consumo | **Negociável** — entregar a curva de trade-off |
| Escopo | Medir + salvar + propor, **e implementar as otimizações seguras** com antes/depois |

## Plano de execução

### Fase 0 — Validação de bancada

1. **Efeito do J-Link.** Medir com (a) cabo desconectado, (b) conectado sem sessão,
   (c) `JLink.exe` conectado. Se (b) ≈ (a), pode ficar plugado; senão todo run
   oficial exige desconectar o cabo (passo manual do Bruno).
2. **Piso analógico irredutível** — nRF em `sd_power_system_off()` com a placa
   energizada: mede front-end + pull-ups + leakage, sem CPU nem rádio. **Define se
   2 mA é sequer possível.**
3. **Custo de cada consumidor**, isolando por firmware: LED1 aceso/apagado, ADC em
   powerdown (`ads112c04_powerdown`) e em RESET# baixo, UART desabilitada, I²C
   parado.
4. **Sanidade da PPK2**: `total_amostras/100000` vs relógio de parede (reportar
   deriva) e ausência de perda em captura longa.
5. **Probe do DRDY#** em P0.29 num canal digital → taxa real de conversão do ADC,
   sem tocar no firmware.

### Fase 1 — Baseline

- **Baseline-A**: HEAD como está, build `Release` (e `Debug` para quantificar o
  custo de `-O0`).
- **Baseline-B**: só as correções dos três bugs acima — referência para otimização.

Orquestrador Python percorre, com BLE real:

```
OFF → BOOT → ADVERTISING → CONNECTING → CONNECTED_IDLE → STREAMING
    → GAIN_CHANGE → DISCONNECT → RE_ADVERTISING → OFF
```

Mapeamento ao artigo: `ADVERTISING`=IDLE, `CONNECTED_IDLE`=CONNECTED (CCCD **não**
habilitado), `STREAMING`=TRANSMITTING. Novos: `BOOT` (inrush e energia de
inicialização — **crítico para supercapacitor**, ausente do artigo), `CONNECTING`,
`GAIN_CHANGE`, `DISCONNECT`, `RE_ADVERTISING`.

Durante `STREAMING` o host puxa os dados de verdade e valida que são EMG real.

### Fase 2 — Build instrumentado

- **GPIO markers de estado** em P0.02/P0.03/P1.14/P1.15 → canais digitais da PPK2,
  fronteiras com precisão de microssegundos no mesmo stream da corrente (um dos
  pinos pulsando por notificação, para medir energia por pacote).
- **Contador de sequência** no pacote BLE → detecção real de perda no host.
- **Corrigir o no-op do ganho**, habilitando o teste de resposta ADC→DS3502→BLE.

Habilita as análises que a Fig. 7 não tem: energia por evento de advertising, de
conexão, por notificação e por conversão do ADC — em µJ e µC, que é o que dimensiona
supercapacitor.

### Fase 3 — Curva de trade-off

Varrer e medir: taxa de amostragem (2k/1k/500 SPS); SCL a 100 vs 400 kHz; intervalo
de conexão (75–100 ms atual vs mais longo/curto); DLE 27 vs 251; PHY 1M vs 2M;
TX 0 vs −20 dBm; raw vs envelope RMS no dispositivo (~50 Hz → ~40× menos rádio);
streaming contínuo vs burst com duty-cycle. Entrega: curva consumo-vs-fidelidade.

### Fase 4 — Otimizações, medindo antes/depois de cada uma isoladamente

| # | Otimização | Ganho esperado | Risco |
|---|---|---|---|
| 1 | **Dormir de verdade**: usar o DRDY# (P0.29) como interrupção + I²C não-bloqueante em vez de busy-wait; SCL 400 kHz | **O maior de todos** | Médio — mexe no caminho de aquisição |
| 2 | Apagar LED1 (e resolver a parasita do LED2 no pino de RX) | −1,5 a −3 mA | Baixo |
| 3 | Habilitar DC/DC (`sd_power_dcdc_mode_set`) — hoje desabilitado (`POWER_CONFIG_DEFAULT_DCDCEN 0`) | −30 a −50% do nRF | Médio — validar com a PPK2 vigiando brownout |
| 4 | Remover UART0 (imprime em pino não conectado) e `NRF_LOG` no build de medição; usar `Release` | Moderado | Baixo |
| 5 | Parar aquisição/ADC quando desconectado (`ads112c04_powerdown`) | Grande no estado IDLE | Baixo |
| 6 | DLE 27 → 251 (hoje cada notificação vira ~6 PDUs) | Moderado no TRANSMITTING | Baixo |
| 7 | Cachear o CCCD em vez de `sd_ble_gatts_value_get` a cada pacote | Pequeno | Baixo |
| 8 | TX −20 dBm e advertising 500 ms (implementar o que o artigo afirma) | Pequeno a moderado | Baixo (reduz alcance) |

**Só recomendação, não implementar (hardware):** pull-ups 4k7 → 10k+; front-end e
ADS112C04 a 3,3 V em vez de 5 V; alimentar o módulo pelo pad 3V3 evitando a perda do
regulador interno 5→3,3 V (~34% do consumo do nRF); avaliar o consumo de repouso do
próprio XIAO nRF52840 (carregador de bateria e LED on-board).

## Arquivos

Tudo sob `power_profiling/`. **Manter:** `ppk2_check.py`, `ppk2_hold_on.py`.

| Criar | Responsabilidade |
|---|---|
| `ppk2_stream.py` | Captura contínua sem perda (`PPK2_MP`), timestamps derivados, decodificação de D0–D7 |
| `ble_emg_client.py` | Central BLE com `bleak`: scan `EMG_BLE`, connect, subscribe `19b10002-…`, decodifica 60×int16, escreve ganho, callbacks de transição |
| `orchestrate_states.py` | Percorre a sequência de estados, controla a PPK2 (power-cycle para o `BOOT`), grava timeline e artefatos |
| `analyze.py` | Estatísticas por estado, energia por evento, autonomia, todas as figuras |
| `emg_dsp.py` | DSP compartilhado, consolidando o que hoje está **duplicado** em `data/processdata/processdata.py:51-73` e `processData/featuresPlot.py:7-30`, com **`fs` como parâmetro** (hoje é 2000 hardcoded em três lugares) |
| `requirements.txt` | Fixar dependências (não existe nenhum hoje) + instalar `scipy`/`pandas` |

**Refatorar:** `ppk2_capture.py` passa a usar o núcleo de `ppk2_stream.py`, mantendo
os arquivos de saída atuais por compatibilidade.

### Arquitetura de execução (ponto de maior risco técnico)

**Processo único.** A PPK2 derruba o VOUT quando a conexão serial fecha, e a placa
não tem outra fonte — então o processo que mede tem que ser o mesmo que vive do
início ao fim do run. Dois processos separados arriscam resetar a placa no meio.

**Concorrência:** `bleak` é asyncio; a leitura da PPK2 é serial bloqueante. Captura
da PPK2 vai numa **thread de background** (via `PPK2_MP`, que já implementa fetch
próprio com ring buffer), e o BLE fica no loop asyncio principal. O
`PPK2_API` simples com polling de 100 ms — o que o `ppk2_capture.py` atual faz —
**perde amostras** em captura longa; a 100 kS/s são 400 kB/s de serial.

**Alinhamento temporal:** relógio comum via `time.monotonic()`. O tempo da amostra
vem de `índice/100000`, e a deriva é medida e reportada comparando
`total_amostras/100000` com a duração de parede — se divergir, houve perda e o run
é invalidado. Com janelas de estado de 10–30 s, erro de dezenas de ms é aceitável na
Fase 1. Na Fase 2 os GPIO markers nos canais digitais D0–D7 dão fronteiras exatas no
mesmo stream, e a análise re-ancora a timeline do host detectando as bordas.

**Firmware** (`emg_nrf_ses/project/ble_peripheral/ble_app_blinky/`): `main.c`,
`ble_emg_service.c/h`, `ADS112C04.c` (o da **raiz**, não o de `ses/`),
`pca10056/s140/config/sdk_config.h`.

## Artefatos e figuras

Por run: `power_profiling/results/<run-id>/` com `current.npy` + `meta.json` (traço
cheio a 100 kS/s — CSV de 30 M linhas é inviável; mais uma versão decimada para
plot), `emg_samples.csv` (**com `fs` e ganho registrados** — os CSVs existentes não
têm, o que os tornou ambíguos), `timeline.json`, `summary.csv` (sucessor da Tabela 2:
média **e** RMS, p95, p99, min/max, desvio), `figs/*.png` a 300 dpi, `REPORT.md`.

| Fig | Conteúdo | O que a Fig. 7 não tinha |
|---|---|---|
| A | Ciclo completo com faixas por estado | 10 estados em vez de 4, registro muito mais longo, sem corte no eixo Y, média/RMS anotados |
| B | Zooms: evento de conexão, advertising, notificação, conversão do ADC | Inexistente — resolução de µs numa janela longa |
| C | Distribuições por estado (histograma/violino/CDF) | Inexistente — o RMS único esconde a dispersão |
| D | Energia e carga por evento (µJ, µC) | Inexistente — métrica que dimensiona supercap |
| E | Autonomia: bateria 400 mAh e dimensionamento de supercap | Inexistente — o artigo não tem autonomia nenhuma |
| F | Validação: forma de onda EMG + FFT + taxa verificada | Inexistente — prova que o dado é real |
| G | Antes/depois de cada otimização | Inexistente |
| H | Curva consumo vs fidelidade | Inexistente |

## Validação de integridade dos dados

- **Taxa de pacotes** medida vs esperada (a taxa efetiva é ditada pelo I²C, então
  medir, não assumir — e comparar com os pulsos de DRDY# capturados na PPK2)
- **Perda de pacotes**: Fase 1 só por inferência (sem sequência); Fase 2 direta
- **Sanidade estatística**: não constante, não saturado nos limites do int16
- **Espectro** na banda mioelétrica de 20–500 Hz
- **Teste de resposta ao ganho**: só possível **após** corrigir o no-op da Fase 2

## Verificação

1. `python power_profiling/ppk2_check.py --port COM8` → OK (já validado).
2. `bash .claude/skills/build-flash-nrf52/scripts/build_and_flash.sh Release` →
   compila e grava (pipeline já validado nesta máquina).
3. Rodar o orquestrador: `timeline.json` com as transições, `emg_samples.csv` com
   taxa medida coerente com os pulsos de DRDY#, espectro na banda correta,
   `summary.csv` com os 4 estados do artigo preenchidos.
4. Comparar com a Tabela 2 (2,922 / 8,497 / 9,063 mA) — divergências esperadas e
   explicáveis (firmware evoluiu, TX 0 dBm em vez de −20, adv 200 ms, LEDs, build);
   documentar, não esconder.
5. Após cada otimização, re-rodar o mesmo run e comparar antes/depois.

## Riscos

- **2 mA pode exigir hardware.** A Fase 0 responde isso objetivamente; reporto o
  mínimo real atingível e as mudanças de HW necessárias.
- **A PPK2 é a única fonte da placa** — se cair, a placa reseta no meio do run; o
  processo de captura tem que ficar vivo do início ao fim.
- **MTU no Windows**: payload de 120 B exige MTU ≥ 123; verificar o MTU efetivo no
  início do run.
- **DC/DC pode não ter indutor** no módulo — testar com a PPK2 vigiando brownout.
- **Os dois `while(1);` de falha do ADC** (`main.c:676`, `main.c:686`) parecem estado
  válido mas são CPU girando: detectar essa assinatura para não medir errado.
- **Comparabilidade com o artigo é limitada por construção** — instrumentação
  original não documentada e firmware diferente. O relatório declara isso.
