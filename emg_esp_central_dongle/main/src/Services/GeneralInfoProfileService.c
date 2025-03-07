#include "Services/GeneralInfoProfileService.h"

#include <string.h>

#include "Services/BluetoothService.h"
#include "Services/MainUcService.h"

#include <esp_bt.h>
#include <esp_bt_defs.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_common_api.h>
#include <esp_gatts_api.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

enum {
    IDX_GENERAL_INFO_SRV,

    IDX_NAME_CHAR,
    IDX_NAME_CHAR_VAL,

    IDX_MAIN_FW_VERSION_CHAR,
    IDX_MAIN_FW_VERSION_CHAR_VAL,

    IDX_BLE_FW_VERSION_CHAR,
    IDX_BLE_FW_VERSION_CHAR_VAL,

    IDX_PROTOCOL_VERSION_CHAR,
    IDX_PROTOCOL_VERSION_CHAR_VAL,

    IDX_SERIAL_NUMBER_CHAR,
    IDX_SERIAL_NUMBER_CHAR_VAL,

    IDX_DEVICE_CODE_CHAR,
    IDX_DEVICE_CODE_CHAR_VAL,

    IDX_FREE_STORAGE_CHAR,
    IDX_FREE_STORAGE_CHAR_VAL,

    IDX_TOTAL_STORAGE_CHAR,
    IDX_TOTAL_STORAGE_CHAR_VAL,

    IDX_BATTERY_LEVEL_CHAR,
    IDX_BATTERY_LEVEL_CHAR_VAL,

    DB_SIZE,
};

static const uint8_t charValueIdx[GEN_INFO_CHAR_SIZE] = {
    IDX_NAME_CHAR_VAL,
    IDX_MAIN_FW_VERSION_CHAR_VAL,
    IDX_BLE_FW_VERSION_CHAR_VAL,
    IDX_PROTOCOL_VERSION_CHAR_VAL,
    IDX_SERIAL_NUMBER_CHAR_VAL,
    IDX_DEVICE_CODE_CHAR_VAL,
    IDX_FREE_STORAGE_CHAR_VAL,
    IDX_TOTAL_STORAGE_CHAR_VAL,
    IDX_BATTERY_LEVEL_CHAR_VAL,
};

// clang-format off
#define GENERAL_INFO_SRV_UUID      {0x06, 0xfd, 0x9f, 0x98, 0xb8, 0x1c, 0x45, 0x9a, 0x42, 0x00, 0x18, 0x3b, 0xe9, 0x6e, 0x4c, 0x0c}
#define NAME_CHAR_UUID             {0x06, 0xfd, 0x9f, 0x98, 0xb8, 0x1c, 0x45, 0x9a, 0x42, 0x01, 0x18, 0x3b, 0xe9, 0x6e, 0x4c, 0x0c}
#define MAIN_FW_VERSION_CHAR_UUID  {0x06, 0xfd, 0x9f, 0x98, 0xb8, 0x1c, 0x45, 0x9a, 0x42, 0x02, 0x18, 0x3b, 0xe9, 0x6e, 0x4c, 0x0c}
#define BLE_FW_VERSION_CHAR_UUID   {0x06, 0xfd, 0x9f, 0x98, 0xb8, 0x1c, 0x45, 0x9a, 0x42, 0x03, 0x18, 0x3b, 0xe9, 0x6e, 0x4c, 0x0c}
#define PROTOCOL_VERSION_CHAR_UUID {0x06, 0xfd, 0x9f, 0x98, 0xb8, 0x1c, 0x45, 0x9a, 0x42, 0x04, 0x18, 0x3b, 0xe9, 0x6e, 0x4c, 0x0c}
#define SERIAL_NUMBER_CHAR_UUID    {0x06, 0xfd, 0x9f, 0x98, 0xb8, 0x1c, 0x45, 0x9a, 0x42, 0x05, 0x18, 0x3b, 0xe9, 0x6e, 0x4c, 0x0c}
#define DEVICE_CODE_CHAR_UUID      {0x06, 0xfd, 0x9f, 0x98, 0xb8, 0x1c, 0x45, 0x9a, 0x42, 0x06, 0x18, 0x3b, 0xe9, 0x6e, 0x4c, 0x0c}
#define FREE_STORAGE_CHAR_UUID     {0x06, 0xfd, 0x9f, 0x98, 0xb8, 0x1c, 0x45, 0x9a, 0x42, 0x07, 0x18, 0x3b, 0xe9, 0x6e, 0x4c, 0x0c}
#define TOTAL_STORAGE_CHAR_UUID    {0x06, 0xfd, 0x9f, 0x98, 0xb8, 0x1c, 0x45, 0x9a, 0x42, 0x08, 0x18, 0x3b, 0xe9, 0x6e, 0x4c, 0x0c}
#define BATTERY_LEVEL_CHAR_UUID    {0x06, 0xfd, 0x9f, 0x98, 0xb8, 0x1c, 0x45, 0x9a, 0x42, 0x09, 0x18, 0x3b, 0xe9, 0x6e, 0x4c, 0x0c}
// clang-format on

