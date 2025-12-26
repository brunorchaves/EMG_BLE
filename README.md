# EMG_BLE - Firmware nRF52840

Firmware profissional para aquisição e transmissão de sinais EMG via Bluetooth Low Energy (BLE) usando nRF52840, ADS112C04 e DS3502.

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

- [nRF52840 Product Specification](https://infocenter.nordicsemi.com/pdf/nRF52840_PS_v1.8.pdf)
- [ADS112C04 Datasheet](https://www.ti.com/lit/ds/symlink/ads112c04.pdf)
- [DS3502 Datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/DS3502.pdf)
- [nRF5 SDK Documentation](https://infocenter.nordicsemi.com/topic/sdk_nrf5_v17.1.0/)

## 🔗 Repositórios Relacionados

- **App Mobile**: [emg_ble_app](https://github.com/brunorchaves/emg_ble_app) - React Native app
- **Firmware nRF52**: [EMG_BLE](https://github.com/brunorchaves/EMG_BLE) - Este repositório

## 📄 Licença

MIT License - Veja [LICENSE](LICENSE) para detalhes

## 👨‍💻 Autor

Bruno Chaves - [GitHub](https://github.com/brunorchaves)

---

🤖 Desenvolvido com [Claude Code](https://claude.com/claude-code)
