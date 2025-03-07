#include "Services/CalibrationProfileService.h"

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
    IDX_CALIBRATION_SRV,

    IDX_CURRENT_STATE_CHAR,
    IDX_CURRENT_STATE_CHAR_VAL,

    IDX_MOBILE_REQUEST_CHAR,
    IDX_MOBILE_REQUEST_CHAR_VAL,

    IDX_SENSOR_READ_LEVEL_CHAR,
    IDX_SENSOR_READ_LEVEL_CHAR_VAL,

    IDX_INPUT_LEVEL_CHAR,
    IDX_INPUT_LEVEL_CHAR_VAL,

    // TODO (Vitor): Add self test characteristics

    DB_SIZE,
};

static const uint8_t charValueIdx[CALIB_CHAR_SIZE] = {
    IDX_CURRENT_STATE_CHAR_VAL,
    IDX_MOBILE_REQUEST_CHAR_VAL,
    IDX_SENSOR_READ_LEVEL_CHAR_VAL,
    IDX_INPUT_LEVEL_CHAR_VAL,
    // TODO (Vitor): Add self test characteristics
};


// clang-format off
#define CALIBRATION_SRV_UUID        {0xA9, 0xC2, 0xD1, 0x8C, 0x3E, 0xD2, 0xFC, 0x95, 0x00, 0x42, 0x55, 0x48, 0xA5, 0xAF, 0x55, 0x17};
#define CURRENT_STATE_CHAR_UUID     {0xA9, 0xC2, 0xD1, 0x8C, 0x3E, 0xD2, 0xFC, 0x95, 0x01, 0x42, 0x55, 0x48, 0xA5, 0xAF, 0x55, 0x17};
#define MOBILE_REQUEST_CHAR_UUID    {0xA9, 0xC2, 0xD1, 0x8C, 0x3E, 0xD2, 0xFC, 0x95, 0x02, 0x42, 0x55, 0x48, 0xA5, 0xAF, 0x55, 0x17};
#define SENSOR_READ_LEVEL_CHAR_UUID {0xA9, 0xC2, 0xD1, 0x8C, 0x3E, 0xD2, 0xFC, 0x95, 0x03, 0x42, 0x55, 0x48, 0xA5, 0xAF, 0x55, 0x17};
#define INPUT_LEVEL_CHAR_UUID       {0xA9, 0xC2, 0xD1, 0x8C, 0x3E, 0xD2, 0xFC, 0x95, 0x04, 0x42, 0x55, 0x48, 0xA5, 0xAF, 0x55, 0x17};
// clang-format on

static const uint8_t calibrationSrvUuid[ESP_UUID_LEN_128] =
    CURRENT_STATE_CHAR_UUID;
static const uint8_t currentStateCharUuid[ESP_UUID_LEN_128] =
    CURRENT_STATE_CHAR_UUID;
static const uint8_t mobileRequestCharUuid[ESP_UUID_LEN_128] =
    MOBILE_REQUEST_CHAR_UUID;
static const uint8_t sensorReadLevelCharUuid[ESP_UUID_LEN_128] =
    SENSOR_READ_LEVEL_CHAR_UUID;
static const uint8_t inputLevelCharUuid[ESP_UUID_LEN_128] =
    INPUT_LEVEL_CHAR_UUID;

static const char *indexNames[DB_SIZE] = {"CALIBRATION_SRV",
                                          "CURRENT_STATE_CHAR",
                                          "CURRENT_STATE_CHAR_VAL",
                                          "MOBILE_REQUEST_CHAR",
                                          "MOBILE_REQUEST_CHAR_VAL",
                                          "SENSOR_READ_LEVEL_CHAR",
                                          "SENSOR_READ_LEVEL_CHAR_VAL",
                                          "INPUT_LEVEL_CHAR",
                                          "INPUT_LEVEL_CHAR_VAL"};

// Characteristic Values
static const uint8_t currentState;
static const uint8_t mobileRequest;
static const float sensorReadLevel;
static const float inputLevel;

static uint16_t serviceHandleTable[DB_SIZE];

static uint16_t connectionID;
static uint16_t interfaceType;
static uint8_t isConnected;

