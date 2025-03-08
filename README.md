# **EMG_BLE**

Codes for the EMG BLE project.

---

## **📌 Initial Setup**

Follow these steps to set up your development environment:

### **1️⃣ Install Arduino IDE**
Download the latest version of the Arduino IDE for your operating system:  
🔗 [Arduino IDE Download](https://www.arduino.cc/en/software)

### **2️⃣ Launch the Arduino Application**
After installation, open the Arduino IDE.

### **3️⃣ Add Seeed Studio XIAO nRF52840 (Sense) Board Package**
To add support for the XIAO nRF52840 (Sense) board:

1. Open **Arduino IDE** and navigate to **File** > **Preferences**.
2. In the **"Additional Boards Manager URLs"** field, add the following URL:
   ```
   https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json
   ```
3. Click **OK** and restart the Arduino IDE if necessary.
4. Go to **Tools** > **Board** > **Boards Manager** and search for "Seeed nRF52 Boards".
5. Install the package for XIAO nRF52840 (Sense).

---

## **🔗 Useful Links**

- 📖 **XIAO BLE Wiki**: [Seeed Studio XIAO BLE Documentation](https://wiki.seeedstudio.com/XIAO_BLE/)
- 🛠 **Arduino IDE**: [Download & Install](https://www.arduino.cc/en/software)
- 📡 **BLE Communication Guide**: [Send/Receive Data with XIAO BLE](https://how2electronics.com/send-receive-data-to-mobile-app-with-xiao-ble-nrf52840-sense/)
- 📘 **BLE Scanning, Connecting & Reading Services**: [ESP-IDF GATT Client Guide](https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/bluedroid/ble/gatt_client/tutorial/Gatt_Client_Example_Walkthrough.md)

---

## **⚙️ Butterworth Filter Design**

This project uses a **1st-order Butterworth bandpass filter** with:
- **Corner Frequencies**: 70 Hz - 400 Hz
- **Sample Rate**: 2000 Hz

To generate your own filter coefficients, use this online tool:  
🔗 [Butterworth Code Generator](http://www.piclist.com/techref/uk/ac/york/cs/www-users/http/~fisher/mkfilter/trad.html)

---

🚀 **Now you're ready to start developing!** Happy coding! 🎸