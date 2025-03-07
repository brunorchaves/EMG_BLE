#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

static const char *TAG = "BLE_SCAN";

// Callback function for scanning results
static void scan_result_callback(esp_gap_ble_scan_result_evt_param_t *scan_result) {
    if (scan_result->search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
        char name[ESP_BT_GAP_MAX_BDNAME_LEN + 1] = {0};
        uint8_t *adv_name = NULL;
        uint8_t adv_name_len = 0;

        // Try to extract the device name from the advertising data
        adv_name = esp_ble_resolve_adv_data(scan_result->ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);

        if (adv_name) {
            memcpy(name, adv_name, adv_name_len);
            name[adv_name_len] = '\0'; // Null-terminate the string
        } else {
            strcpy(name, "Unknown Device");
        }

        ESP_LOGI(TAG, "Device found: %s | RSSI: %d dBm", name, scan_result->rssi);
    }
}

// GAP event handler
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_SCAN_RESULT_EVT:
            scan_result_callback(&param->scan_rst);
            break;
        default:
            break;
    }
}

static void start_scan() {
    esp_ble_scan_params_t scan_params = {
        .scan_type = BLE_SCAN_TYPE_PASSIVE,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval = 0x50,
        .scan_window = 0x30
    };

    esp_ble_gap_set_scan_params(&scan_params);
    esp_ble_gap_start_scanning(30); // Scan for 30 seconds
}

static void app_init(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // Initialize BLE
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // Register GAP callback
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));

    // Start scanning
    start_scan();
}

void app_main(void) {
    app_init();
    while (1) {
        printf("Scanning for BLE devices...\n");
        vTaskDelay(10000 / portTICK_PERIOD_MS); // Delay for 10 seconds
    }
}

#if defined(__cplusplus)
}
#endif /** __cplusplus */
