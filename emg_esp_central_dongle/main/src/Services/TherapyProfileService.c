#include "Services/TherapyProfileService.h"

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
    IDX_THERAPY_SRV,
    IDX_CURRENT_STATE_CHAR,
    IDX_CURRENT_STATE_CHAR_VAL,
    IDX_REQ_STATE_CHAR,
    IDX_REQ_STATE_CHAR_VAL,
    IDX_LEAK_LEVEL_CHAR,
    IDX_LEAK_LEVEL_CHAR_VAL,
    IDX_HUMIDIFIER_LEVEL_CHAR,
    IDX_HUMIDIFIER_LEVEL_CHAR_VAL,
    IDX_RAMP_TIME_LEFT_CHAR,
    IDX_RAMP_TIME_LEFT_CHAR_VAL,
    IDX_REQ_LIST_CHAR,
    IDX_REQ_LIST_CHAR_VAL,
    IDX_LIST_EMPTY_CHAR,
    IDX_LIST_EMPTY_CHAR_VAL,
    IDX_REQ_BY_ID_CHAR,
    IDX_REQ_BY_ID_CHAR_VAL,
    IDX_DATA_CHAR,
    IDX_DATA_CHAR_VAL,
    DB_SIZE,
};

typedef enum CPAPState_E {
    IDLE,
    LEAKAGE_TEST,
    CPAP_ON
} CPAPState_E;

// clang-format off
#define THERAPY_SRV_UUID            {0x7d, 0x1c, 0xe3, 0x91, 0x47, 0xd0, 0x48, 0x6c, 0x42, 0x00, 0x5c, 0x3c, 0xf4, 0xc3, 0xc3, 0xaf}
#define CURRENT_STATE_CHAR_UUID     {0x7d, 0x1c, 0xe3, 0x91, 0x47, 0xd0, 0x48, 0x6c, 0x42, 0x01, 0x5c, 0x3c, 0xf4, 0xc3, 0xc3, 0xaf}
#define REQ_STATE_CHAR_UUID         {0x7d, 0x1c, 0xe3, 0x91, 0x47, 0xd0, 0x48, 0x6c, 0x42, 0x02, 0x5c, 0x3c, 0xf4, 0xc3, 0xc3, 0xaf}
#define LEAK_LEVEL_CHAR_UUID        {0x7d, 0x1c, 0xe3, 0x91, 0x47, 0xd0, 0x48, 0x6c, 0x42, 0x03, 0x5c, 0x3c, 0xf4, 0xc3, 0xc3, 0xaf}
#define HUMIDIFIER_LEVEL_CHAR_UUID  {0x7d, 0x1c, 0xe3, 0x91, 0x47, 0xd0, 0x48, 0x6c, 0x42, 0x04, 0x5c, 0x3c, 0xf4, 0xc3, 0xc3, 0xaf}
#define RAMP_TIME_LEFT_CHAR_UUID    {0x7d, 0x1c, 0xe3, 0x91, 0x47, 0xd0, 0x48, 0x6c, 0x42, 0x05, 0x5c, 0x3c, 0xf4, 0xc3, 0xc3, 0xaf}
#define REQ_LIST_CHAR_UUID          {0x7d, 0x1c, 0xe3, 0x91, 0x47, 0xd0, 0x48, 0x6c, 0x42, 0x06, 0x5c, 0x3c, 0xf4, 0xc3, 0xc3, 0xaf}
#define LIST_EMPTY_CHAR_UUID        {0x7d, 0x1c, 0xe3, 0x91, 0x47, 0xd0, 0x48, 0x6c, 0x42, 0x07, 0x5c, 0x3c, 0xf4, 0xc3, 0xc3, 0xaf}
#define REQ_BY_ID_CHAR_UUID         {0x7d, 0x1c, 0xe3, 0x91, 0x47, 0xd0, 0x48, 0x6c, 0x42, 0x08, 0x5c, 0x3c, 0xf4, 0xc3, 0xc3, 0xaf}
#define DATA_CHAR_UUID              {0x7d, 0x1c, 0xe3, 0x91, 0x47, 0xd0, 0x48, 0x6c, 0x42, 0x09, 0x5c, 0x3c, 0xf4, 0xc3, 0xc3, 0xaf}
// clang-format on