const uint8_t generalInfoSrvUuid[ESP_UUID_LEN_128] = GENERAL_INFO_SRV_UUID;
static const uint8_t nameCharUuid[]                = NAME_CHAR_UUID;
static const uint8_t mainFwVersionCharUuid[]       = MAIN_FW_VERSION_CHAR_UUID;
static const uint8_t bleFwVersionCharUuid[]        = BLE_FW_VERSION_CHAR_UUID;
static const uint8_t protocolVersionCharUuid[]     = PROTOCOL_VERSION_CHAR_UUID;
static const uint8_t serialNumberCharUuid[]        = SERIAL_NUMBER_CHAR_UUID;
static const uint8_t deviceCodeCharUuid[]          = DEVICE_CODE_CHAR_UUID;
static const uint8_t freeStorageCharUuid[]         = FREE_STORAGE_CHAR_UUID;
static const uint8_t totalStorageCharUuid[]        = TOTAL_STORAGE_CHAR_UUID;
static const uint8_t batteryLevelCharUuid[]        = BATTERY_LEVEL_CHAR_UUID;

static const char *indexNames[DB_SIZE] = {"GENERAL_INFO_SRV",
                                          "NAME_CHAR",
                                          "NAME_CHAR_VAL",
                                          "MAIN_FW_VERSION_CHAR",
                                          "MAIN_FW_VERSION_CHAR_VAL",
                                          "BLE_FW_VERSION_CHAR",
                                          "BLE_FW_VERSION_CHAR_VAL",
                                          "PROTOCOL_VERSION_CHAR",
                                          "PROTOCOL_VERSION_CHAR_VAL",
                                          "SERIAL_NUMBER_CHAR",
                                          "SERIAL_NUMBER_CHAR_VAL",
                                          "DEVICE_CODE_CHAR",
                                          "DEVICE_CODE_CHAR_VAL",
                                          "FREE_STORAGE_CHAR",
                                          "FREE_STORAGE_CHAR_VAL",
                                          "TOTAL_STORAGE_CHAR",
                                          "TOTAL_STORAGE_CHAR_VAL",
                                          "BATTERY_LEVEL_CHAR",
                                          "BATTERY_LEVEL_CHAR_VAL"};

static CharWriteEventCB writeEvtCallback[GEN_INFO_CHAR_SIZE] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

// Characteristic Values
static const char name[13];
static const uint8_t mainFwVersion[3];
static const uint8_t bleFwVersion[3];
static const uint8_t protocolVersion;
static const uint8_t serialNumber[12];
static const uint8_t deviceCode;
static const uint16_t freeStorage;
static const uint16_t totalStorage;
static const uint8_t batteryLevel;

static uint16_t serviceHandleTable[DB_SIZE];

static uint16_t connectionID;
static uint16_t interfaceType;
static uint8_t isConnected;

