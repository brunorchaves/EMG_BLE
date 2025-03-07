#include "Services/BluetoothService.h"

#include <string.h>

#include "Services/CalibrationProfileService.h"
#include "Services/DeviceConfigProfileService.h"
#include "Services/GeneralInfoProfileService.h"
#include "Services/MainUcService.h"
#include "Services/TherapyProfileService.h"

#include "App/AppData.h"
#include <esp_bt.h>
#include <esp_bt_defs.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_common_api.h>
#include <esp_gatts_api.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#define DEVICE_NAME_PREFIX "OXYGENIS"
#define DEVICE_NAME_SIZE   13
#define TAG "BLE_INIT"

static char bleDeviceName[DEVICE_NAME_SIZE];

#define MAC_ADDRESS_BUFF_SIZE ((uint8_t)6)
uint8_t macAddress[MAC_ADDRESS_BUFF_SIZE];

static void Bluetooth_SRV_GAPEventHandler(esp_gap_ble_cb_event_t event,
                                          esp_ble_gap_cb_param_t *param);

static void Bluetooth_SRV_GATTSEventHandler(esp_gatts_cb_event_t event,
                                            esp_gatt_if_t gatts_if,
                                            esp_ble_gatts_cb_param_t *param);

struct gattsProfile {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

static struct gattsProfile profileTable[PROFILE_NUM] = {
    [GENERAL_PROFILE_APP] =
        {
            .gatts_cb = GeneralProfile_SRV_EventHandler,
            .gatts_if = ESP_GATT_IF_NONE,
        },
    [DEVICE_CONFIG_PROFILE_APP] =
        {
            .gatts_cb = DeviceConfig_SRV_EventHandler,
            .gatts_if = ESP_GATT_IF_NONE,
        },
    [THERAPY_PROFILE_APP] =
        {
            .gatts_cb = TherapyProfile_SRV_EventHandler,
            .gatts_if = ESP_GATT_IF_NONE,
        },
    [CALIBRATION_PROFILE_APP] =
        {
            .gatts_cb = CalibrationProfile_SRV_EventHandler,
            .gatts_if = ESP_GATT_IF_NONE,
        },
};

static esp_ble_adv_params_t advParams = {
    .adv_int_min       = 0x20,
    .adv_int_max       = 0x40,
    .adv_type          = ADV_TYPE_IND,
    .own_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .channel_map       = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static uint8_t manufacturer_data[] = {0x42, 0x42};

static esp_ble_adv_data_t advData = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = false,
    .min_interval        = 0x06,  // Time  = val * 1.25 msec
    .max_interval        = 0x10,  // Time  = val * 1.25 msec
    .appearance          = 0,
    .manufacturer_len    = sizeof(manufacturer_data),
    .p_manufacturer_data = manufacturer_data,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = 0,
    .p_service_uuid      = NULL,
    .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT};

// "ESP_GATT_UUID_xxx" Attribute Types
const uint16_t primaryServiceUuid = ESP_GATT_UUID_PRI_SERVICE;
const uint16_t characteristicUuid = ESP_GATT_UUID_CHAR_DECLARE;
const uint16_t configurationUuid  = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

// Characteristic Properties
const uint8_t characteristicPropRead  = ESP_GATT_CHAR_PROP_BIT_READ;
const uint8_t characteristicPropWrite = ESP_GATT_CHAR_PROP_BIT_WRITE;
const uint8_t characteristicPropReadNotify =
    ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
const uint8_t characteristicPropReadWrite =
    ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE;
const uint8_t characteristicPropReadWriteNotify =
    ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE
    | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

static uint8_t isConnected;
static uint8_t isInitialized;

int32_t Bluetooth_SRV_Init() {
    esp_err_t ret;

    // Release memory allocated for classic BT if needed
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
        ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "BT Controller memory release failed: %s", esp_err_to_name(ret));
            return -1;
        }
    }

    // Initializes BT Controller if not already initialized
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        ret = esp_bt_controller_init(&bt_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "BT Controller init failed: %s", esp_err_to_name(ret));
            return -1;
        }
    }

    // Enable the BT Controller if not already enabled
    if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED) {
        ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "BT Controller enable failed: %s", esp_err_to_name(ret));
            return -1;
        }
    }

    // Initialize and enable Bluedroid if not already done
    if (!esp_bluedroid_get_status()) {
        ret = esp_bluedroid_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
            return -1;
        }

        ret = esp_bluedroid_enable();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
            return -1;
        }
    }

    // Register callbacks and applications
    if (esp_ble_gatts_register_callback(Bluetooth_SRV_GATTSEventHandler) != ESP_OK) {
        ESP_LOGE(TAG, "GATT event handler registration failed");
        return -1;
    }

    if (esp_ble_gap_register_callback(Bluetooth_SRV_GAPEventHandler) != ESP_OK) {
        ESP_LOGE(TAG, "GAP event handler registration failed");
        return -1;
    }

    if (esp_ble_gatts_app_register(GENERAL_PROFILE_APP) != ESP_OK) {
        ESP_LOGE(TAG, "GATT app register GENERAL_PROFILE_APP failed");
        return -1;
    }

    if (esp_ble_gatts_app_register(DEVICE_CONFIG_PROFILE_APP) != ESP_OK) {
        ESP_LOGE(TAG, "GATT app register DEVICE_CONFIG_PROFILE_APP failed");
        return -1;
    }

    if (esp_ble_gatts_app_register(THERAPY_PROFILE_APP) != ESP_OK) {
        ESP_LOGE(TAG, "GATT app register THERAPY_PROFILE_APP failed");
        return -1;
    }

    if (esp_ble_gatts_app_register(CALIBRATION_PROFILE_APP) != ESP_OK) {
        ESP_LOGE(TAG, "GATT app register CALIBRATION_PROFILE_APP failed");
        return -1;
    }

    // Start advertising
    if (esp_ble_gap_start_advertising(&advParams) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start advertising");
        return -1;
    }

    isInitialized = 1;
    ESP_LOGI(TAG, "Bluetooth initialized successfully");
    return 0;
}

