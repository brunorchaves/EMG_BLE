#include "sdk_common.h"


#include "ble_emg_service.h"
#include "ble_srv_common.h"
#include "nrf_log.h"


static void on_write(ble_emg_service_t * p_emg, ble_evt_t const * p_ble_evt)
{
    const ble_gatts_evt_write_t * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    if (p_evt_write->handle == p_emg->gain_char_handles.value_handle && p_evt_write->len == 1) {
        uint8_t new_gain = p_evt_write->data[0];

        if (new_gain >= 1 && new_gain <= 10) {
            NRF_LOG_INFO("Gain write received: %d", new_gain);
            // Aqui você salva o novo ganho ou chama um callback
            // Exemplo: p_emg->gain_callback(new_gain); ou armazenar localmente
        } else {
            NRF_LOG_WARNING("Invalid gain value received: %d (valid: 1-10)", new_gain);
        }
    }
}

void ble_emg_service_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context)
{
    ble_emg_service_t * p_emg = (ble_emg_service_t *)p_context;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GATTS_EVT_WRITE:
            on_write(p_emg, p_ble_evt);
            break;

        case BLE_GATTS_EVT_HVN_TX_COMPLETE:
            // Notificação enviada com sucesso, libera para próxima transmissão
            p_emg->tx_in_progress = false;

            // Log periódico de TX complete (a cada 100 eventos = ~1 segundo)
            static uint32_t tx_complete_count = 0;
            if (tx_complete_count % 100 == 0) {
                NRF_LOG_INFO("BLE TX complete: %d notifications sent", tx_complete_count);
            }
            tx_complete_count++;
            break;

        default:
            break;
    }
}

uint32_t ble_emg_service_init(ble_emg_service_t * p_emg)
{
    NRF_LOG_INFO("Initializing EMG BLE service...");
    uint32_t              err_code;
    ble_uuid_t            ble_uuid;
    ble_add_char_params_t add_char_params;

    // Add custom base UUID
    ble_uuid128_t base_uuid = { EMG_SERVICE_UUID_BASE };
    err_code = sd_ble_uuid_vs_add(&base_uuid, &p_emg->uuid_type);
    VERIFY_SUCCESS(err_code);
    NRF_LOG_INFO("EMG service UUID registered");

    ble_uuid.type = p_emg->uuid_type;
    ble_uuid.uuid = EMG_SERVICE_UUID;

    // Add the service
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_emg->service_handle);
    VERIFY_SUCCESS(err_code);
    NRF_LOG_INFO("EMG service added - handle: 0x%04x", p_emg->service_handle);

    // --- Add EMG Characteristic (notify) ---
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid              = EMG_CHAR_UUID;
    add_char_params.uuid_type         = p_emg->uuid_type;
    add_char_params.max_len           = EMG_MAX_PAYLOAD;  // 40 bytes para pacotes
    add_char_params.init_len          = sizeof(uint16_t);
    add_char_params.char_props.notify = 1;
    add_char_params.cccd_write_access = SEC_OPEN;

    err_code = characteristic_add(p_emg->service_handle, &add_char_params, &p_emg->emg_char_handles);
    VERIFY_SUCCESS(err_code);
    NRF_LOG_INFO("EMG notify characteristic added - max payload: %d bytes", EMG_MAX_PAYLOAD);

    // Inicializa flag de controle
    p_emg->tx_in_progress = false;

    // --- Add Gain Characteristic (write) ---
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid              = EMG_GAIN_CHAR_UUID;
    add_char_params.uuid_type         = p_emg->uuid_type;
    add_char_params.max_len           = sizeof(uint8_t);
    add_char_params.init_len          = sizeof(uint8_t);
    add_char_params.char_props.write  = 1;
    add_char_params.write_access      = SEC_OPEN;

    err_code = characteristic_add(p_emg->service_handle, &add_char_params, &p_emg->gain_char_handles);
    VERIFY_SUCCESS(err_code);
    NRF_LOG_INFO("Gain write characteristic added");

    NRF_LOG_INFO("EMG service initialization complete");
    return NRF_SUCCESS;
}

uint32_t ble_emg_service_notify(ble_emg_service_t * p_emg, uint16_t conn_handle, uint16_t emg_value)
{
    if (conn_handle == BLE_CONN_HANDLE_INVALID)
        return NRF_ERROR_INVALID_STATE;

    ble_gatts_hvx_params_t params;
    uint16_t len = sizeof(emg_value);

    memset(&params, 0, sizeof(params));
    params.type   = BLE_GATT_HVX_NOTIFICATION;
    params.handle = p_emg->emg_char_handles.value_handle;
    params.p_data = (uint8_t*)&emg_value;
    params.p_len  = &len;

    return sd_ble_gatts_hvx(conn_handle, &params);
}

// Nova função para enviar pacotes de múltiplas amostras
uint32_t ble_emg_service_notify_packet(ble_emg_service_t * p_emg, uint16_t conn_handle,
                                        int16_t * p_data, uint16_t num_samples)
{
    if (conn_handle == BLE_CONN_HANDLE_INVALID) {
        NRF_LOG_WARNING("Notify packet failed: invalid connection handle");
        return NRF_ERROR_INVALID_STATE;
    }

    // CRITICAL FIX: Check if CCCD is enabled before sending notifications
    uint16_t cccd_value = 0;
    ble_gatts_value_t gatts_value;
    memset(&gatts_value, 0, sizeof(gatts_value));
    gatts_value.len = sizeof(cccd_value);
    gatts_value.p_value = (uint8_t*)&cccd_value;

    uint32_t cccd_err = sd_ble_gatts_value_get(conn_handle,
                                                 p_emg->emg_char_handles.cccd_handle,
                                                 &gatts_value);

    if (cccd_err != NRF_SUCCESS || cccd_value != BLE_GATT_HVX_NOTIFICATION) {
        // CCCD not enabled - silently return (client hasn't subscribed yet)
        return NRF_ERROR_INVALID_STATE;
    }

    if (p_emg->tx_in_progress) {
        // Log apenas em caso de busy recorrente (debug)
        static uint32_t busy_count = 0;
        if (busy_count++ % 100 == 0) {
            NRF_LOG_WARNING("TX busy: %d times", busy_count);
        }
        return NRF_ERROR_BUSY;
    }

    if (num_samples == 0 || p_data == NULL) {
        NRF_LOG_ERROR("Notify packet failed: invalid parameters");
        return NRF_ERROR_INVALID_PARAM;
    }

    ble_gatts_hvx_params_t params;
    uint16_t len = num_samples * sizeof(int16_t);

    // Limita ao tamanho máximo permitido
    if (len > EMG_MAX_PAYLOAD) {
        len = EMG_MAX_PAYLOAD;
        NRF_LOG_WARNING("Packet size limited to %d bytes", EMG_MAX_PAYLOAD);
    }

    memset(&params, 0, sizeof(params));
    params.type   = BLE_GATT_HVX_NOTIFICATION;
    params.handle = p_emg->emg_char_handles.value_handle;
    params.p_data = (uint8_t*)p_data;
    params.p_len  = &len;

    p_emg->tx_in_progress = true;
    uint32_t err_code = sd_ble_gatts_hvx(conn_handle, &params);

    if (err_code != NRF_SUCCESS) {
        p_emg->tx_in_progress = false;
        static uint32_t err_count = 0;
        if (err_count++ % 10 == 0) {
            NRF_LOG_ERROR("Notify packet failed: err=0x%x, count=%d", err_code, err_count);
        }
    }

    return err_code;
}