static const uint8_t charValueIdx[THERAPY_CHAR_SIZE] = {
    IDX_CURRENT_STATE_CHAR_VAL,
    IDX_REQ_STATE_CHAR_VAL,
    IDX_LEAK_LEVEL_CHAR_VAL,
    IDX_HUMIDIFIER_LEVEL_CHAR_VAL,
    IDX_RAMP_TIME_LEFT_CHAR_VAL,
    IDX_REQ_LIST_CHAR_VAL,    // request last night list
    IDX_LIST_EMPTY_CHAR_VAL,  // last night list is empty
    IDX_REQ_BY_ID_CHAR_VAL,   // request last night item
    IDX_DATA_CHAR_VAL,
};

static void WriteCurrentStateCallback(uint16_t len, const uint8_t *value);
static void WriteReqStateCallback(uint16_t len, const uint8_t *value);
static void WriteLeakageLvlCallback(uint16_t len, const uint8_t *value);
static void WriteHumidifierLvlCallback(uint16_t len, const uint8_t *value);
static void WriteRampTimeLeftCallback(uint16_t len, const uint8_t *value);
static void WriteReqLastNightCallback(uint16_t len, const uint8_t *value);
static void WriteReqNightReportByIdCallback(uint16_t len, const uint8_t *value);
static void WriteDataCallback(uint16_t len, const uint8_t *value);

static const uint8_t therapySrvUuid[]          = THERAPY_SRV_UUID;
static const uint8_t currentStateCharUuid[]    = CURRENT_STATE_CHAR_UUID;
static const uint8_t reqStateCharUuid[]        = REQ_STATE_CHAR_UUID;
static const uint8_t leakLevelCharUuid[]       = LEAK_LEVEL_CHAR_UUID;
static const uint8_t humidifierLevelCharUuid[] = HUMIDIFIER_LEVEL_CHAR_UUID;
static const uint8_t rampTimeLeftCharUuid[]    = RAMP_TIME_LEFT_CHAR_UUID;
static const uint8_t reqListCharUuid[]         = REQ_LIST_CHAR_UUID;
static const uint8_t listEmptyCharUuid[]       = LIST_EMPTY_CHAR_UUID;
static const uint8_t reqByIdCharUuid[]         = REQ_BY_ID_CHAR_UUID;
static const uint8_t dataCharUuid[]            = DATA_CHAR_UUID;

static CharWriteEventCB writeEvtCallback[THERAPY_CHAR_SIZE] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

static const char *indexNames[DB_SIZE] = {"THERAPY_SRV",
                                          "CURRENT_STATE_CHAR",
                                          "CURRENT_STATE_CHAR_VAL",
                                          "REQ_STATE_CHAR",
                                          "REQ_STATE_CHAR_VAL",
                                          "LEAK_LEVEL_CHAR",
                                          "LEAK_LEVEL_CHAR_VAL",
                                          "HUMIDIFIER_LEVEL_CHAR",
                                          "HUMIDIFIER_LEVEL_CHAR_VAL",
                                          "RAMP_TIME_LEFT_CHAR",
                                          "RAMP_TIME_LEFT_CHAR_VAL",
                                          "REQ_LIST_CHAR",
                                          "REQ_LIST_CHAR_VAL",
                                          "LIST_EMPTY_CHAR",
                                          "LIST_EMPTY_CHAR_VAL",
                                          "REQ_BY_ID_CHAR",
                                          "REQ_BY_ID_CHAR_VAL",
                                          "DATA_CHAR",
                                          "DATA_CHAR_VAL"};

// Characteristic Values
#define REGISTER_DATA_SIZE 33
static const uint8_t currentState;
static const uint8_t reqState;
static const uint8_t leakLevel;
static const uint8_t humidifierLevel;
static const uint16_t rampTimeLeft;
static const uint8_t reqList;
static const uint8_t listEmpty;
static const uint8_t reqById;
static const uint8_t data[REGISTER_DATA_SIZE];

static uint16_t serviceHandleTable[DB_SIZE];

static uint16_t connectionID;
static uint16_t interfaceType;
static uint8_t isConnected;

