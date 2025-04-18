#include <Wire.h>
#include "SparkFun_ADS122C04_ADC_Arduino_Library.h"
#include <ArduinoBLE.h>

// BLE Configuration (can be enabled/disabled with bleEnabled flag)
BLEService emgService("19B10000-E8F2-537E-4F6C-D104768A1214");
BLEIntCharacteristic emgDataChar("19B10001-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify);
int oldEMGData = 0;
bool bleEnabled = false; // Flag to enable/disable BLE functionality

#define ADDRESS_0X40  0x40
SFE_ADS122C04 EMG_0x40_sensor(ADDRESS_0X40);

bool isADSConfigured = false;
unsigned long previousMillis = 0;
const long interval = 1000; // Interval for LED blinking (1 second)
bool ledState = false; // Track LED state

#define LED1  D7
#define LED2  D8
#define RSTPIN  D2
#define DRDYPIN D3 // DRDY pin connected to D3 (active low)

// Butterworth Filter Parameters (mkfilter -Bu -Bp -o 1 -a 0.035 0.2 -l)
#define NZEROS 2
#define NPOLES 2
#define GAIN   2.643255112e+000
float filteredEMG =0.0f;
// ADC Conversion Constants
#define VREF 5.0           // Your signal range is 0-5V
#define ADC_RESOLUTION 65535.0 // 2^16 - 1 for unsigned 16-bit
#define OFFSET_VOLTAGE 2.5 // Your signal offset voltage

static float xv[NZEROS+1] = {0}, yv[NPOLES+1] = {0};

// Butterworth Bandpass Filter (70Hz - 400Hz, Fs = 2000 Hz)
float butterworthFilter(float input) {
  xv[0] = xv[1]; xv[1] = xv[2];
  xv[2] = input / GAIN;
  
  yv[0] = yv[1]; yv[1] = yv[2];
  yv[2] = (xv[2] - xv[0])
        + (-0.2735690431 * yv[0]) + (1.0844313730 * yv[1]);

  return yv[2]; // Filtered signal output
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);   // UART por pinos TX/RX (D6/D7)
  Serial.println("Starting ADS112...");

  pinMode(RSTPIN, OUTPUT);
  pinMode(DRDYPIN,INPUT); // Set DRDY pin as input with pullup (active low)
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(RSTPIN, HIGH);

  Wire.begin();

  if (!EMG_0x40_sensor.begin(ADDRESS_0X40)) {
    Serial.println("ERROR: ADS112 not detected! Check wiring.");
  } else {
    Serial.println("ADS112 detected at I2C 0x40.");
    EMG_0x40_sensor.configureADCmode(ADS122C04_RAW_MODE);
    isADSConfigured = true;
    // After begin(), add:
    EMG_0x40_sensor.start(); // Explicitly start conversions
  }

  // Initialize BLE if enabled
  if (bleEnabled) {
    if (!BLE.begin()) {
      Serial.println("Starting BLE module failed!");
    }
    else {
      // Set BLE device properties
      BLE.setLocalName("XIAO_BLE_EMG");
      BLE.setAdvertisedService(emgService);

      // Add characteristic to the service
      emgService.addCharacteristic(emgDataChar);
      BLE.addService(emgService);

      // Start advertising
      BLE.advertise();
      Serial.println("BLE EMG Peripheral Started...");
    }
  }
  // attachInterrupt(digitalPinToInterrupt(DRDYPIN), onDataReady, FALLING);

}

void loop() 
{
  static int16_t raw_EMG_0X40 = 0;
  // Handle BLE connections if enabled
  if (bleEnabled) 
  {
    BLEDevice central = BLE.central();
  }

  if (EMG_0x40_sensor.checkDataReady()) 
  {
    raw_EMG_0X40 = EMG_0x40_sensor.readRawVoltage();
    // float voltage = ((float)raw / ADC_RESOLUTION) * VREF; // Convert to voltage (0-5V)
    // voltage -= OFFSET_VOLTAGE; // Remove 2.5V offset
    // voltage *= 1000;
    // filteredEMG = butterworthFilter(voltage); // Apply Butterworth filter
    Serial1.println(raw_EMG_0X40); // Print filtered signal
  }
  // Only read when data is ready (DRDY goes low)
  // if (digitalRead(DRDYPIN) == LOW) 
  // {
  //   // 1. Read raw unsigned 16-bit value (0-65535)
  //   raw_EMG_0X40 = EMG_0x40_sensor.readRawVoltage();
    
  //   // 2. Convert to voltage (0-5V)
  //   float voltage = ((float)raw_EMG_0X40 / ADC_RESOLUTION) * VREF;
    
  //   // 3. Remove 2.5V offset (now ranges from -2.5V to +2.5V)
  //   voltage -= OFFSET_VOLTAGE;
    
  //   // 4. Apply Butterworth filter
  //   float filtered_voltage = butterworthFilter(voltage);

  //   // Send data via BLE if enabled
  //   if (bleEnabled) 
  //   {
  //     emgDataChar.writeValue(filtered_voltage);
  //   }
  // }


  // Handle LED blinking using millis()
  if(isADSConfigured) {

    emgDataChar.writeValue(raw_EMG_0X40);

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
}