int32_t Bluetooth_SRV_Deinit() {
    if (!isInitialized) {
        ESP_LOGI(TAG, "Bluetooth already deinitialized");
        return 0;  // Already deinitialized
    }

    // Unregister applications
    esp_err_t ret;
    ret = esp_ble_gatts_app_unregister(GENERAL_PROFILE_APP);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to unregister GENERAL_PROFILE_APP: %s", esp_err_to_name(ret));
    }

    ret = esp_ble_gatts_app_unregister(DEVICE_CONFIG_PROFILE_APP);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to unregister DEVICE_CONFIG_PROFILE_APP: %s", esp_err_to_name(ret));
    }

    ret = esp_ble_gatts_app_unregister(THERAPY_PROFILE_APP);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to unregister THERAPY_PROFILE_APP: %s", esp_err_to_name(ret));
    }

    ret = esp_ble_gatts_app_unregister(CALIBRATION_PROFILE_APP);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to unregister CALIBRATION_PROFILE_APP: %s", esp_err_to_name(ret));
    }

    // Disable Bluedroid
    ret = esp_bluedroid_disable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid disable failed: %s", esp_err_to_name(ret));
        return -1;
    }

    // Stop scanning if it is active
    ret = esp_ble_gap_stop_scanning();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to stop scanning: %s", esp_err_to_name(ret));
        return -1;
    }

    // Deinitialize Bluedroid
    ret = esp_bluedroid_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid deinit failed: %s", esp_err_to_name(ret));
        return -1;  // Failed to disable and deinitialize Bluedroid
    }

    // Disable BT controller
    ret = esp_bt_controller_disable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BT Controller disable failed: %s", esp_err_to_name(ret));
        return -1;
    }

    // Deinitialize BT controller
    ret = esp_bt_controller_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BT Controller deinit failed: %s", esp_err_to_name(ret));
        return -1;  // Failed to disable and deinitialize BT controller
    }

    isInitialized = 0;
    ESP_LOGI(TAG, "Bluetooth deinitialized successfully");
    return 0;  // Successful deinitialization
}

uint8_t Bluetooth_SRV_ConnectionState(void) {
    if (!isInitialized) {
        return 0;
    }

    return isConnected;
}

uint8_t Bluetooth_SRV_EnabledState(void) { return isInitialized; }

static void Bluetooth_SRV_GAPEventHandler(esp_gap_ble_cb_event_t event,
                                          esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            esp_ble_gap_start_advertising(&advParams);
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            {
                esp_err_t err;
                if ((err = param->adv_start_cmpl.status)
                    != ESP_BT_STATUS_SUCCESS) {
                    ESP_LOGE(__FUNCTION__,
                             "Advertising start failed: %s\n",
                             esp_err_to_name(err));
                }
            }
            break;
        default:
            break;
    }
    return;
}

static void Bluetooth_SRV_GATTSEventHandler(esp_gatts_cb_event_t event,
                                            esp_gatt_if_t gatts_if,
                                            esp_ble_gatts_cb_param_t *param) {
    static uint8_t configDone = 0;
    switch (event) {
        case ESP_GATTS_REG_EVT:
            if (!configDone) {
                esp_read_mac(macAddress, ESP_MAC_BT);
                snprintf(bleDeviceName,
                         DEVICE_NAME_SIZE,
                         "%s%02X%02X",
                         DEVICE_NAME_PREFIX,
                         macAddress[4],
                         macAddress[5]);

                ESP_LOGI(__FUNCTION__, "BLE DEVICE NAME: %s", bleDeviceName);
                esp_ble_gap_set_device_name(bleDeviceName);
                esp_ble_gap_config_adv_data(&advData);
                configDone = 1;
            }

            if (param->reg.status == ESP_GATT_OK) {
                profileTable[param->reg.app_id].gatts_if = gatts_if;
            } else {
                ESP_LOGI(__FUNCTION__,
                         "Reg app failed, app_id %04x, status %d\n",
                         param->reg.app_id,
                         param->reg.status);
                return;
            }
            break;
        case ESP_GATTS_CONNECT_EVT:
            if (!isConnected) {
                ESP_LOGI(__FUNCTION__, "Device Connected");
                isConnected = 1;

                MainUc_SRV_AppWriteEvtCB(
                    (uint8_t)WIRELESS_CONNECTIONS_PROFILE_APP,
                    WIRELESS_CONNECTIONS_BLUETOOTH_PAIRED_STATE_CHAR,
                    sizeof(isConnected),
                    &isConnected);
            }
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            if (isConnected) {
                isConnected = 0;
                ESP_LOGI(__FUNCTION__, "Device Disconnected");
                esp_ble_gap_start_advertising(&advParams);

                MainUc_SRV_AppWriteEvtCB(
                    (uint8_t)WIRELESS_CONNECTIONS_PROFILE_APP,
                    WIRELESS_CONNECTIONS_BLUETOOTH_PAIRED_STATE_CHAR,
                    sizeof(isConnected),
                    &isConnected);
            }
            break;

        default:
            break;
    }

    for (uint8_t i = 0; i < PROFILE_NUM; ++i) {
        // ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every
        // profile cb function
        if (gatts_if == ESP_GATT_IF_NONE
            || gatts_if == profileTable[i].gatts_if) {
            if (profileTable[i].gatts_cb) {
                profileTable[i].gatts_cb(event, gatts_if, param);
            }
        }
    }
}
