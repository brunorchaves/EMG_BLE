# EMG_BLE

This repository contains the source code and configuration files for the **EMG_BLE** project, which integrates Electromyography (EMG) signal acquisition with Bluetooth Low Energy (BLE) communication. The project is designed to collect EMG signals, process them, and transmit the data wirelessly using BLE.

---

## 📂 Repository Structure

### **Main Directories**
- **`emg_nrf_ses/`**: Contains the Nordic Semiconductor nRF SDK-based implementation for BLE communication and EMG signal processing.
  - **`project/ble_peripheral/ble_app_blinky/`**: Example BLE peripheral application with custom EMG service integration.
  - **`components/`**: Includes BLE services, libraries, and SoftDevice headers for BLE stack integration.
  - **`config/`**: Configuration files for various nRF devices (e.g., nRF52810, nRF52820, nRF52832, nRF52840).
  - **`modules/`**: Contains Nordic-specific modules like `nrfx` for hardware abstraction.
- **`emgBLE/`**: Arduino-based implementation for EMG signal acquisition and processing using I2C communication with an ADS112 sensor.
- **`.gitignore`**: Specifies files and directories to be ignored by Git.

---

## 🛠 Features

### **BLE Services**
- **Custom EMG Service**:
  - **EMG Characteristic**: Notifies EMG signal data.
  - **Gain Characteristic**: Allows configuration of the EMG signal gain.

### **EMG Signal Processing**
- **Butterworth Bandpass Filter**: Filters raw EMG signals to remove noise and extract relevant frequency components.
- **ADS112 Sensor Integration**: Reads raw EMG signals via I2C.

### **BLE Communication**
- BLE advertising and connection management using Nordic's SoftDevice stack.
- Support for multiple BLE configurations (e.g., GAP, GATT, L2CAP).

---

## 🚀 Getting Started

### **1️⃣ Prerequisites**
- **Hardware**:
  - Nordic nRF52840 Development Kit (or compatible board).
  - ADS112 sensor for EMG signal acquisition.
  - Jlink Segger for program loading and debugging.
- **Software**:
  - [SEGGER Embedded Studio](https://www.segger.com/products/development-tools/embedded-studio/) for nRF SDK projects.
  - [Arduino IDE](https://www.arduino.cc/en/software) for Arduino-based implementation.

### **2️⃣ Setup**
1. Clone the repository:
   ```bash
   git clone https://github.com/brunorchaves/EMG_BLE.git
