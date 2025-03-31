import serial
import time
import matplotlib.pyplot as plt
import pandas as pd
from datetime import datetime

# Configuration
SERIAL_PORT = 'COM55'  # Change to your port
BAUD_RATE = 115200
DURATION = 10  # seconds to record
CSV_FILENAME = f"emg_data_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"

# Lists to store data
timestamps = []
values = []

try:
    print(f"Connecting to {SERIAL_PORT}...")
    with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) as ser:
        print(f"Connected. Recording for {DURATION} seconds...")
        ser.reset_input_buffer()
        
        start_time = time.time()
        while (time.time() - start_time) < DURATION:
            if ser.in_waiting:
                try:
                    line = ser.readline().decode('utf-8').strip()
                    value = float(line)  # Convert to float
                    
                    # Store data with timestamp
                    elapsed = time.time() - start_time
                    timestamps.append(elapsed)
                    values.append(value * 1000)  # Convert to mV
                    
                    print(f"{elapsed:.2f}s: {value:.2f}V", end='\r')
                except (ValueError, UnicodeDecodeError):
                    continue  # Skip bad data

    # Create DataFrame and save to CSV
    df = pd.DataFrame({
        'timestamp': timestamps,
        'voltage_mV': values
    })
    df.to_csv(CSV_FILENAME, index=False)
    print(f"\nData saved to {CSV_FILENAME}")

    # Plot the data
    plt.figure(figsize=(12, 6))
    plt.plot(df['timestamp'], df['voltage_mV'])
    plt.title(f"EMG Signal ({DURATION} seconds)")
    plt.xlabel('Time (s)')
    plt.ylabel('Voltage (mV)')
    plt.grid(True)
    plt.show()

except serial.SerialException as e:
    print(f"Error: {e}")
except KeyboardInterrupt:
    print("\nRecording stopped by user")
finally:
    print("Done")