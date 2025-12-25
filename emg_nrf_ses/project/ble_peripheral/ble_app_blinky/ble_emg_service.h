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

// Configuração de pacotes otimizados para MTU padrão (8 amostras x 2 bytes = 16 bytes)
// MTU padrão BLE = 23 bytes (20 bytes payload - 3 bytes header = 17 bytes úteis)
#define EMG_PACKET_SIZE               8       // Número de amostras por pacote
#define EMG_MAX_PAYLOAD               (EMG_PACKET_SIZE * sizeof(int16_t))  // 16 bytes

typedef struct {
    uint16_t                    service_handle;
    ble_gatts_char_handles_t    emg_char_handles;
    ble_gatts_char_handles_t    gain_char_handles;
    uint8_t                     uuid_type;
    uint16_t                    conn_handle;
    bool                        tx_in_progress;  // Flag de controle de transmissão
} ble_emg_service_t;

void ble_emg_service_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context);

uint32_t ble_emg_service_init(ble_emg_service_t * p_emg);

// Função original - mantida para compatibilidade
uint32_t ble_emg_service_notify(ble_emg_service_t * p_emg, uint16_t conn_handle, uint16_t emg_value);

// Nova função para enviar pacotes de múltiplas amostras (otimizado)
uint32_t ble_emg_service_notify_packet(ble_emg_service_t * p_emg, uint16_t conn_handle,
                                        int16_t * p_data, uint16_t num_samples);

#endif // BLE_EMG_SERVICE_H__
