#ifndef BLE_EMG_SERVICE_H__
#define BLE_EMG_SERVICE_H__

#include <stdint.h>
#include "ble.h"
#include "ble_srv_common.h"

#define EMG_SERVICE_UUID_BASE         { 0x14, 0xA1, 0x68, 0xD1, 0x6C, 0x4F, 0x7E, 0x53, \
                                        0xF2, 0xE8, 0x00, 0x10, 0x00, 0x00, 0xB1, 0x19 }

#define EMG_SERVICE_UUID              0x0001
#define EMG_CHAR_UUID                 0x0002
#define EMG_GAIN_CHAR_UUID            0x0003

typedef struct {
    uint16_t                    service_handle;
    ble_gatts_char_handles_t    emg_char_handles;
    ble_gatts_char_handles_t    gain_char_handles;  // NOVO
    uint8_t                     uuid_type;
    uint16_t                    conn_handle;
} ble_emg_service_t;

void ble_emg_service_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context);

uint32_t ble_emg_service_init(ble_emg_service_t * p_emg);

uint32_t ble_emg_service_notify(ble_emg_service_t * p_emg, uint16_t conn_handle, uint16_t emg_value);

#endif // BLE_EMG_SERVICE_H__
