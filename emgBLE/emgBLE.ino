#include <Wire.h>
#include <ArduinoBLE.h>

/* ─────────── Configuração BLE ─────────── */
BLEService           emgService("19B10000-E8F2-537E-4F6C-D104768A1214");
BLEIntCharacteristic emgDataChar("19B10001-E8F2-537E-4F6C-D104768A1214",
                                  BLERead | BLENotify);

/* ─────────── Pinos ─────────── */
#define LED1    D7
#define LED2    D8
#define RSTPIN  D2
#define DRDYPIN D3   // (LOW ativo)

/* ─────────── Timers e estados ─────────── */
constexpr uint32_t BLE_ON_TIME_MS  = 15000;  // 5 s ligado
constexpr uint32_t BLE_OFF_TIME_MS = 15000;  // 5 s desligado

bool     bleActive      = false;            // estado atual do rádio
uint32_t bleToggleStamp = 0;                // instante da última troca

/* ─────────── ADC fictício ─────────── */
bool     isADSConfigured = true;            // só para compilar
int16_t  raw_EMG_0x40    = 0;

/* ─────────── Protótipos ─────────── */
void startBLE(void);
void stopBLE(void);
void updateLEDs(void);

/* ─────────── Setup ─────────── */
void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);   // UART nos pinos D6/D7

  pinMode(RSTPIN, OUTPUT);
  pinMode(LED1,  OUTPUT);
  pinMode(LED2,  OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(RSTPIN, HIGH);

  /* Começamos com o BLE DESLIGADO */
  stopBLE();
}

/* ─────────── Loop principal ─────────── */
void loop() {
  uint32_t now = millis();

  /* ── Alterna BLE a cada 5 s ── */
  uint32_t timeout = bleActive ? BLE_ON_TIME_MS : BLE_OFF_TIME_MS;
  if (now - bleToggleStamp >= timeout) {
    bleToggleStamp = now;
    if (bleActive)
      stopBLE();
    else
      startBLE();
  }

  /* ── Se BLE ativo, trate conexões e envie dado ── */
  if (bleActive) {
    BLEDevice central = BLE.central();  // atualiza estado de conexão

    raw_EMG_0x40 = 4;                 // demo
    emgDataChar.writeValue(raw_EMG_0x40);

    BLE.poll();                         // mantém a pilha viva
  }
}

/* ─────────── Liga BLE ─────────── */
void startBLE() {
  if (!BLE.begin()) {
    Serial.println("Erro ao iniciar BLE!");
    return;
  }

  BLE.setLocalName("XIAO_BLE_EMG");
  BLE.setAdvertisedService(emgService);

  emgService.addCharacteristic(emgDataChar);
  BLE.addService(emgService);

  BLE.advertise();
  bleActive = true;
  updateLEDs();
  Serial.println("BLE ligado (anunciando)...");
}

/* ─────────── Desliga BLE ─────────── */
void stopBLE() {
  if (bleActive) {
    BLE.stopAdvertise();
    BLE.disconnect();       // encerra ligações, se houver
    BLE.end();              // desliga a pilha
  }
  bleActive = false;
  updateLEDs();
  Serial.println("BLE desligado.");
}

/* ─────────── Atualiza LEDs conforme estado do BLE ─────────── */
void updateLEDs() {
  digitalWrite(LED1,        bleActive ? HIGH : LOW);
  digitalWrite(LED2,        bleActive ? HIGH : LOW);
  digitalWrite(LED_BUILTIN, bleActive ? HIGH : LOW);
}
