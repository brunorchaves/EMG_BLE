#include "sdk_common.h"


#include "ble_emg_service.h"
#include "ble_srv_common.h"


static void on_write(ble_emg_service_t * p_emg, ble_evt_t const * p_ble_evt)
{
    const ble_gatts_evt_write_t * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    if (p_evt_write->handle == p_emg->gain_char_handles.value_handle && p_evt_write->len == 1) {
        uint8_t new_gain = p_evt_write->data[0];

        if (new_gain >= 1 && new_gain <= 10) {
            // Aqui você salva o novo ganho ou chama um callback
            // Exemplo: p_emg->gain_callback(new_gain); ou armazenar localmente
            //NRF_LOG_INFO("Ganho recebido via BLE: %d", new_gain);
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

        default:
            break;
    }
}

uint32_t ble_emg_service_init(ble_emg_service_t * p_emg)
{
    uint32_t              err_code;
    ble_uuid_t            ble_uuid;
    ble_add_char_params_t add_char_params;

    // Add custom base UUID
    ble_uuid128_t base_uuid = { EMG_SERVICE_UUID_BASE };
    err_code = sd_ble_uuid_vs_add(&base_uuid, &p_emg->uuid_type);
    VERIFY_SUCCESS(err_code);

    ble_uuid.type = p_emg->uuid_type;
    ble_uuid.uuid = EMG_SERVICE_UUID;

    // Add the service
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_emg->service_handle);
    VERIFY_SUCCESS(err_code);

    // --- Add EMG Characteristic (notify) ---
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid              = EMG_CHAR_UUID;
    add_char_params.uuid_type         = p_emg->uuid_type;
    add_char_params.max_len           = sizeof(uint16_t);  // 2 bytes EMG signal
    add_char_params.init_len          = sizeof(uint16_t);
    add_char_params.char_props.notify = 1;
    add_char_params.cccd_write_access = SEC_OPEN;

    err_code = characteristic_add(p_emg->service_handle, &add_char_params, &p_emg->emg_char_handles);
    VERIFY_SUCCESS(err_code);

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