// clang-format off
static const esp_gatts_attr_db_t therapyAttributeDatabase[DB_SIZE] = {
    // Service declaration
    [IDX_THERAPY_SRV] =  
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
         {
             .uuid_length = ESP_UUID_LEN_16,
             .uuid_p      = (uint8_t *)&primaryServiceUuid,
             .perm        = ESP_GATT_PERM_READ,
             .max_length  = sizeof(therapySrvUuid),
             .length      = sizeof(therapySrvUuid),
             .value       = (uint8_t *)&therapySrvUuid,
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
    [IDX_REQ_STATE_CHAR] =  
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
    [IDX_REQ_STATE_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&reqStateCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(reqState),
            .length      = sizeof(reqState),
            .value       = (uint8_t *)&reqState,
        }},
    // Characteristic declaration
    [IDX_LEAK_LEVEL_CHAR] =  
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
    [IDX_LEAK_LEVEL_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&leakLevelCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(leakLevel),
            .length      = sizeof(leakLevel),
            .value       = (uint8_t *)&leakLevel,
        }},
    // Characteristic declaration
    [IDX_HUMIDIFIER_LEVEL_CHAR] =  
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
    [IDX_HUMIDIFIER_LEVEL_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&humidifierLevelCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(humidifierLevel),
            .length      = sizeof(humidifierLevel),
            .value       = (uint8_t *)&humidifierLevel,
        }},
    // Characteristic declaration
    [IDX_RAMP_TIME_LEFT_CHAR] =  
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
    [IDX_RAMP_TIME_LEFT_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&rampTimeLeftCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(rampTimeLeft),
            .length      = sizeof(rampTimeLeft),
            .value       = (uint8_t *)&rampTimeLeft,
        }},
    // Characteristic declaration
    [IDX_REQ_LIST_CHAR] =  
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
    [IDX_REQ_LIST_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&reqListCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(reqList),
            .length      = sizeof(reqList),
            .value       = (uint8_t *)&reqList,
        }},
    // Characteristic declaration
    [IDX_LIST_EMPTY_CHAR] =  
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
    [IDX_LIST_EMPTY_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&listEmptyCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(listEmpty),
            .length      = sizeof(listEmpty),
            .value       = (uint8_t *)&listEmpty,
        }},
    // Characteristic declaration
    [IDX_REQ_BY_ID_CHAR] =  
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
    [IDX_REQ_BY_ID_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&reqByIdCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(reqById),
            .length      = sizeof(reqById),
            .value       = (uint8_t *)&reqById,
        }},
    // Characteristic declaration
    [IDX_DATA_CHAR] =  
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
    [IDX_DATA_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&dataCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(data),
            .length      = sizeof(data),
            .value       = (uint8_t *)&data,
        }},
};
// clang-format on

void TherapyProfile_SRV_EventHandler(esp_gatts_cb_event_t event,
                                     esp_gatt_if_t gatts_if,
                                     esp_ble_gatts_cb_param_t *param) {
    esp_ble_gatts_cb_param_t *p_data = (esp_ble_gatts_cb_param_t *)param;

    ESP_LOGI(__FUNCTION__, "EVENT: %d\n", event);

    switch (event) {
        case ESP_GATTS_REG_EVT:
            esp_ble_gatts_create_attr_tab(therapyAttributeDatabase,
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
                    serviceHandleTable[IDX_THERAPY_SRV]);
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
                    for (charId = 0; charId < THERAPY_CHAR_SIZE; ++charId) {
                        printf("%d\n",
                               serviceHandleTable[charValueIdx[charId]]);

                        if (p_data->write.handle
                            == serviceHandleTable[charValueIdx[charId]]) {
                            MainUc_SRV_AppWriteEvtCB(
                                (uint8_t)THERAPY_PROFILE_APP,
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
                for (charId = 0; charId < THERAPY_CHAR_SIZE; ++charId) {
                    if (p_data->read.handle
                        == serviceHandleTable[charValueIdx[charId]]) {
                        TherapyProfile_SRV_GetCharValue(
                            (TherapyCharacteristic_E)charId,
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

void TherapyProfile_SRV_SetCharValue(uint8_t charId, uint8_t size,
                                     const uint8_t* value) {
    esp_ble_gatts_set_attr_value(serviceHandleTable[charValueIdx[charId]], size,
                                 value);
    if (*therapyAttributeDatabase[charValueIdx[charId] - 1].att_desc.value &
        ESP_GATT_CHAR_PROP_BIT_NOTIFY) {
        esp_ble_gatts_send_indicate(interfaceType, connectionID,
                                    serviceHandleTable[charValueIdx[charId]],
                                    size, (uint8_t*)value, false);

    } else if (*therapyAttributeDatabase[charValueIdx[charId] - 1]
                    .att_desc.value &
               ESP_GATT_CHAR_PROP_BIT_INDICATE) {
        esp_ble_gatts_send_indicate(interfaceType, connectionID,
                                    serviceHandleTable[charValueIdx[charId]],
                                    size, (uint8_t*)value, true);
    }
}

void TherapyProfile_SRV_GetCharValue(TherapyCharacteristic_E id,
                                     uint8_t size,
                                     const uint8_t *value) {
    uint16_t size16         = size;
    const uint8_t *valuePtr = value;
    esp_ble_gatts_get_attr_value(serviceHandleTable[charValueIdx[id]],
                                 &size16,
                                 &valuePtr);
}
