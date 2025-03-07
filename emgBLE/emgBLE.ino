#include <Wire.h>
#include "SparkFun_ADS122C04_ADC_Arduino_Library.h" 


// TODO 
// - Comunicate with ADS112 - ok
// - Read emg signal and plot - 
// - Transmit via BLE
// - Receive ble data using ESP32 dongle

#define ADDRESS_0X40  0x40
SFE_ADS122C04 EMG_0x40_sensor(ADDRESS_0X40);

bool isADSConfigured = false;
unsigned long previousMillis = 0;
const long interval = 1000; // Interval for LED blinking (1 second)
bool ledState = false; // Track LED state

#define LED1  D7  // Define pin for LED 1
#define LED2  D8  // Define pin for LED 2
#define RSTPIN  D2 // Define pin for LED 2

void setup() 
{
  Serial.begin(115200);
  Serial.println("Starting ADS122C04...");
  pinMode(RSTPIN, OUTPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(RSTPIN, HIGH);

  Wire.begin();

  if (!EMG_0x40_sensor.begin(ADDRESS_0X40)) 
  {
    Serial.println("ERROR: ADS122C04 not detected! Check wiring.");
  } 
  else 
  {
    Serial.println("ADS122C04 detected at I2C 0x40.");
    EMG_0x40_sensor.configureADCmode(ADS122C04_RAW_MODE); // Configure for raw mode
    isADSConfigured = true;
  }
}

void loop() {
  if (isADSConfigured) 
  {
    Serial.println("Reading voltage...");
    int32_t raw_EMG_0X40 = EMG_0x40_sensor.readRawVoltage();
    Serial.print("Raw Voltage: ");
    Serial.println(raw_EMG_0X40);
  } 
  else 
  {
    Serial.println("ADC not configured.");
  }

  // Handle LED blinking using millis()
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    
    // Toggle LED state
    ledState = !ledState;
    
    digitalWrite(LED1, ledState);
    digitalWrite(LED2, ledState);
    digitalWrite(LED_BUILTIN, ledState);
  }
}