// clang-format off
static const esp_gatts_attr_db_t generalInfoAttributeDatabase[DB_SIZE] = {
    // Service declaration
    [IDX_GENERAL_INFO_SRV] =  
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
         {
             .uuid_length = ESP_UUID_LEN_16,
             .uuid_p      = (uint8_t *)&primaryServiceUuid,
             .perm        = ESP_GATT_PERM_READ,
             .max_length  = sizeof(generalInfoSrvUuid),
             .length      = sizeof(generalInfoSrvUuid),
             .value       = (uint8_t *)&generalInfoSrvUuid,
         }},
    // Characteristic declaration
    [IDX_NAME_CHAR] =  
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
         {
             .uuid_length = ESP_UUID_LEN_16,
             .uuid_p      = (uint8_t *)&characteristicUuid,
             .perm        = ESP_GATT_PERM_READ,
             .max_length  = sizeof(uint8_t),
             .length      = sizeof(uint8_t),
             .value       = (uint8_t *)&characteristicPropReadWriteNotify,
         }},
    // Characteristic value
    [IDX_NAME_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&nameCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(name),
            .length      = sizeof(name),
            .value       = (uint8_t *)&name,
        }},
    // Characteristic declaration
    [IDX_MAIN_FW_VERSION_CHAR] =  
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
         {
             .uuid_length = ESP_UUID_LEN_16,
             .uuid_p      = (uint8_t *)&characteristicUuid,
             .perm        = ESP_GATT_PERM_READ,
             .max_length  = sizeof(uint8_t),
             .length      = sizeof(uint8_t),
             .value       = (uint8_t *)&characteristicPropRead,
         }},
    // Characteristic value
    [IDX_MAIN_FW_VERSION_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&mainFwVersionCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(mainFwVersion),
            .length      = sizeof(mainFwVersion),
            .value       = (uint8_t *)&mainFwVersion,
        }},
    // Characteristic declaration
    [IDX_BLE_FW_VERSION_CHAR] =  
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
         {
             .uuid_length = ESP_UUID_LEN_16,
             .uuid_p      = (uint8_t *)&characteristicUuid,
             .perm        = ESP_GATT_PERM_READ,
             .max_length  = sizeof(uint8_t),
             .length      = sizeof(uint8_t),
             .value       = (uint8_t *)&characteristicPropRead,
         }},
    // Characteristic value
    [IDX_BLE_FW_VERSION_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&bleFwVersionCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(bleFwVersion),
            .length      = sizeof(bleFwVersion),
            .value       = (uint8_t *)&bleFwVersion,
        }},
    // Characteristic declaration
    [IDX_PROTOCOL_VERSION_CHAR] =  
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
         {
             .uuid_length = ESP_UUID_LEN_16,
             .uuid_p      = (uint8_t *)&characteristicUuid,
             .perm        = ESP_GATT_PERM_READ,
             .max_length  = sizeof(uint8_t),
             .length      = sizeof(uint8_t),
             .value       = (uint8_t *)&characteristicPropRead,
         }},
    // Characteristic value
    [IDX_PROTOCOL_VERSION_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&protocolVersionCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(protocolVersion),
            .length      = sizeof(protocolVersion),
            .value       = (uint8_t *)&protocolVersion,
        }},
    // Characteristic declaration
    [IDX_SERIAL_NUMBER_CHAR] =  
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
         {
             .uuid_length = ESP_UUID_LEN_16,
             .uuid_p      = (uint8_t *)&characteristicUuid,
             .perm        = ESP_GATT_PERM_READ,
             .max_length  = sizeof(uint8_t),
             .length      = sizeof(uint8_t),
             .value       = (uint8_t *)&characteristicPropRead,
         }},
    // Characteristic value
    [IDX_SERIAL_NUMBER_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&serialNumberCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(serialNumber),
            .length      = sizeof(serialNumber),
            .value       = (uint8_t *)&serialNumber,
        }},
    // Characteristic declaration
    [IDX_DEVICE_CODE_CHAR] =  
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
         {
             .uuid_length = ESP_UUID_LEN_16,
             .uuid_p      = (uint8_t *)&characteristicUuid,
             .perm        = ESP_GATT_PERM_READ,
             .max_length  = sizeof(uint8_t),
             .length      = sizeof(uint8_t),
             .value       = (uint8_t *)&characteristicPropRead,
         }},
    // Characteristic value
    [IDX_DEVICE_CODE_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&deviceCodeCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(deviceCode),
            .length      = sizeof(deviceCode),
            .value       = (uint8_t *)&deviceCode,
        }},
    // Characteristic declaration
    [IDX_FREE_STORAGE_CHAR] =  
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
         {
             .uuid_length = ESP_UUID_LEN_16,
             .uuid_p      = (uint8_t *)&characteristicUuid,
             .perm        = ESP_GATT_PERM_READ,
             .max_length  = sizeof(uint8_t),
             .length      = sizeof(uint8_t),
             .value       = (uint8_t *)&characteristicPropReadNotify,
         }},
    // Characteristic value
    [IDX_FREE_STORAGE_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&freeStorageCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(freeStorage),
            .length      = sizeof(freeStorage),
            .value       = (uint8_t *)&freeStorage,
        }},
    // Characteristic declaration
    [IDX_TOTAL_STORAGE_CHAR] =  
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
         {
             .uuid_length = ESP_UUID_LEN_16,
             .uuid_p      = (uint8_t *)&characteristicUuid,
             .perm        = ESP_GATT_PERM_READ,
             .max_length  = sizeof(uint8_t),
             .length      = sizeof(uint8_t),
             .value       = (uint8_t *)&characteristicPropReadNotify,
         }},
    // Characteristic value
    [IDX_TOTAL_STORAGE_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&totalStorageCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(totalStorage),
            .length      = sizeof(totalStorage),
            .value       = (uint8_t *)&totalStorage,
        }},
    // Characteristic declaration
    [IDX_BATTERY_LEVEL_CHAR] =  
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
         {
             .uuid_length = ESP_UUID_LEN_16,
             .uuid_p      = (uint8_t *)&characteristicUuid,
             .perm        = ESP_GATT_PERM_READ,
             .max_length  = sizeof(uint8_t),
             .length      = sizeof(uint8_t),
             .value       = (uint8_t *)&characteristicPropReadNotify,
         }},
    // Characteristic value
    [IDX_BATTERY_LEVEL_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&batteryLevelCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(batteryLevel),
            .length      = sizeof(batteryLevel),
            .value       = (uint8_t *)&batteryLevel,
        }},
};
// clang-format on

