import pandas as pd
import matplotlib.pyplot as plt
import os
import glob
from datetime import datetime

# Configuration
DATA_FOLDER = '.'  # Folder where CSV files are saved (current directory)
PLOT_LAST_N = 3    # Number of most recent files to plot (0 for all)
PLOT_RAW_AND_FILTERED = False  # Set True if CSV contains both raw and filtered data

def plot_emg_data(filepath):
    """Plot EMG data from a single CSV file"""
    try:
        df = pd.read_csv(filepath)
        filename = os.path.basename(filepath)
        
        plt.figure(figsize=(12, 6))
        
        if 'voltage_mV' in df.columns:
            # Single signal plot
            plt.plot(df['timestamp'], df['voltage_mV'], label='EMG Signal')
            plt.ylabel('Voltage (mV)')
        elif 'filtered_voltage_mV' in df.columns and PLOT_RAW_AND_FILTERED:
            # Dual signal plot
            plt.plot(df['timestamp'], df['raw_voltage_mV'], alpha=0.5, label='Raw EMG')
            plt.plot(df['timestamp'], df['filtered_voltage_mV'], label='Filtered EMG')
            plt.ylabel('Voltage (mV)')
            plt.legend()
        else:
            print(f"Unexpected columns in {filename}")
            return

        plt.title(f"EMG Signal - {filename}")
        plt.xlabel('Time (s)')
        plt.grid(True)
        plt.tight_layout()
        
        # Save plot as PNG
        plot_filename = os.path.splitext(filepath)[0] + '.png'
        plt.savefig(plot_filename, dpi=300)
        print(f"Saved plot: {plot_filename}")
        plt.show()
        
    except Exception as e:
        print(f"Error plotting {filepath}: {e}")

def find_emg_files():
    """Find all EMG CSV files in the data folder"""
    files = glob.glob(os.path.join(DATA_FOLDER, 'emg_data_*.csv'))
    # Sort by modification time (newest first)
    files.sort(key=os.path.getmtime, reverse=True)
    return files

def main():
    print("EMG Data Plotter")
    files = find_emg_files()
    
    if not files:
        print("No EMG CSV files found!")
        return
    
    if PLOT_LAST_N > 0:
        files = files[:PLOT_LAST_N]
    
    print(f"Found {len(files)} files. Plotting...")
    for file in files:
        print(f"\nPlotting {os.path.basename(file)}")
        plot_emg_data(file)

if __name__ == "__main__":
    main()