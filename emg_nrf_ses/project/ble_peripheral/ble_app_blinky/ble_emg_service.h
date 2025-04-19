#ifndef BLE_EMG_SERVICE_H
#define BLE_EMG_SERVICE_H

#include <stdint.h>
#include "ble.h"

void ble_emg_stack_init(void);
void ble_emg_gap_init(void);
void ble_emg_gatt_init(void);
void ble_emg_advertising_init(void);
void ble_emg_advertising_start(void);
void ble_emg_service_init(void);
void ble_emg_notify(int16_t sample);

#endif // BLE_EMG_SERVICE_H
