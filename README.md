# EMG_BLE - Firmware nRF52840

Firmware profissional para aquisição e transmissão de sinais EMG via Bluetooth Low Energy (BLE) usando nRF52840, ADS112C04 e DS3502.

Este é o firmware do sensor sEMG vestível de baixo consumo apresentado no artigo
**"A Low-Power Bluetooth LE Surface EMG Sensor"** (LEB/UFMG, SEB 2025).
Detalhes em [Publicação Científica](#-publicação-científica).

> **⚠️ Este README descreve o firmware ATUAL, que divergiu do artigo.**
> Vários parâmetros publicados não correspondem ao código (TX power,
> intervalo de advertising, PHY, taxa de pacotes), e a aquisição do ADC
> **não funcionava**: sem nenhuma fonte de tempo governando a amostragem, o
> ADC era lido ~1 vez por segundo em vez dos 2 kS/s reivindicados. Corrigido —
> hoje são 2042 S/s medidos com 0% de conversões perdidas. Consumo e
> desempenho medidos com PPK2 em
> [Consumo e desempenho medidos](#-consumo-e-desempenho-medidos); as
> divergências estão anotadas em cada seção.

## 📄 Publicação Científica

Este repositório contém o firmware do sensor descrito no artigo:

> **A Low-Power Bluetooth LE Surface EMG Sensor**
> Robert Ribeiro Gomes, **Bruno Ribeiro Chaves**, Renan Fernandes Kozan, Dalton Martini Colombo
> Laboratório de Engenharia Biomédica (LEB), Universidade Federal de Minas Gerais (UFMG), Belo Horizonte, Brasil
> **XVI Simpósio de Engenharia Biomédica (SEB 2025)** — Universidade Federal de Uberlândia (UFU)
> Apresentação oral em 16/09/2025 · Anais com ISSN 2358-3568
> 📎 [Artigo completo (PDF)](663744.pdf) · 🔗 [Site do evento](https://seb2025.sciencesconf.org/)

**ORCID dos autores**

| Autor | ORCID | Vínculo |
|---|---|---|
| Robert Ribeiro Gomes | 0000-0003-3831-2264 | Mestrando — PPGEE/UFMG |
| Bruno Ribeiro Chaves | 0009-0000-4130-3209 | Graduando em Engenharia Elétrica — UFMG |
| Renan Fernandes Kozan | 0000-0002-1056-0904 | Professor — UFMG |
| Dalton Martini Colombo | 0000-0002-6781-9673 | Professor — UFMG |

Trabalho desenvolvido no **LEB — Laboratório de Engenharia Biomédica da UFMG**.

### Resumo

Apresentamos um sistema de eletromiografia de superfície (sEMG) de baixo consumo baseado em
Bluetooth Low Energy (BLE), projetado para monitoramento muscular vestível. O dispositivo integra
uma cadeia de aquisição totalmente customizada — filtragem analógica, condicionamento de sinal,
reprodução de formas de onda a partir de um dataset e transmissão sem fio via BLE — centrada em um
ADC ΔΣ de 16 bits e um SoC Nordic nRF52840. Para avaliar o desempenho, um movimento representativo
de *sidekicking* (chute lateral) de um dataset público de sEMG foi fisicamente reproduzido e gravado
simultaneamente pelo dispositivo proposto e por um sistema EMG de grau clínico. As análises nos
domínios do tempo e da frequência mostram que o sistema preserva as características principais dos
sinais mioelétricos na banda de 30–400 Hz. Os resultados sustentam sua adequação para aplicações em
ciências do esporte, reabilitação e interação humano–máquina.

**Palavras-chave:** Wearable EMG, low-power, Bluetooth Low Energy, signal-to-noise ratio, sports monitoring.

### Arquitetura do sensor (artigo)

A cadeia de aquisição descrita no artigo é dividida em sete blocos: pré-amplificação, filtro
passa-alta (HPF), filtro passa-baixa (LPF), amplificador de ganho ajustável, conversão A/D, circuito
de tensão de referência e microcontrolador.

| Bloco | Componente | Detalhes |
|---|---|---|
| Eletrodos | Ag/AgCl com gel condutivo | 3 eletrodos (2 diferenciais + referência), espaçamento de 25 mm |
| Amplificador de instrumentação | **INA317** | 50 µA, offset de 75 µV, CMRR de 110 dB, ganho fixo de 35× |
| Proteção de entrada | R1 (sobrecorrente), R2/C2/C3 (EMI/ESD), diodos BAT54S (sub/sobretensão) | — |
| Filtro passa-banda | Sallen-Key de 2ª ordem (Butterworth) | HPF 20 Hz (medido 30 Hz), LPF 482 Hz (medido 400 Hz), ganho 1,55 por estágio |
| Ganho ajustável | **DS3502** (potenciômetro digital de 7 bits, I²C) | 10 kΩ máx., ganho de 2× a 11× |
| ADC | **ADS112C04** | 16 bits, ΔΣ, I²C, 4 canais diferenciais, até 2 kS/s, ~315 µA, PGA 1×–128× |
| Level shifter | **MAX6106** (ref. 2,048 V) + **MCP609** | Topologia inversora, desloca o sinal para a faixa 0–5 V |
| MCU / rádio | **nRF52840** | Cortex-M4F 64 MHz, 1 MB flash, 256 kB RAM, Bluetooth 5.4 (LE, Long Range) |

Alimentação: bateria Li-Ion de célula única 400 mAh @ 3,7 V, com conversor step-up de alta eficiência
e regulador de baixo ruído gerando os trilhos do front-end analógico e do microcontrolador.

**Tabela 1 — Características do sensor sEMG proposto**

| Descrição | Valor |
|---|---|
| Número de canais | 1 |
| Frequência de amostragem (*f*<sub>s</sub>) | 2 kS/s |
| Resolução A/D (α) | 16 bits |
| Banda passante (*BW*) | 30 Hz – 400 Hz |
| Ganho ajustável (*G*) | 1× – 11× |
| Comunicação | BLE 5.1 |
| Receptor | Computador / smartphone |
| Alimentação (*V*<sub>cc</sub>) | 5 V |
| Bateria | Li-Ion 400 mAh @ 3,7 V |
| Dimensões | 55 mm × 30 mm |
| Material dos eletrodos | Ag/AgCl |

Na configuração caracterizada no artigo, o firmware agrega dados de 1 kS/s em notificações BLE de
20 Hz, reduzindo significativamente o tempo de rádio no ar sem sacrificar resolução.

### Protocolo de validação

O dataset utilizado é o *EMG Physical Action Data Set* (UCI Machine Learning Repository, 2011):
4 voluntários (3 homens e 1 mulher, 20–30 anos), 20 sessões cada, com 10 ações normais e 10
agressivas. Os sinais foram adquiridos com sistema Delsys de 8 eletrodos de superfície em braços
(bíceps/tríceps) e pernas (quadríceps/isquiotibiais), na Essex Robotic Arena (4 × 5,5 m), com saco
de pancadas de 1,75 m — ~10.000 amostras por canal em cada sessão.

Um movimento de *sidekicking* foi **fisicamente reproduzido** e gravado simultaneamente pelo
protótipo e por um sistema EMG clínico. Pipeline de processamento unificado para ambos os registros:

1. Normalização para [-1, 1]
2. Detecção de envelope por RMS em janelas deslizantes de 50 ms
3. Segmentos ativos identificados quando o envelope excede 20% do pico do sinal
4. Estimativa de ruído por RMS dos segmentos de repouso
5. SNR calculada como:

```
SNR_dB = 20 · log10( RMS_signal / RMS_noise )
```

### Resultados

**SNR (domínio do tempo)**

| Sinal | SNR |
|---|---|
| Dataset original | 10,7 dB |
| **Sistema proposto** | **9,0 dB** |
| Sistema clínico | 12,7 dB |

Os três sinais apresentam forte alinhamento temporal, com picos de ativação sincronizados, e as
regiões de contração detectadas automaticamente foram similares em todos os casos. O sistema
proposto exibe ruído de alta frequência ligeiramente maior, mas mantém picos bem definidos e
consistência morfológica.

**Resposta em frequência** — Ambos os sistemas apresentam ganho consistente na faixa fisiologicamente
relevante. O sistema proposto exibe ganho geralmente mais alto (maior sensibilidade, com maior
vulnerabilidade a ruído e saturação) e variações de fase mais abruptas em altas frequências
(*group delay* não uniforme), enquanto o clínico apresenta deslocamento de fase mais linear e
gradual — desejável para preservar a integridade temporal de eventos rápidos como o início da
contração.

**Tabela 2 — Corrente RMS e potência por estado de operação (5,0 V)**

| Estado | Corrente (mA) | Potência (mW) |
|---|---|---|
| OFF (0 V) | 0 | 0 |
| IDLE | 2,922 | 14,61 |
| CONNECTED | 8,497 | 42,49 |
| TRANSMITTING | 9,063 | 45,32 |

Os quatro modos de operação são: **OFF** (sistema desligado), **IDLE** (eletrônica energizada, sem
conexão com um central BLE), **CONNECTED** (pareado, mas a característica BLE não está sendo lida) e
**TRANSMITTING** (característica lida ativamente, pacotes enviados periodicamente).

> **Reconciliação com as medições atuais (PPK2).** Remedindo com o firmware
> corrigido: CONNECTED e TRANSMITTING **reproduzem** os valores publicados
> (9,14 vs 8,50 mA e 9,06 vs 9,06 mA RMS). Já o IDLE ficou **~2,9× maior**
> (8,46 vs 2,92 mA) — e isso não é regressão: no firmware do artigo não havia
> fonte de tempo governando a amostragem, então o ADC era lido ~1 vez por
> segundo e o "IDLE" era um dispositivo genuinamente dormindo. Os 2 kS/s que o
> artigo reivindica não estavam acontecendo. Hoje o dispositivo adquire
> 2042 S/s continuamente, conectado ou não. Ou seja: **o envelope de potência
> publicado se reproduz, mas o dispositivo agora entrega ~2000× mais amostras
> pela mesma energia.** Detalhes em
> [`power_profiling/relatorio_consumo.pdf`](power_profiling/relatorio_consumo.pdf).

**Tabela 3 — Comparação com outros sistemas de detecção sEMG**

| Sistema | Canais | Resolução ADC (bits) | Transmissão | Banda (Hz) | Taxa (kS/s) |
|---|---|---|---|---|---|
| **Este trabalho** | 1 | 16 | BLE | 30 – 400 | 2,0 |
| Imperatori et al., 2013 | 4 | 12 | BLE | 8,7 – 952 | 4,0 |
| Tang et al., 2012 | 6 | 10 | RF | 20 – 500 | 1,0 |
| Cerone et al., 2019 | 32 | 16 | Wi-Fi | 10 – 500 | 2,0 |
| Ahamed et al., 2015 | 6 | 10 | Bluetooth | 0,03 – 564 | 1,2 |
| Chen et al., 2017 | 8 | 12 | Cabeado | 20 – 450 | 1,0 |

**Tabela 4 — Comparação de consumo entre arquiteturas de hardware sEMG**

| Arquitetura | Consumo |
|---|---|
| **Este trabalho** | **45,32 mW** |
| Dispositivo clínico (Delsys Trigno) | 65 mW |
| Chen et al., 2020 | 62,7 mW |
| Simić et al., 2024 | 67,65 mW |

O nó transmite a 2 kS/s consumindo apenas 45 mW — cerca de **30% menos** que sistemas similares
reportados na literatura.

### Estratégias de baixo consumo no firmware

Estratégias **descritas no artigo**:

- Todos os periféricos não utilizados são desabilitados quando o rádio está desconectado
- Rotina de gerenciamento de energia da Nordic invocada durante os períodos de *idle*
- Potência de transmissão BLE reduzida para **-20 dBm**
- Intervalo de *advertising* estendido de 40 ms (padrão) para **500 ms**, reduzindo o *duty cycle* do rádio

> **Verificação contra o código.** Duas dessas quatro **não estão
> implementadas**: `sd_ble_gap_tx_power_set` nunca é chamado (TX fica no default
> de 0 dBm) e o advertising é de 200 ms, não 500 ms. A terceira ("periféricos
> desabilitados quando desconectado") também não se sustenta: a aquisição roda
> idêntica desconectado, conectado e transmitindo, e `ads112c04_powerdown()`
> existe mas nunca é chamado. Só a rotina de gerenciamento de energia
> (`nrf_pwr_mgmt_run` → `sd_app_evt_wait`) está de fato no loop.
>
> Medido, isso importa menos do que parece: o rádio inteiro custa 0,48 mA de
> 6,80 mA, então implementar TX a −20 dBm renderia menos de 3%. O consumo está
> na **aquisição** (~4,1 mA). Otimizações efetivamente aplicadas e as
> pendentes estão em
> [Otimizações implementadas](#otimizações-implementadas).

### Conclusão e trabalhos futuros

O sistema captura sinais mioelétricos de forma confiável durante movimentos intensos, com resposta
temporal bem alinhada ao sinal de referência e desempenho comparável a soluções estabelecidas em
termos de resolução, banda e taxa de amostragem. Embora limitado a um único canal, entrega esse
desempenho em um dispositivo compacto e de baixo consumo. Trabalhos futuros focam em **aquisição
multicanal** e em **testes com voluntários humanos**, após aprovação de comitê de ética, para
consolidar o sistema como solução robusta para uso além do ambiente clínico.

### Financiamento

Trabalho financiado em parte pela CAPES (Finance Code 001), pelo Instituto Serrapilheira
(grant Serra-2211-42117) e pela FAPEMIG (APQ-05837-23).

### Como citar

```bibtex
@inproceedings{gomes2025lowpower,
  title     = {A Low-Power Bluetooth LE Surface EMG Sensor},
  author    = {Gomes, Robert Ribeiro and Chaves, Bruno Ribeiro and
               Kozan, Renan Fernandes and Colombo, Dalton Martini},
  booktitle = {Anais do XVI Simp\'osio de Engenharia Biom\'edica (SEB 2025)},
  year      = {2025},
  month     = {9},
  address   = {Uberl\^andia, MG, Brasil},
  publisher = {Universidade Federal de Uberl\^andia},
  issn      = {2358-3568},
  url       = {https://seb2025.sciencesconf.org/}
}
```

> **Nota:** o artigo descreve a configuração de referência caracterizada em laboratório
> (banda analógica 30–400 Hz, notificações BLE agregadas em 20 Hz, -20 dBm). O firmware neste
> repositório evoluiu para um modo de streaming de alta taxa (60 amostras/pacote, ~250 pacotes/s,
> filtro digital 20–500 Hz). As seções abaixo documentam os parâmetros atuais do firmware.

---

## 🎯 Características

Valores **medidos** no firmware atual (ver [Consumo e desempenho medidos](#-consumo-e-desempenho-medidos)):

- **Aquisição** — ADS112C04, 16 bits, **2042 S/s medidos**, governada pela interrupção de DRDY#, 0% de conversões perdidas
- **Streaming BLE** — 60 amostras/pacote, 120 B por notificação, **~32 pacotes/s**, MTU 247, DLE 251
- **Controle de ganho remoto** — DS3502 via I²C (ver ressalva em [Limitações conhecidas](#-limitações-conhecidas))
- **Filtragem digital** — Butterworth passa-banda de 2ª ordem, 20–400 Hz, coeficientes amarrados à taxa de amostragem
- **Consumo** — 6,80 mA a 5,0 V; **4,37 mA a 3,3 V**; 2,75 mA a 3,3 V com o ADC a 1 kSPS
- **Buffer circular** — FIFO de 64 amostras, pacotes de 60
- **Diagnóstico** — contadores em RAM legíveis via J-Link, independentes do BLE

## 🔧 Hardware

### Componentes Principais
- **MCU**: nRF52840 (módulo Seeed XIAO nRF52840)
- **ADC**: ADS112C04 (Texas Instruments) — **16 bits**, delta-sigma, I²C
- **Potenciômetro Digital**: DS3502 (Maxim) — 10 kΩ, I²C
- **Programador**: J-Link (SEGGER)

### Pinout
```
TWI0 (I2C) @ 400 kHz:
- SDA:  P0.04
- SCL:  P0.05

Outros:
- DRDY# do ADC: P0.29  (interrupção que governa a amostragem)
- RESET# do ADC: P0.28
- LED1: P1.13   |  LED2: P1.12 (compartilhado com o RX da UART)
- Livres para instrumentação: P0.02, P0.03, P1.14, P1.15

Endereços I2C:
- ADS112C04: 0x40
- DS3502:    0x28
```

### Configuração ADS112C04
```c
Data rate reg: DR=110 (0x06)
Modo:          turbo    -> 2000 SPS   (ADS_TURBO_MODE = 1, default)
               normal   -> 1000 SPS   (ADS_TURBO_MODE = 0, -37% de consumo)
Resolução:     16 bits com sinal
Entrada:       AIN0 vs AVSS (single-ended)
PGA:           habilitado, ganho 1x (ajuste fino via DS3502)
Referência:    AVDD
Conversão:     contínua, leitura disparada por DRDY#
```

`ADS_TURBO_MODE` vive em `ADS112C04.h` e define também qual conjunto de
coeficientes do filtro digital `main.c` usa — os dois não podem sair de
sincronia.

### Configuração DS3502
```c
Resistance: 0-10kΩ (127 steps)
Interface: I2C write-only
Mapping: Level 1-10 → 0x00-0x7F linear
Update: Real-time via BLE command
```

## 📡 Protocolo BLE

### GAP (Generic Access Profile)
```c
Device Name:           "EMG_BLE"
Advertising Interval:  320 units (200 ms), sem timeout
Connection Interval:   75-100 ms (60-80 units)   // negociado ~93 ms com Windows
Slave Latency:         0
Supervision Timeout:   6000 ms
PHY:                   BLE 1M   // escolhido por consumo, não 2M
TX Power:              0 dBm (default da SoftDevice)
GAP Event Length:      24 units (30 ms)
Data Length Extension: 251
```

> O artigo menciona TX a −20 dBm e advertising de 500 ms. **Nenhum dos dois
> está implementado**: `sd_ble_gap_tx_power_set` nunca é chamado, e o
> advertising é de 200 ms. Medido, o rádio inteiro custa apenas 0,48 mA
> (diferença entre ADVERTISING e STREAMING), então reduzir a potência de TX
> renderia menos de 3% do consumo total.

### GATT (Generic Attribute Profile)

#### EMG Service
```c
Service UUID: 19b10001-1000-e8f2-537e-4f6cd168a114

Characteristics:
1. EMG Data (NOTIFY)
   UUID:   19b10002-1000-e8f2-537e-4f6cd168a114
   Format: array de int16_t little-endian, 60 amostras
           (reinterpret-cast cru: sem header, sequência ou timestamp)
   Size:   120 bytes por notificação (123 B de PDU ATT)
   Rate:   ~32 pacotes/s medidos  (= 2042 S/s ÷ 60)

2. Gain Control (WRITE)
   UUID:   19b10003-1000-e8f2-537e-4f6cd168a114
   Format: uint8_t (1-10)
   Action: atualiza o wiper do DS3502  -- ver Limitações conhecidas
```

Decodificação no host: `np.frombuffer(data, dtype='<i2')`. Cliente BLE de
referência em [`power_profiling/ble_client.py`](power_profiling/ble_client.py)
(o `esp_dongle_ble/` está obsoleto: nome e UUIDs do protótipo Arduino antigo).

### MTU Negotiation
```c
Default MTU: 23 bytes
Negotiated MTU: 247 bytes
Payload útil: 244 bytes
Packet overhead: 3 bytes (ATT header)
```

## 🚀 Instalação

### Pré-requisitos
- **SEGGER Embedded Studio** 8.30a (testado; 8.24+ deve servir) — precisa de
  licença gratuita para dispositivos Nordic, obtida em
  [license.segger.com/Nordic.cgi](https://license.segger.com/Nordic.cgi) e
  instalável pela CLI: `emLicense.exe install '<chave>'`
- **nRF5 SDK** 17.1.0 (incluído em `emg_nrf_ses/`)
- **SoftDevice** S140 **v7.2.0** (`s140_nrf52_7.2.0_softdevice.hex` na raiz)
- **J-Link Software** — o driver USB do probe é instalado separadamente por
  `<install>/USBDriver/InstDrivers.exe`, que exige privilégio de administrador

### Build e Flash

Este projeto padroniza a **CLI da SEGGER** (`emBuild.exe` e `JLink.exe`).
**Não use `nrfjprog`.** Os scripts prontos resolvem os caminhos de instalação
automaticamente, inclusive pastas versionadas (`JLink_V970`,
`SEGGER Embedded Studio 8.30a`):

```bash
# Loop do dia a dia: compila e grava
bash .claude/skills/build-flash-nrf52/scripts/build_and_flash.sh Release

# Placa nova ou recém-apagada: erase + SoftDevice + app
bash .claude/skills/build-flash-nrf52/scripts/flash_all.sh Release

# Passos individuais
bash .claude/skills/build-flash-nrf52/scripts/build.sh Release
bash .claude/skills/build-flash-nrf52/scripts/flash_app.sh Release
bash .claude/skills/build-flash-nrf52/scripts/flash_softdevice.sh
bash .claude/skills/build-flash-nrf52/scripts/erase.sh
bash .claude/skills/build-flash-nrf52/scripts/rtt_log.sh rtt.txt
```

Se um executável reclamar de arquivo inexistente num caminho que claramente
existe: os `.exe` da SEGGER são binários Windows nativos e não entendem
caminhos `/c/...` do git-bash. Os scripts convertem via `cygpath -w` antes de
passar qualquer caminho — faça o mesmo em comandos ad-hoc.

**A placa precisa estar alimentada para o J-Link conectar.** Se não houver
bateria, alimente pela PPK2 antes de gravar:

```bash
python power_profiling/ppk2_hold_on.py --port COM8 --voltage-mv 5000 &
```

## 📊 Processamento de Sinal

### Pipeline de Dados
```
ADS112C04 --DRDY#--> leitura I2C --> Butterworth --> FIFO --> notificação BLE
 2042 S/s            400 kHz         20-400 Hz     64 amostras   ~32 pkt/s
```

### Filtro Butterworth

Passa-banda de 2ª ordem (4 polos / 4 zeros), 20–400 Hz. **Os coeficientes
dependem da taxa de amostragem** e são selecionados pelo mesmo
`ADS_TURBO_MODE` que define a taxa do ADC — taxa sem conjunto de coeficientes
vira `#error` em tempo de compilação.

| Taxa | GAIN | Coeficientes de realimentação |
|---|---|---|
| 2000 SPS | 5.182411747 | −0.2066719852 · 0.8192636853 · −1.9509646898 · 2.3350824021 |
| 1000 SPS | 1.715890586 | −0.3476653949 · −0.1939361276 · 0.8157085862 · 0.6874450146 |

Gerados com [mkfilter](https://github.com/brunorchaves/mkfilter_in_python):

```bash
python mkfilter.py -Bu -Bp -o 2 -f 20 400 -s 2000 -c    # 2 kSPS
python mkfilter.py -Bu -Bp -o 2 -f 20 400 -s 1000 -c    # 1 kSPS
```

Resposta verificada numericamente nos dois casos: cortes de −3 dB em
exatamente 20,0 Hz e 400,0 Hz.

**Sobre anti-aliasing:** o ADS112C04 é delta-sigma. O modulador roda a
centenas de kHz e o filtro digital interno atenua de f<sub>DR</sub>/2 até
f<sub>MOD</sub>, onde a resposta se repete. O filtro analógico da placa
(Sallen-Key de 2ª ordem em 482 Hz) só precisa bloquear conteúdo perto de
f<sub>MOD</sub>, onde tem ~109 dB de folga. Ele está adequado nas duas taxas
e **não precisa ser alterado**.

### FIFO Buffer
```c
Size: 64 amostras (int16_t), circular
Pacote: 60 amostras
Overflow: dado mais antigo descartado
Transmissão: quando o pacote fecha
```

## 🔬 Configurações BLE Avançadas

### RAM Allocation
```c
RAM_START: 0x20002C80   // no .emProject, não no sdk_config.h
RAM_SIZE:  0x3D380
```

Se a SoftDevice reclamar `Insufficient RAM allocated` no boot (visível via
RTT), ela informa o valor exato a usar — atualize `RAM_START`/`RAM_SIZE` no
`.emProject` e recompile. Aumentar `GAP_EVENT_LENGTH`, `ATTR_TAB_SIZE` ou a
fila de notificações consome RAM da SoftDevice e costuma exigir esse ajuste.

### BLE Stack Config
```c
NRF_SDH_BLE_GATT_MAX_MTU_SIZE:      247
NRF_SDH_BLE_GAP_DATA_LENGTH:        251   // era 27: cada notificação de
                                          // 123 B virava ~6 PDUs de link
NRF_SDH_BLE_GAP_EVENT_LENGTH:        24   // era 6 (7,5 ms) e limitava o
                                          // throughput a ~1,7 pkt/evento
NRF_SDH_BLE_GATTS_HVN_TX_QUEUE_SIZE:  6   // era 1 (default do SDK)
NRF_SDH_BLE_VS_UUID_COUNT:           10
NRF_SDH_BLE_GATTS_ATTR_TAB_SIZE:   1408
```

### Connection Parameters
```c
MIN_CONN_INTERVAL: MSEC_TO_UNITS(75,   UNIT_1_25_MS)   // 60 units
MAX_CONN_INTERVAL: MSEC_TO_UNITS(100,  UNIT_1_25_MS)   // 80 units
SLAVE_LATENCY:     0
CONN_SUP_TIMEOUT:  MSEC_TO_UNITS(6000, UNIT_10_MS)     // 6 s
```

## 📝 Estrutura do Código

```
emg_nrf_ses/project/ble_peripheral/ble_app_blinky/
├── main.c                    # Loop principal e inicialização
├── ble_emg_service.c/h       # Serviço BLE customizado
├── ADS112C04.c/h            # Driver I2C para ADC
├── sdk_config.h             # Configurações do nRF SDK
└── pca10056/s140/ses/       # Projeto SEGGER Embedded Studio

Principais Funções:
- main()                     # Inicialização e loop principal
- ble_emg_service_init()     # Setup do serviço EMG
- ads112c04_init()           # Configuração do ADC
- ds3502_set_resistance()    # Controle de ganho
- butterworth_filter()       # Processamento de sinal
- ble_emg_service_notify_packet() # Transmissão BLE
```

## 🐛 Debug e Logs

### RTT Viewer (SEGGER)
```bash
# Conectar J-Link RTT Viewer
JLinkRTTViewer

# Logs disponíveis:
- Inicialização de periféricos (I2C, Timer, BLE)
- Estatísticas de transmissão BLE
- Erros de notificação (CCCD, MTU, busy)
- Mudanças de ganho DS3502
- Eventos de conexão/desconexão
```

### UART — desabilitada

A UART0 está **desligada** (`ENABLE_UART 0` em `main.c`) por dois motivos
independentes:

1. O TX (P1.11) **não está conectado a nada na PCB** — o firmware imprimia em
   pino solto, gastando energia sem destino.
2. O RX (P1.12) é o **mesmo pino do LED2**, cujo ânodo fica no trilho de 5 V
   via 1 kΩ. Com o pino configurado como entrada de UART, corria corrente pelos
   diodos de proteção — era a causa do brilho basal permanente do LED2.

Ambos os pinos ficam agora como saída em nível alto. Para depuração use **RTT**,
que é o backend do `NRF_LOG`. Reativar a UART é mudar `ENABLE_UART` para 1, mas
lembre que isso reacende o LED2.

### Contadores de diagnóstico

Independentes do BLE e do RTT, legíveis via J-Link com
[`power_profiling/fw_counters.py`](power_profiling/fw_counters.py):

| Contador | O que indica |
|---|---|
| `g_init_status` | bitmask de progresso do init (twi, ads, ds3502, loop, drdy) |
| `g_loop_count` | iterações do loop principal — prova que o `main()` está vivo |
| `g_drdy_count` | interrupções de DRDY# = taxa real de conversão do ADC |
| `g_adc_ok_count` | leituras bem-sucedidas — comparado com `g_drdy_count` dá a perda |
| `g_notify_ok_count` / `g_notify_err_count` | notificações BLE enviadas e recusadas |
| `g_block_drop_count` | blocos de 60 amostras descartados |
| `g_last_raw` / `g_last_filtered` | última amostra, antes e depois do filtro |

## 📈 Consumo e desempenho medidos

Medições com **Nordic Power Profiler Kit II** (PCA63100) em source meter a
100 kS/s, captura verificada sem perda de amostra. Relatório completo em
**[`power_profiling/relatorio_consumo.pdf`](power_profiling/relatorio_consumo.pdf)**;
metodologia em [`power_profiling/PLANO.md`](power_profiling/PLANO.md).

### Métricas de transmissão (medidas)
```
Taxa de aquisição:  2042 S/s   (0% de conversões do ADC perdidas)
Pacotes:            ~32/s de 120 B  =  ~3,8 kB/s
Amostras entregues: ~1900 S/s  (~8% de blocos descartados pelo firmware)
Continuidade:       0,997      (junções suaves entre pacotes consecutivos)
Intervalo conexão:  ~93 ms negociado com Windows
```

### Consumo por estado

A 5,0 V, com LED e UART desligados:

| Estado | Corrente média | Potência | Pico real* |
|---|---|---|---|
| OFF | 0,000 mA | 0,00 mW | — |
| BOOT | 6,79 mA | 33,96 mW | 28,1 mA |
| ADVERTISING | 6,80 mA | 34,02 mW | 28,8 mA |
| CONNECTED (sem CCCD) | 6,83 mA | 34,16 mW | 26,3 mA |
| STREAMING | 7,28 mA | 36,40 mW | 29,6 mA |

\* Picos excluem a vizinhança das trocas de faixa de medição da PPK2, onde o
instrumento gera artefatos — sem essa exclusão o máximo aparente chegaria a
563 mA, fisicamente impossível nesta placa.

### Efeito da tensão de alimentação e da taxa do ADC

| Configuração | Corrente | Potência | Autonomia* |
|---|---|---|---|
| 5,0 V · 2 kSPS | 6,80 mA | 34,02 mW | 36,9 h |
| **3,3 V · 2 kSPS** | **4,37 mA** | **14,41 mW** | **87,0 h** |
| 3,3 V · 1 kSPS | 2,75 mA | 9,10 mW | 138,2 h |

\* Bateria de 400 mAh @ 3,7 V, conversor a 85%.

Tensão mínima de operação: o firmware roda até 2,0 V, mas **2,7 V é o piso com
todos os componentes em especificação** (limite inferior do DS3502). Abaixo de
~3,0 V a corrente para de cair — regulador do módulo em dropout.

A 3,3 V a referência de 2,048 V do MAX6106 deixa de estar perto do meio do
trilho e o pico positivo do sinal é ceifado ~16%. Correção: dividir a
referência para ~1,65 V com dois resistores antes do buffer U2.4.

### Para onde vai a corrente

- **Aquisição (ADC + I²C + CPU): ~4,1 mA** — obtido comparando o transiente de
  partida (2,69 mA antes do ADC ser configurado) com o regime (6,81 mA)
- **Rádio BLE: 0,48 mA** — diferença entre ADVERTISING e STREAMING, com o link
  entregando 32 pacotes/s. O rádio **não** é o gargalo
- **LED1 + UART: 1,35 mA** — já removidos

Os picos de corrente ocorrem a **~2000/s, um por conversão do ADC**, com
largura mediana de 20 µs — são a leitura I²C, não o rádio. Cada pico move
~0,4 µC, então ~100 µF de bulk local mantém o trilho dentro de ~5 mV e a fonte
(supercapacitor ou PMU) passa a ver só a média. **A placa não tem nenhum
capacitor de bulk hoje** — o maior é 220 nF.

### Otimizações implementadas
1. ✅ Aquisição governada pela interrupção de DRDY# (era sem fonte de tempo)
2. ✅ I²C a 400 kHz (era 100 kHz, perdia 11% das conversões)
3. ✅ Data Length Extension 251 (era 27)
4. ✅ Fila de notificações 6 e `GAP_EVENT_LENGTH` 24
5. ✅ LED1 e UART0 desligados
6. ✅ Pinos configurados no início do `main()` (evita LEDs acesos no boot)
7. ⬜ DC/DC do nRF52840 — ainda desabilitado (`POWER_CONFIG_DEFAULT_DCDCEN 0`)
8. ⬜ `ads112c04_powerdown()` quando desconectado — existe e nunca é chamado
9. ⬜ TWIM com EasyDMA disparado por PPI (hoje usa o driver legado `nrfx_twi`)

## 🔬 Ferramental de medição

[`power_profiling/`](power_profiling/) contém a bancada usada nas medições
acima:

| Script | Função |
|---|---|
| `run_bench.py` | Percorre 9 estados de operação num único stream contínuo da PPK2 |
| `fw_counters.py` | Lê os contadores de diagnóstico do firmware via J-Link, sem depender do BLE |
| `ble_client.py` | Central BLE (`bleak`) que puxa e decodifica as amostras |
| `voltage_sweep.py` | Mede corrente e potência a cada tensão de alimentação |
| `analyze.py` / `figures.py` / `report_pdf.py` | Estatísticas, figuras e o relatório em PDF |
| `emg_validate.py` | Valida que o stream é sinal real (taxa, continuidade, espectro) |

```bash
pip install -r power_profiling/requirements.txt
python power_profiling/ppk2_check.py --port COM8          # teste de comunicação
python power_profiling/fw_counters.py --watch 10          # taxa de aquisição real
python power_profiling/run_bench.py --port COM8 --voltage-mv 3300
```

## ⚠️ Limitações conhecidas

- **A escrita BLE de ganho é no-op.** `on_write` em `ble_emg_service.c` valida e
  loga o valor recebido mas nunca atribui `gain_level`, então o DS3502 fica no
  valor de boot. O caminho I²C funciona; falta o `gain_level = new_gain`.
- **~8% dos blocos de 60 amostras são descartados** quando a fila de
  notificações da SoftDevice enche. O host não consegue detectar isso
  diretamente porque o pacote não tem número de sequência — a detecção atual é
  por continuidade do sinal (o filtro IIR mantém estado entre pacotes).
- **A UART não é um caminho de dados.** Está desabilitada, e mesmo habilitada
  o TX (P1.11) não está conectado na PCB. BLE é o único caminho.
- **`esp_dongle_ble/` está obsoleto** — nome e UUIDs do protótipo Arduino
  antigo, e decodifica 1 amostra em vez de 60.
- **Não há isolamento de estados por subsistema.** O firmware nunca para a
  aquisição, então não existe um estado "só rádio" ou "só ADC" — o que o
  protocolo experimental pede para atribuir consumo por subsistema.

## 🔗 Integração com App Mobile

### Protocolo de Comunicação

**Leitura de Dados EMG**:
```typescript
// Subscribe para notificações
await device.monitorCharacteristicForService(
  SERVICE_UUID,
  EMG_DATA_UUID,
  (error, characteristic) => {
    const data = base64.decode(characteristic.value);
    // Array de 60 int16_t (120 bytes)
  }
);
```

**Controle de Ganho**:
```typescript
// Enviar comando de ganho (1-10)
const gainValue = 5; // 5x amplification
await device.writeCharacteristicWithResponseForService(
  SERVICE_UUID,
  GAIN_CONTROL_UUID,
  base64.encode([gainValue])
);
```

## 🛠️ Troubleshooting

### O primeiro diagnóstico: leia os contadores do firmware

Antes de suspeitar do BLE, confirme se o loop principal está vivo. A pilha BLE
responde por interrupção da SoftDevice **mesmo com o `main()` travado**, então
"conecta mas não chega dado" e "firmware travado" são indistinguíveis de fora:

```bash
python power_profiling/fw_counters.py --watch 10
```

Esperado com a placa saudável:
```
g_init_status = 0x3F  [twi_ok, ads_init_ok, ads_config_ok, ds3502_ok,
                       main_loop_entered, drdy_irq_ok]
taxa de amostragem efetiva: ~2040 S/s
conversoes do ADC nao lidas: 0 (0.0% perdidas)
```

- `g_init_status` sem o bit `drdy_irq_ok` → a interrupção de amostragem não
  subiu, e o loop cai no fallback de leitura por iteração
- Bit `0x80` (`ADC_INIT_FAILED`) → o ADS112C04 não respondeu no I²C
- `taxa de amostragem` perto de 1,0 S/s → nada está governando a amostragem
- `conversoes nao lidas` alta → o I²C não acompanha a taxa do ADC

### J-Link não conecta ao alvo
- ✅ **A placa está alimentada?** Sem bateria, ligue a PPK2 primeiro. Erros de
  `RESET (pin 15) high` e `Failed to power up DAP` costumam ser só falta de
  alimentação, não defeito no cabo ou proteção de readback
- ✅ Driver USB do probe instalado (`USBDriver/InstDrivers.exe`, precisa de admin)
- ✅ Só depois disso considerar `erase.sh` + `flash_all.sh`

### Dispositivo não anuncia
- ✅ Ver RTT no boot: `Insufficient RAM allocated` indica o valor exato a pôr em
  `RAM_START`/`RAM_SIZE` no `.emProject`
- ✅ Se o rádio Bluetooth do PC desligou sozinho, `ble_client.py` religa
  automaticamente; fora dele, verifique o toggle do Windows

### Dados cortados ou ausentes
- ✅ MTU precisa negociar ≥ 123 B para o payload de 120 B. `ble_client.read_mtu()`
  aborta o run se ficar abaixo
- ✅ Pacotes de **2 bytes** em vez de 120 indicam `is_var_len` não setado na
  característica — sem isso o atributo GATT tem tamanho fixo em `init_len` e a
  SoftDevice trunca toda notificação
- ✅ CCCD habilitado (o firmware relê a CCCD a cada pacote)
- ✅ ~8% de blocos descartados é o comportamento atual esperado, não defeito

### DS3502 não responde
- ✅ Endereço I²C 0x28, pull-ups de 4k7 presentes
- ✅ Alimentação ≥ 2,7 V (limite inferior do componente)
- ✅ **Se o ganho não muda via BLE, não é o DS3502**: a escrita BLE é no-op
  (ver [Limitações conhecidas](#-limitações-conhecidas))

## 📚 Referências

### Artigo, protocolo e esquemáticos
- [A Low-Power Bluetooth LE Surface EMG Sensor](663744.pdf) — artigo do projeto
- [Esquemático EMG v2.0](Schematic_EMG-schematic-v2.0_2025-02-19.pdf) — circuito do sensor
- [Protocolo Experimental sEMG + Supercapacitor](Protocolo_Experimental_sEMG_Supercapacitor.pdf) — 8 ensaios de caracterização e validação

### Medições de consumo
- [Relatório de consumo (PDF)](power_profiling/relatorio_consumo.pdf) — caracterização com PPK2, 9 páginas
- [Metodologia e plano](power_profiling/PLANO.md) — decisões de bancada e histórico de diagnóstico
- [Guia da PPK2](power_profiling/README.md) — fiação, modos source vs ampere

### Datasheets e documentação
- [nRF52840 Product Specification](https://infocenter.nordicsemi.com/pdf/nRF52840_PS_v1.8.pdf)
- [ADS112C04 Datasheet](https://www.ti.com/lit/ds/symlink/ads112c04.pdf)
- [DS3502 Datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/DS3502.pdf)
- [INA317 Datasheet](https://www.ti.com/lit/ds/symlink/ina317.pdf)
- [nRF5 SDK Documentation](https://infocenter.nordicsemi.com/topic/sdk_nrf5_v17.1.0/)

### Dataset de validação
- [EMG Physical Action Data Set](https://doi.org/10.24432/C53W49) - UCI Machine Learning Repository (Theodoridis, 2011)

## 🔗 Repositórios Relacionados

- **App Mobile**: [emg_ble_app](https://github.com/brunorchaves/emg_ble_app) - React Native app
- **Firmware nRF52**: [EMG_BLE](https://github.com/brunorchaves/EMG_BLE) - Este repositório

## 📄 Licença

MIT License - Veja [LICENSE](LICENSE) para detalhes

## 👨‍💻 Autores

**Firmware e desenvolvimento**
- Bruno Chaves - [GitHub](https://github.com/brunorchaves)

**Autores do artigo** (LEB — Laboratório de Engenharia Biomédica, UFMG)
- Robert Ribeiro Gomes — mestrando, PPGEE/UFMG
- Bruno Ribeiro Chaves — graduando em Engenharia Elétrica, UFMG
- Renan Fernandes Kozan — professor, UFMG
- Dalton Martini Colombo — professor, UFMG

---

🤖 Desenvolvido com [Claude Code](https://claude.com/claude-code)
