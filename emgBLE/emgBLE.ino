#include <Wire.h>
#include "SparkFun_ADS122C04_ADC_Arduino_Library.h"
#include <ArduinoBLE.h>

BLEService emgService("19B10000-E8F2-537E-4F6C-D104768A1214");
// Bluetooth® Low Energy EMG Data Characteristic
BLEIntCharacteristic emgDataChar("19B10001-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify);
int oldEMGData = 0;  // Last EMG data reading
// TODO
// - Communicate with ADS112 - ok
// - Read EMG signal and plot - ok
// - Transmit via BLE
// - Receive BLE data using ESP32 dongle

#define ADDRESS_0X40  0x40
SFE_ADS122C04 EMG_0x40_sensor(ADDRESS_0X40);

bool isADSConfigured = false;
unsigned long previousMillis = 0;
const long interval = 1000; // Interval for LED blinking (1 second)
bool ledState = false; // Track LED state

#define LED1  D7
#define LED2  D8
#define RSTPIN  D2

// Butterworth Filter Parameters (mkfilter -Bu -Bp -o 1 -a 0.035 0.2 -l)
#define NZEROS 2
#define NPOLES 2
#define GAIN   2.643255112e+000

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
  Serial.println("Starting ADS112...");

  pinMode(RSTPIN, OUTPUT);
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
  }

  // // Start BLE
  // if (!BLE.begin()) {
  //   Serial.println("Starting BLE module failed!");
  // }
  // else
  // {
  //    // Set BLE device properties
  //   BLE.setLocalName("XIAO_BLE_EMG");
  //   BLE.setAdvertisedService(emgService);

  //   // Add characteristic to the service
  //   emgService.addCharacteristic(emgDataChar);
  //   BLE.addService(emgService);

  //   // Start advertising
  //   BLE.advertise();
  //   Serial.println("BLE EMG Peripheral Started...");
  // }
}

void loop() {
  static int16_t raw_EMG_0X40 = 0;
  // BLEDevice central = BLE.central();  // Listen for BLE connections

  if (isADSConfigured) {
    // Read raw ADC value
    raw_EMG_0X40 = EMG_0x40_sensor.readRawVoltage();
    
    // Apply Butterworth filter
    // float filtered_EMG = butterworthFilter((float)raw_EMG_0X40);
    Serial.println(raw_EMG_0X40);


    // Print filtered value
    // Serial.println(filtered_EMG);
  } else {
    Serial.println("ADC not configured.");
  }

  // Handle LED blinking using millis()
  if(isADSConfigured)
  {
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

  // if (central) {
    // Serial.print("Connected to: ");
    // Serial.println(central.address());

    // While the central is still connected to the peripheral:
    // if (central.connected()) 
    // {
      
        // Serial.print("EMG Data: ");
        
        // emgDataChar.writeValue(filtered_EMG);  // Update EMG data characteristic
        // oldEMGData = emgData;  // Save new EMG data for next comparison
        
    // }

    // Serial.print("Disconnected from: ");
    // Serial.println(central.address());
  // }
}


