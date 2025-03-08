#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"

static const char *TAG = "BLE_SCAN";

esp_ble_scan_params_t ble_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_duplicate = BLE_SCAN_DUPLICATE_ENABLE,
    .scan_interval = 0x50,
    .scan_window = 0x30,
};

static void get_device_name(esp_ble_gap_cb_param_t *param, char *name, int name_len)
{
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;

    // Extract the device name from the advertisement data
    adv_name = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);

    if (adv_name != NULL && adv_name_len > 0) {
        // Copy the device name to the output buffer
        snprintf(name, name_len, "%.*s", adv_name_len, adv_name);
    } else {
        // If no name is found, show "Unknown device"
        snprintf(name, name_len, "Unknown device");
    }
}
void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
            if (param->scan_param_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Scan parameters set, starting scan...");
                esp_ble_gap_start_scanning(10);  // Scan for 10 seconds
            } else {
                ESP_LOGE(TAG, "Failed to set scan parameters");
            }
            break;

        case ESP_GAP_BLE_SCAN_RESULT_EVT:
            if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
                // Get the device name
                char device_name[64];
                get_device_name(param, device_name, sizeof(device_name));

                // Log the device name and RSSI
                ESP_LOGI(TAG, "Device found: Name: %s, RSSI %d", device_name, param->scan_rst.rssi);
            }
            break;

        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "Scan complete");
            break;

        default:
            break;
    }
}
void init_nvs(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void init_ble(void) {
    esp_err_t ret;

    // Initialize the Bluetooth controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "Failed to initialize BT controller: %s", esp_err_to_name(ret));
        return;
    }

    // Enable the Bluetooth controller
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "Failed to enable BT controller: %s", esp_err_to_name(ret));
        return;
    }

    // Initialize Bluedroid
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "Failed to initialize Bluedroid: %s", esp_err_to_name(ret));
        return;
    }

    // Enable Bluedroid
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "Failed to enable Bluedroid: %s", esp_err_to_name(ret));
        return;
    }
}

void app_main(void)
{
    // Initialize NVS
    init_nvs();

    // Initialize BLE
    init_ble();

    // Register GAP callback
    esp_ble_gap_register_callback(gap_event_handler);

    // Set scan parameters
    esp_err_t ret = esp_ble_gap_set_scan_params(&ble_scan_params);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "BLE scan parameters set successfully");
    } else {
        ESP_LOGE(TAG, "Failed to set scan params: %s", esp_err_to_name(ret));
    }
}