// clang-format off
static const esp_gatts_attr_db_t errorRegistersAttributeDatabase[DB_SIZE] = {
    // Service declaration
    [IDX_CALIBRATION_SRV] =  
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
         {
             .uuid_length = ESP_UUID_LEN_16,
             .uuid_p      = (uint8_t *)&primaryServiceUuid,
             .perm        = ESP_GATT_PERM_READ,
             .max_length  = sizeof(calibrationSrvUuid),
             .length      = sizeof(calibrationSrvUuid),
             .value       = (uint8_t *)&calibrationSrvUuid,
         }},
    // Characteristic declaration
    [IDX_CURRENT_STATE_CHAR] =  
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
    [IDX_CURRENT_STATE_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&currentStateCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(currentState),
            .length      = sizeof(currentState),
            .value       = (uint8_t *)&currentState,
        }},
    // Characteristic declaration
    [IDX_MOBILE_REQUEST_CHAR] =  
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
    [IDX_MOBILE_REQUEST_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&mobileRequestCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(mobileRequest),
            .length      = sizeof(mobileRequest),
            .value       = (uint8_t *)&mobileRequest,
        }},
    // Characteristic declaration
    [IDX_SENSOR_READ_LEVEL_CHAR] =  
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
    [IDX_SENSOR_READ_LEVEL_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&sensorReadLevelCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(sensorReadLevel),
            .length      = sizeof(sensorReadLevel),
            .value       = (uint8_t *)&sensorReadLevel,
        }},
    // Characteristic declaration
    [IDX_INPUT_LEVEL_CHAR] =  
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
         {
             .uuid_length = ESP_UUID_LEN_16,
             .uuid_p      = (uint8_t *)&characteristicUuid,
             .perm        = ESP_GATT_PERM_READ,
             .max_length  = sizeof(uint8_t),
             .length      = sizeof(uint8_t),
             .value       = (uint8_t *)&characteristicPropWrite,
         }},
    // Characteristic value
    [IDX_INPUT_LEVEL_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&inputLevelCharUuid,
            .perm        = ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(inputLevel),
            .length      = sizeof(inputLevel),
            .value       = (uint8_t *)&inputLevel,
        }},
};

void CalibrationProfile_SRV_EventHandler(esp_gatts_cb_event_t event,
                                     esp_gatt_if_t gatts_if,
                                     esp_ble_gatts_cb_param_t *param) {
    esp_ble_gatts_cb_param_t *p_data = (esp_ble_gatts_cb_param_t *)param;

    switch (event) {
        case ESP_GATTS_REG_EVT:
            esp_ble_gatts_create_attr_tab(errorRegistersAttributeDatabase,
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
                    serviceHandleTable[IDX_CALIBRATION_SRV]);
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
                    for (charId = 0; charId < CALIB_CHAR_SIZE; ++charId) {
                        printf("%d\n",
                               serviceHandleTable[charValueIdx[charId]]);

                        if (p_data->write.handle
                            == serviceHandleTable[charValueIdx[charId]]) {
                            MainUc_SRV_AppWriteEvtCB(
                                (uint8_t)CALIBRATION_PROFILE_APP,
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
                for (charId = 0; charId < CALIB_CHAR_SIZE; ++charId) {
                    if (p_data->read.handle
                        == serviceHandleTable[charValueIdx[charId]]) {
                        CalibrationProfile_SRV_GetCharValue(
                            (CalibProfileCharacteristic_E)charId,
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


void CalibrationProfile_SRV_SetCharValue(uint8_t charId, uint8_t size,
                                         const uint8_t* value) {
    /**
     * @warning Não remover este if!!!
     * Ele garante que as funções que vêm abaixo NÃO farão acesso
     * incorreto à memória gerando reset do ESP!!!
    */
    if(charId >= CALIB_CHAR_SIZE)
    {
        return;
    }
    
    esp_ble_gatts_set_attr_value(serviceHandleTable[charValueIdx[charId]], size,
                                 value);

    if (*errorRegistersAttributeDatabase[charValueIdx[charId] - 1].att_desc.value &
        ESP_GATT_CHAR_PROP_BIT_NOTIFY) {
        esp_ble_gatts_send_indicate(interfaceType, connectionID,
                                    serviceHandleTable[charValueIdx[charId]],
                                    size, (uint8_t*)value, false);
    } else if (*errorRegistersAttributeDatabase[charValueIdx[charId] - 1]
                    .att_desc.value &
               ESP_GATT_CHAR_PROP_BIT_INDICATE) {
        esp_ble_gatts_send_indicate(interfaceType, connectionID,
                                    serviceHandleTable[charValueIdx[charId]],
                                    size, (uint8_t*)value, true);
    }
}

void CalibrationProfile_SRV_GetCharValue(CalibProfileCharacteristic_E id,
                                         uint8_t size,
                                        const uint8_t *value) {
    uint16_t size16         = size;
    const uint8_t *valuePtr = value;
    esp_ble_gatts_get_attr_value(serviceHandleTable[charValueIdx[id]],
                                 &size16,
                                 &valuePtr);
}