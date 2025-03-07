# **EMG_BLE**

Codes for the EMG BLE project.

## **Initial Setup**

Follow these steps to set up your development environment:

### **Step 1: Download and Install Arduino IDE**
Download the latest version of the Arduino IDE according to your operating system:  
🔗 [Arduino IDE Download](https://www.arduino.cc/en/software)

### **Step 2: Launch the Arduino Application**
Open the Arduino IDE after installation.

### **Step 3: Add Seeed Studio XIAO nRF52840 (Sense) Board Package**
To add support for the XIAO nRF52840 (Sense) board:

1. Navigate to **File** > **Preferences**.
2. In the **"Additional Boards Manager URLs"** field, add the following URL:
   ```
   https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json
   ```
3. Click **OK** and restart the Arduino IDE if necessary.

## **Useful Links**

- 🔗 **XIAO BLE Wiki**: [Seeed Studio XIAO BLE Documentation](https://wiki.seeedstudio.com/XIAO_BLE/)
- 🔗 **Arduino IDE**: [Download & Install](https://www.arduino.cc/en/software)

## ⚙️ Butterworth Filter Design
This project uses a **1st-order Butterworth bandpass filter** with corner frequencies **70Hz - 400Hz** at **2000 Hz sample rate**.  
To generate your own filter coefficients, use:  
🔗 [Butterworth Code Generator](http://www.piclist.com/techref/uk/ac/york/cs/www-users/http/~fisher/mkfilter/trad.html)



BLE: https://how2electronics.com/send-receive-data-to-mobile-app-with-xiao-ble-nrf52840-sense/