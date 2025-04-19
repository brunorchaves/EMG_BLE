#include "ble_emg_service.h"

#include <string.h>
#include "app_error.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_srv_common.h"

NRF_BLE_GATT_DEF(m_gatt);
BLE_ADVERTISING_DEF(m_advertising);

static ble_uuid_t m_emg_uuid;
static ble_gatts_char_handles_t m_emg_char_handles;
static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;
static uint16_t m_emg_service_handle = 0;

#define EMG_SERVICE_UUID          0x0000
#define EMG_CHAR_UUID             0x0001

static const ble_uuid128_t EMG_BASE_UUID = {
    .uuid128 = {0x14, 0x12, 0x8A, 0x76, 0x04, 0xD1, 0x6C, 0x4F,
                0x7E, 0x53, 0xF2, 0xE8, 0x00, 0x00, 0xB1, 0x19}
};

void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            ble_emg_advertising_start();
            break;

        default:
            break;
    }
}

NRF_SDH_BLE_OBSERVER(m_ble_observer, 3, ble_evt_handler, NULL);

void ble_emg_stack_init(void)
{
    ret_code_t err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(1, &ram_start);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);
}

void ble_emg_gap_init(void)
{
    ble_gap_conn_sec_mode_t sec_mode;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    sd_ble_gap_device_name_set(&sec_mode, (const uint8_t *)"XIAO_BLE_EMG", strlen("XIAO_BLE_EMG"));
}

void ble_emg_gatt_init(void)
{
    nrf_ble_gatt_init(&m_gatt, NULL);
}

void ble_emg_advertising_init(void)
{
    ble_advdata_t advdata;
    ble_uuid_t adv_uuids[] = {
        {EMG_SERVICE_UUID, m_emg_uuid.type}
    };

    memset(&advdata, 0, sizeof(advdata));
    advdata.name_type = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance = false;
    advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    ble_advdata_t srdata;
    memset(&srdata, 0, sizeof(srdata));
    srdata.uuids_complete.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
    srdata.uuids_complete.p_uuids = adv_uuids;

    ble_advertising_init_t init = {0};
    init.advdata = advdata;
    init.srdata = srdata;
    init.config.ble_adv_fast_enabled = true;
    init.config.ble_adv_fast_interval = 64;
    init.config.ble_adv_fast_timeout = 180;

    init.evt_handler = NULL;
    ble_advertising_init(&m_advertising, &init);
    ble_advertising_conn_cfg_tag_set(&m_advertising, 1);
}

void ble_emg_advertising_start(void)
{
    ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
}

void ble_emg_service_init(void)
{
    ret_code_t err_code;

    err_code = sd_ble_uuid_vs_add(&EMG_BASE_UUID, &m_emg_uuid.type);
    APP_ERROR_CHECK(err_code);

    m_emg_uuid.uuid = EMG_SERVICE_UUID;

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &m_emg_uuid,
                                        &m_emg_service_handle);
    APP_ERROR_CHECK(err_code);

    ble_gatts_char_md_t char_md = {0};
    char_md.char_props.notify = 1;
    char_md.char_props.read = 1;

    ble_uuid_t char_uuid = {
        .uuid = EMG_CHAR_UUID,
        .type = m_emg_uuid.type
    };

    ble_gatts_attr_md_t attr_md = {0};
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    attr_md.vloc = BLE_GATTS_VLOC_STACK;

    ble_gatts_attr_t attr_char_value = {0};
    attr_char_value.p_uuid = &char_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len = sizeof(int16_t);
    attr_char_value.max_len = sizeof(int16_t);
    attr_char_value.p_value = NULL;

    err_code = sd_ble_gatts_characteristic_add(m_emg_service_handle,
                                               &char_md,
                                               &attr_char_value,
                                               &m_emg_char_handles);
    APP_ERROR_CHECK(err_code);
}

void ble_emg_notify(int16_t sample)
{
    if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        uint16_t len = sizeof(sample);
        ble_gatts_hvx_params_t hvx_params = {
            .handle = m_emg_char_handles.value_handle,
            .type   = BLE_GATT_HVX_NOTIFICATION,
            .offset = 0,
            .p_len  = &len,
            .p_data = (uint8_t *)&sample
        };
        sd_ble_gatts_hvx(m_conn_handle, &hvx_params);
    }
}
