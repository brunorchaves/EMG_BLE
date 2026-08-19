# EMG_BLE - Firmware nRF52840

Firmware profissional para aquisição e transmissão de sinais EMG via Bluetooth Low Energy (BLE) usando nRF52840, ADS112C04 e DS3502.

Este é o firmware do sensor sEMG vestível de baixo consumo apresentado no artigo
**"A Low-Power Bluetooth LE Surface EMG Sensor"** (LEB/UFMG, SEB 2025) — 45 mW transmitindo a 2 kS/s,
~30% abaixo de sistemas comparáveis. Detalhes em [Publicação Científica](#-publicação-científica).

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

- Todos os periféricos não utilizados são desabilitados quando o rádio está desconectado
- Rotina de gerenciamento de energia da Nordic invocada durante os períodos de *idle*
- Potência de transmissão BLE reduzida para **-20 dBm**
- Intervalo de *advertising* estendido de 40 ms (padrão) para **500 ms**, reduzindo o *duty cycle* do rádio

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

- **Aquisição de alta performance** - ADS112C04 @ 2000 Hz, 16-bit
- **Streaming BLE otimizado** - 60 amostras/pacote, MTU 247 bytes
- **Controle de ganho remoto** - DS3502 digital potentiometer (1x-10x)
- **Filtragem digital** - Butterworth bandpass 20-500 Hz
- **BLE 2M PHY** - 2 Mbps throughput para streaming em tempo real
- **Connection interval 7.5ms** - Latência ultra-baixa
- **Buffer circular** - 60 amostras FIFO para transmissão em lote
- **Logs detalhados** - NRF_LOG + UART para debug

## 🔧 Hardware

### Componentes Principais
- **MCU**: nRF52840 (Nordic Semiconductor)
- **ADC**: ADS112C04 (Texas Instruments) - 24-bit, I2C
- **Potenciômetro Digital**: DS3502 (Maxim) - 10kΩ, I2C
- **Programador**: J-Link (SEGGER)

### Pinout I2C
```
TWI0 (I2C):
- SCL: P0.27
- SDA: P0.26

Dispositivos:
- ADS112C04: 0x45 (ADC)
- DS3502: 0x28 (Potentiômetro)
```

### Configuração ADS112C04
```c
Sample Rate: 2000 Hz
Resolution: 16-bit signed
Input: Differential (AIN0/AIN1)
Gain: 1x (ajustável via DS3502)
Mode: Continuous conversion
```

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
Device Name: "EMG_BLE"
Advertising Interval: 64 units (40ms)
Connection Interval: 7.5-15ms (6-12 units)
Slave Latency: 0
Supervision Timeout: 4000ms
PHY: BLE 2M (2 Mbps)
```

### GATT (Generic Attribute Profile)

#### EMG Service
```c
Service UUID: 19b10001-1000-e8f2-537e-4f6cd168a114

Characteristics:
1. EMG Data (NOTIFY)
   UUID: 19b10002-1000-e8f2-537e-4f6cd168a114
   Format: Array of int16_t (60 samples)
   Size: 120 bytes per notification
   Rate: ~250 packets/second

2. Gain Control (WRITE)
   UUID: 19b10003-1000-e8f2-537e-4f6cd168a114
   Format: uint8_t (1-10)
   Action: Updates DS3502 wiper value
```

### MTU Negotiation
```c
Default MTU: 23 bytes
Negotiated MTU: 247 bytes
Payload útil: 244 bytes
Packet overhead: 3 bytes (ATT header)
```

## 🚀 Instalação

### Pré-requisitos
- **SEGGER Embedded Studio** 8.24+
- **nRF5 SDK** 17.1.0
- **SoftDevice** S140 v7.3.0
- **J-Link** Software v8.96+
- **nRF Command Line Tools**

### Build e Flash

1. **Abrir projeto no SES**:
```bash
cd emg_nrf_ses/project/ble_peripheral/ble_app_blinky/pca10056/s140/ses
# Abrir ble_app_blinky_pca10056_s140.emProject
```

2. **Compilar**:
```bash
# Via linha de comando (Windows)
"C:/Program Files/Segger/SEGGER Embedded Studio 8.24/bin/emBuild.exe" \
  -config Release -rebuild ble_app_blinky_pca10056_s140.emProject

# Ou dentro do SES: Build → Build Solution (F7)
```

3. **Gravar SoftDevice** (primeira vez):
```bash
nrfjprog --family NRF52 --eraseall
nrfjprog --family NRF52 --program s140_nrf52_7.3.0_softdevice.hex --verify
```

4. **Gravar Firmware**:
```bash
# Via J-Link Commander
JLink -device NRF52840_XXAA -if SWD -speed 4000 -CommandFile flash_firmware.jlink

# Ou via nrfjprog
nrfjprog --family NRF52 --program Output/Release/Exe/ble_app_blinky_pca10056_s140.hex --verify
nrfjprog --family NRF52 --reset
```

## 📊 Processamento de Sinal

### Pipeline de Dados
```
ADS112C04 → Butterworth Filter → FIFO Buffer → BLE Notification
  2kHz          20-500 Hz          60 samples      250 pkt/s
```

### Filtro Butterworth
```c
Type: Bandpass 4th order
Cutoff frequencies: 20-500 Hz
Sample rate: 2000 Hz
Implementation: Direct Form II
Coefficients: Pre-calculated normalized
```

### FIFO Buffer
```c
Size: 60 samples (int16_t)
Type: Circular buffer
Overflow: Oldest data discarded
Transmission: When full (60 samples ready)
```

## 🔬 Configurações BLE Avançadas

### RAM Allocation
```c
RAM_START: 0x20002B78  // Após SoftDevice
RAM_SIZE: 0x3D488      // 244 KB disponível
SoftDevice RAM: ~11 KB (MTU 247)
```

### BLE Stack Config
```c
NRF_SDH_BLE_GATT_MAX_MTU_SIZE: 247
NRF_SDH_BLE_GAP_DATA_LENGTH: 251
NRF_SDH_BLE_VS_UUID_COUNT: 2
NRF_SDH_BLE_GATTS_ATTR_TAB_SIZE: 1408
```

### Connection Parameters
```c
MIN_CONN_INTERVAL: MSEC_TO_UNITS(7.5, UNIT_1_25_MS)   // 6 units
MAX_CONN_INTERVAL: MSEC_TO_UNITS(15, UNIT_1_25_MS)    // 12 units
SLAVE_LATENCY: 0                                       // Zero latency
CONN_SUP_TIMEOUT: MSEC_TO_UNITS(4000, UNIT_10_MS)     // 4s timeout
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

### UART Debug
```c
Baud rate: 115200
Data bits: 8
Stop bits: 1
Parity: None
TX: Assíncrono (non-blocking)

Mensagens:
- "ADS112C04 configured"
- "DS3502 resistance set successfully"
- "BLE connected"
- "Packet sent: N/M successful"
```

## 📈 Performance

### Métricas de Transmissão
```
Sample Rate: 2000 Hz
Packet Rate: 250 packets/second
Samples/Packet: 60
Latency: 7.5-15ms (connection interval)
Throughput: ~30 KB/s (240 kbps)
Packet Loss: <0.1% (com CCCD check)
```

### Otimizações Implementadas
1. ✅ MTU negotiation para pacotes maiores
2. ✅ Connection interval otimizado (7.5ms)
3. ✅ BLE 2M PHY para dobrar throughput
4. ✅ CCCD verification antes de notificar
5. ✅ HVN_TX_COMPLETE observer para flow control
6. ✅ Buffer circular para streaming contínuo

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

### Dispositivo não conecta
- ✅ Verificar se SoftDevice foi gravado
- ✅ Confirmar RAM allocation correta
- ✅ Resetar dispositivo após flash
- ✅ Verificar logs RTT para erros

### Dados cortados/incompletos
- ✅ MTU deve ser negociado (247 bytes)
- ✅ CCCD deve estar habilitado
- ✅ Connection interval muito alto
- ✅ Verificar tx_in_progress flag

### Alta taxa de erro
- ✅ Distância >5m do dispositivo
- ✅ Interferência BLE (WiFi 2.4GHz)
- ✅ Múltiplas conexões simultâneas
- ✅ Latência de processamento no app

### DS3502 não responde
- ✅ Verificar endereço I2C (0x28)
- ✅ Pull-ups em SDA/SCL
- ✅ Alimentação 3.3V estável
- ✅ Logs RTT mostram falha

## 📚 Referências

### Artigo e esquemáticos
- [A Low-Power Bluetooth LE Surface EMG Sensor](663744.pdf) - Artigo do projeto
- [Esquemático EMG v2.0](Schematic_EMG-schematic-v2.0_2025-02-19.pdf) - Circuito do sensor

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