void GeneralProfile_SRV_EventHandler(esp_gatts_cb_event_t event,
                                     esp_gatt_if_t gatts_if,
                                     esp_ble_gatts_cb_param_t *param) {
    esp_ble_gatts_cb_param_t *p_data = (esp_ble_gatts_cb_param_t *)param;

    ESP_LOGI(__FUNCTION__, "EVENT: %d\n", event);

    switch (event) {
        case ESP_GATTS_REG_EVT:
            esp_ble_gatts_create_attr_tab(generalInfoAttributeDatabase,
                                          gatts_if,
                                          DB_SIZE,
                                          0);
            break;
        case ESP_GATTS_CONNECT_EVT:
            connectionID  = p_data->connect.conn_id;
            interfaceType = gatts_if;
            isConnected   = 1;
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            connectionID  = 0;
            interfaceType = ESP_GATT_IF_NONE;
            isConnected   = 0;
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            if ((param->add_attr_tab.status == ESP_GATT_OK)
                && (param->add_attr_tab.num_handle == DB_SIZE)) {
                memcpy(serviceHandleTable,
                       param->add_attr_tab.handles,
                       sizeof(serviceHandleTable));
                esp_ble_gatts_start_service(
                    serviceHandleTable[IDX_GENERAL_INFO_SRV]);
            } else {
                ESP_LOGE(__FUNCTION__, "CREATE ATTRIBUTE TABLE ERROR!");
            }
            break;
        case ESP_GATTS_WRITE_EVT:
            {
                if (p_data->write.len < 255) {
                    uint16_t j;
                    for (j = 0; j < DB_SIZE; ++j) {
                        if (p_data->write.handle == serviceHandleTable[j])
                            break;
                    }
                    printf("%s: ", indexNames[j]);
                    ESP_LOGI(__FUNCTION__, "ESP_GATTS_WRITE_EVT:");
                    for (uint16_t i = 0; i < p_data->write.len; ++i) {
                        printf("0x%02x ", p_data->write.value[i]);
                    }
                    printf("\n");

                    uint8_t charId = 0;
                    for (charId = 0; charId < GEN_INFO_CHAR_SIZE; ++charId) {
                        printf("%d\n",
                               serviceHandleTable[charValueIdx[charId]]);

                        if (p_data->write.handle
                            == serviceHandleTable[charValueIdx[charId]]) {
                            MainUc_SRV_AppWriteEvtCB(
                                (uint8_t)GENERAL_PROFILE_APP,
                                charId,
                                p_data->write.len,
                                p_data->write.value);
                            break;
                        }
                    }
                }
            }
            break;

        case ESP_GATTS_READ_EVT:
            {
                uint8_t charId = 0;
                for (charId = 0; charId < GEN_INFO_CHAR_SIZE; ++charId) {
                    if (p_data->read.handle
                        == serviceHandleTable[charValueIdx[charId]]) {
                        GeneralInfoProfile_SRV_GetCharValue(
                            (GeneralInfoCharacteristic_E)charId,
                            p_data->write.len,
                            p_data->write.value);
                        break;
                    }
                }
            }
        default:
            break;
    }
}

void GeneralInfoProfile_SRV_SetCharValue(uint8_t charId,
                                         uint8_t size,
                                         const uint8_t *value) {
    if (charId >= DB_SIZE) {
        return;
    }
    
    esp_ble_gatts_set_attr_value(serviceHandleTable[charValueIdx[charId]],
                                 size,
                                 value);
    if (*generalInfoAttributeDatabase[charValueIdx[charId] - 1].att_desc.value
        & ESP_GATT_CHAR_PROP_BIT_NOTIFY) {
        esp_ble_gatts_send_indicate(interfaceType,
                                    connectionID,
                                    serviceHandleTable[charValueIdx[charId]],
                                    size,
                                    (uint8_t *)value,
                                    false);
    } else if (*generalInfoAttributeDatabase[charValueIdx[charId] - 1]
                    .att_desc.value
               & ESP_GATT_CHAR_PROP_BIT_INDICATE) {
        esp_ble_gatts_send_indicate(interfaceType,
                                    connectionID,
                                    serviceHandleTable[charValueIdx[charId]],
                                    size,
                                    (uint8_t *)value,
                                    true);
    }
}

void GeneralInfoProfile_SRV_GetCharValue(GeneralInfoCharacteristic_E id,
                                         uint8_t size,
                                         const uint8_t *value) {
    uint16_t size16         = size;
    const uint8_t *valuePtr = value;
    esp_ble_gatts_get_attr_value(serviceHandleTable[charValueIdx[id]],
                                 &size16,
                                 &valuePtr);
}
