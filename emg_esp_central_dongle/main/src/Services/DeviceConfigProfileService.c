#include "Services/DeviceConfigProfileService.h"

#include <string.h>

#include "Services/BluetoothService.h"
#include "Services/MainUcService.h"

#include "App/AppConfig.h"
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
    IDX_DEVICE_CONFIG_SRV,

    IDX_HUMIDIFIER_ENABLE_CHAR,
    IDX_HUMIDIFIER_ENABLE_CHAR_VAL,

    IDX_HUMIDIFIER_LEVEL_CHAR,
    IDX_HUMIDIFIER_LEVEL_CHAR_VAL,

    IDX_RAMP_TIME_CHAR,
    IDX_RAMP_TIME_CHAR_VAL,

    IDX_OUT_RELIEF_ENABLE_CHAR,
    IDX_OUT_RELIEF_ENABLE_CHAR_VAL,

    IDX_OUT_RELIEF_LEVEL_CHAR,
    IDX_OUT_RELIEF_LEVEL_CHAR_VAL,

    IDX_FLOW_RESPONSE_ENABLE_CHAR,
    IDX_FLOW_RESPONSE_ENABLE_CHAR_VAL,

    IDX_MASK_LEAK_ENABLE_CHAR,
    IDX_MASK_LEAK_ENABLE_CHAR_VAL,

    IDX_NIGHT_SHADE_ENABLE_CHAR,
    IDX_NIGHT_SHADE_ENABLE_CHAR_VAL,

    IDX_BRIGHTNESS_LEVEL_CHAR,
    IDX_BRIGHTNESS_LEVEL_CHAR_VAL,

    IDX_STANDBY_ENABLE_CHAR,
    IDX_STANDBY_ENABLE_CHAR_VAL,

    IDX_AMBIENT_SOUND_ENABLE_CHAR,
    IDX_AMBIENT_SOUND_ENABLE_CHAR_VAL,

    IDX_THERAPY_PRESSURE_CHAR,
    IDX_THERAPY_PRESSURE_CHAR_VAL,

    IDX_MANUAL_ENABLE_CHAR,
    IDX_MANUAL_ENABLE_CHAR_VAL,

    IDX_APAP_MIN_CHAR,
    IDX_APAP_MIN_CHAR_VAL,

    IDX_APAP_MAX_CHAR,
    IDX_APAP_MAX_CHAR_VAL,

    IDX_ALERT_CHAR,
    IDX_ALERT_CHAR_VAL,

    IDX_MAINTENANCE_CHAR,
    IDX_MAINTENANCE_CHAR_VAL,

    IDX_FILTER_CHANGE_CHAR,
    IDX_FILTER_CHANGE_CHAR_VAL,

    IDX_BLOWER_HOUR_METER_CHAR,
    IDX_BLOWER_HOUR_METER_CHAR_VAL,

    IDX_BLOWER_HOUR_METER_RST_CHAR,
    IDX_BLOWER_HOUR_METER_RST_CHAR_VAL,

    IDX_SOS_BUTTON_CHAR,
    IDX_SOS_BUTTON_CHAR_VAL,

    IDX_REQ_LIST_CHAR,
    IDX_REQ_LIST_CHAR_VAL,

    IDX_REQ_CLEAR_LIST_CHAR,
    IDX_REQ_CLEAR_LIST_CHAR_VAL,

    IDX_LIST_EMPTY_CHAR,
    IDX_LIST_EMPTY_CHAR_VAL,

    IDX_DATA_CHAR,
    IDX_DATA_CHAR_VAL,

    DB_SIZE,
};

static const uint8_t charValueIdx[DEV_INFO_CHAR_SIZE] = {
    IDX_HUMIDIFIER_ENABLE_CHAR_VAL,
    IDX_HUMIDIFIER_LEVEL_CHAR_VAL,
    IDX_RAMP_TIME_CHAR_VAL,
    IDX_OUT_RELIEF_ENABLE_CHAR_VAL,
    IDX_OUT_RELIEF_LEVEL_CHAR_VAL,
    IDX_FLOW_RESPONSE_ENABLE_CHAR_VAL,
    IDX_MASK_LEAK_ENABLE_CHAR_VAL,
    IDX_NIGHT_SHADE_ENABLE_CHAR_VAL,
    IDX_BRIGHTNESS_LEVEL_CHAR_VAL,
    IDX_STANDBY_ENABLE_CHAR_VAL,
    IDX_AMBIENT_SOUND_ENABLE_CHAR_VAL,
    IDX_THERAPY_PRESSURE_CHAR_VAL,
    IDX_MANUAL_ENABLE_CHAR_VAL,
    IDX_APAP_MIN_CHAR_VAL,
    IDX_APAP_MAX_CHAR_VAL,
    IDX_ALERT_CHAR_VAL,
    IDX_MAINTENANCE_CHAR_VAL,
    IDX_FILTER_CHANGE_CHAR_VAL,
    IDX_BLOWER_HOUR_METER_CHAR_VAL,
    IDX_BLOWER_HOUR_METER_RST_CHAR_VAL,
    IDX_SOS_BUTTON_CHAR_VAL,
    IDX_REQ_LIST_CHAR_VAL,
    IDX_REQ_CLEAR_LIST_CHAR_VAL,
    IDX_LIST_EMPTY_CHAR_VAL,
    IDX_DATA_CHAR_VAL,
};

static const char *indexNames[DB_SIZE] = {"DEVICE_CONFIG_SRV",
                                          "HUMIDIFIER_ENABLE_CHAR",
                                          "HUMIDIFIER_ENABLE_CHAR_VAL",
                                          "HUMIDIFIER_LEVEL_CHAR",
                                          "HUMIDIFIER_LEVEL_CHAR_VAL",
                                          "RAMP_TIME_CHAR",
                                          "RAMP_TIME_CHAR_VAL",
                                          "OUT_RELIEF_ENABLE_CHAR",
                                          "OUT_RELIEF_ENABLE_CHAR_VAL",
                                          "OUT_RELIEF_LEVEL_CHAR",
                                          "OUT_RELIEF_LEVEL_CHAR_VAL",
                                          "FLOW_RESPONSE_ENABLE_CHAR",
                                          "FLOW_RESPONSE_ENABLE_CHAR_VAL",
                                          "MASK_LEAK_ENABLE_CHAR",
                                          "MASK_LEAK_ENABLE_CHAR_VAL",
                                          "NIGHT_SHADE_ENABLE_CHAR",
                                          "NIGHT_SHADE_ENABLE_CHAR_VAL",
                                          "BRIGHTNESS_LEVEL_CHAR",
                                          "BRIGHTNESS_LEVEL_CHAR_VAL",
                                          "STANDBY_ENABLE_CHAR",
                                          "STANDBY_ENABLE_CHAR_VAL",
                                          "AMBIENT_SOUND_ENABLE_CHAR",
                                          "AMBIENT_SOUND_ENABLE_CHAR_VAL",
                                          "THERAPY_PRESSURE_CHAR",
                                          "THERAPY_PRESSURE_CHAR_VAL",
                                          "MANUAL_ENABLE_CHAR",
                                          "MANUAL_ENABLE_CHAR_VAL",
                                          "APAP_MIN_CHAR",
                                          "APAP_MIN_CHAR_VAL",
                                          "APAP_MAX_CHAR",
                                          "APAP_MAX_CHAR_VAL",
                                          "ALERT_CHAR",
                                          "ALERT_CHAR_VAL",
                                          "MAINTENANCE_CHAR",
                                          "MAINTENANCE_CHAR_VAL",
                                          "FILTER_CHANGE_CHAR",
                                          "FILTER_CHANGE_CHAR_VAL",
                                          "BLOWER_HOUR_METER_CHAR",
                                          "BLOWER_HOUR_METER_CHAR_VAL",
                                          "BLOWER_HOUR_METER_RST_CHAR",
                                          "BLOWER_HOUR_METER_RST_CHAR_VAL",
                                          "SOS_BUTTON_CHAR",
                                          "SOS_BUTTON_CHAR_VAL",
                                          "REQ_LIST_CHAR",
                                          "REQ_LIST_CHAR_VAL",
                                          "REQ_CLEAR_LIST_CHAR",
                                          "REQ_CLEAR_LIST_CHAR_VAL",
                                          "LIST_EMPTY_CHAR",
                                          "LIST_EMPTY_CHAR_VAL",
                                          "DATA_CHAR",
                                          "DATA_CHAR_VAL"};

// clang-format off
#define DEVICE_CONFIG_SRV_UUID              {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x00, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define HUMIDIFIER_ENABLE_CHAR_UUID         {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x01, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define HUMIDIFIER_LEVEL_CHAR_UUID          {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x02, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
// #define RAMP_ENABLE_CHAR_UUID               {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x03, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define RAMP_TIME_CHAR_UUID                 {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x04, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define OUT_RELIEF_ENABLE_CHAR_UUID         {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x05, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define OUT_RELIEF_LEVEL_CHAR_UUID          {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x06, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define FLOW_RESPONSE_ENABLE_CHAR_UUID      {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x07, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define MASK_LEAK_ENABLE_CHAR_UUID          {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x08, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define NIGHT_SHADE_ENABLE_CHAR_UUID        {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x09, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define BRIGHTNESS_LEVEL_CHAR_UUID          {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x0A, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define STANDBY_ENABLE_CHAR_UUID            {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x0B, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define AMBIENT_SOUND_ENABLE_CHAR_UUID      {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x0C, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define THERAPY_PRESSURE_CHAR_UUID          {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x0D, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define MANUAL_ENABLE_CHAR_UUID             {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x0E, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define APAP_MIN_CHAR_UUID                  {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x0F, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define APAP_MAX_CHAR_UUID                  {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x10, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define ALERT_CHAR_UUID                     {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x11, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define MAINTENANCE_CHAR_UUID               {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x12, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define FILTER_CHANGE_CHAR_UUID             {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x13, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define BLOWER_HOUR_METER_CHAR_UUID         {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x14, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define BLOWER_HOUR_METER_RESET_CHAR_UUID   {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x15, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define SOS_BUTTON_CHAR_UUID                {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x16, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define REQ_LIST_CHAR_UUID                  {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x17, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define REQ_CLEAR_LIST_CHAR_UUID            {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x18, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define LIST_EMPTY_CHAR_UUID                {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x19, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}
#define DATA_CHAR_UUID                      {0x20, 0xd9, 0x1b, 0x77, 0xca, 0x80, 0x49, 0x76, 0x42, 0x1A, 0x81, 0xde, 0xba, 0xff, 0x80, 0x45}

static const uint8_t deviceConfigSrvUuid[]          = DEVICE_CONFIG_SRV_UUID;
static const uint8_t humidifierEnableCharUuid[]     = HUMIDIFIER_ENABLE_CHAR_UUID;
static const uint8_t humidifierLevelCharUuid[]      = HUMIDIFIER_LEVEL_CHAR_UUID;
// static const uint8_t rampEnableCharUuid[]           = RAMP_ENABLE_CHAR_UUID;
static const uint8_t rampTimeCharUuid[]             = RAMP_TIME_CHAR_UUID;
static const uint8_t outReliefEnableCharUuid[]      = OUT_RELIEF_ENABLE_CHAR_UUID;
static const uint8_t outReliefLevelCharUuid[]       = OUT_RELIEF_LEVEL_CHAR_UUID;
static const uint8_t flowResponseEnableCharUuid[]   = FLOW_RESPONSE_ENABLE_CHAR_UUID;
static const uint8_t maskLeakEnableCharUuid[]       = MASK_LEAK_ENABLE_CHAR_UUID;
static const uint8_t nightShadeEnableCharUuid[]     = NIGHT_SHADE_ENABLE_CHAR_UUID;
static const uint8_t brightnessLevelCharUuid[]      = BRIGHTNESS_LEVEL_CHAR_UUID;
static const uint8_t standbyEnableCharUuid[]        = STANDBY_ENABLE_CHAR_UUID;
static const uint8_t ambientSoundEnableCharUuid[]   = AMBIENT_SOUND_ENABLE_CHAR_UUID;
static const uint8_t therapyPressureCharUuid[]      = THERAPY_PRESSURE_CHAR_UUID;
static const uint8_t manualEnableCharUuid[]         = MANUAL_ENABLE_CHAR_UUID;
static const uint8_t apapMinCharUuid[]              = APAP_MIN_CHAR_UUID;
static const uint8_t apapMaxCharUuid[]              = APAP_MAX_CHAR_UUID;
static const uint8_t alertCharUuid[]                = ALERT_CHAR_UUID;
static const uint8_t maintenanceCharUuid[]          = MAINTENANCE_CHAR_UUID;
static const uint8_t filterChangeCharUuid[]         = FILTER_CHANGE_CHAR_UUID;
static const uint8_t blowerHourMeterCharUuid[]      = BLOWER_HOUR_METER_CHAR_UUID;
static const uint8_t blowerHourMeterResetCharUuid[] = BLOWER_HOUR_METER_RESET_CHAR_UUID;
static const uint8_t sosButtonCharUuid[]            = SOS_BUTTON_CHAR_UUID;
static const uint8_t reqListCharUuid[]              = REQ_LIST_CHAR_UUID;
static const uint8_t reqClearListCharUuid[]         = REQ_CLEAR_LIST_CHAR_UUID;
static const uint8_t listEmptyCharUuid[]            = LIST_EMPTY_CHAR_UUID;
static const uint8_t dataCharUuid[]                 = DATA_CHAR_UUID;
// clang-format on

// Characteristic Values
#define ERROR_REGISTER_DATA_SIZE 20
static const uint8_t humidifierEnable;
static const uint8_t humidifierLevel;
// static const uint8_t rampEnable;
static const uint8_t rampTime;
static const uint8_t outReliefEnable;
static const uint8_t outReliefLevel;
static const uint8_t flowResponseEnable;
static const uint8_t leakMaskEnable;
static const uint8_t nightShadeEnable;
static const uint8_t brightnessLevel;
static const uint8_t standbyEnable;
static const uint8_t ambientSoundEnable;
static const float therapyPressure;
static const uint8_t manualEnable;
static const float apapMin;
static const float apapMax;
static const uint8_t alert;
static const uint8_t maintenance;
static const uint8_t filterChange;
static const uint8_t blowerHourMeter;
static const uint8_t blowerHourMeterReset;
static const uint8_t sosButton;
static const uint8_t reqList;
static const uint8_t reqClearList;
static const uint8_t listEmpty;

static const uint8_t data[ERROR_REGISTER_DATA_SIZE];

static uint16_t serviceHandleTable[DB_SIZE];

static uint16_t connectionID;
static uint16_t interfaceType;
static uint8_t isConnected;

// clang-format off
static const esp_gatts_attr_db_t deviceConfigAttributeDatabase[DB_SIZE] = {
    // Service declaration
    [IDX_DEVICE_CONFIG_SRV] =  
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
         {
             .uuid_length = ESP_UUID_LEN_16,
             .uuid_p      = (uint8_t *)&primaryServiceUuid,
             .perm        = ESP_GATT_PERM_READ,
             .max_length  = sizeof(deviceConfigSrvUuid),
             .length      = sizeof(deviceConfigSrvUuid),
             .value       = (uint8_t *)&deviceConfigSrvUuid,
         }},
    // Characteristic declaration
    [IDX_HUMIDIFIER_ENABLE_CHAR] =  
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
    [IDX_HUMIDIFIER_ENABLE_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&humidifierEnableCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(humidifierEnable),
            .length      = sizeof(humidifierEnable),
            .value       = (uint8_t *)&humidifierEnable,
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
             .value       = (uint8_t *)&characteristicPropReadWriteNotify,
         }},
    // Characteristic value
    [IDX_HUMIDIFIER_LEVEL_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&humidifierLevelCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(humidifierLevel),
            .length      = sizeof(humidifierLevel),
            .value       = (uint8_t *)&humidifierLevel,
        }},
    // Characteristic declaration
    [IDX_RAMP_TIME_CHAR] =  
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
    [IDX_RAMP_TIME_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&rampTimeCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(rampTime),
            .length      = sizeof(rampTime),
            .value       = (uint8_t *)&rampTime,
        }},
    // Characteristic declaration
    [IDX_OUT_RELIEF_ENABLE_CHAR] =  
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
    [IDX_OUT_RELIEF_ENABLE_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&outReliefEnableCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(outReliefEnable),
            .length      = sizeof(outReliefEnable),
            .value       = (uint8_t *)&outReliefEnable,
        }},
    // Characteristic declaration
    [IDX_OUT_RELIEF_LEVEL_CHAR] =  
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
    [IDX_OUT_RELIEF_LEVEL_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&outReliefLevelCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(outReliefLevel),
            .length      = sizeof(outReliefLevel),
            .value       = (uint8_t *)&outReliefLevel,
        }},
    // Characteristic declaration
    [IDX_FLOW_RESPONSE_ENABLE_CHAR] =  
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
    [IDX_FLOW_RESPONSE_ENABLE_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&flowResponseEnableCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(flowResponseEnable),
            .length      = sizeof(flowResponseEnable),
            .value       = (uint8_t *)&flowResponseEnable,
        }},
    // Characteristic declaration
    [IDX_MASK_LEAK_ENABLE_CHAR] =  
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
    [IDX_MASK_LEAK_ENABLE_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&maskLeakEnableCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(leakMaskEnable),
            .length      = sizeof(leakMaskEnable),
            .value       = (uint8_t *)&leakMaskEnable,
        }},
    // Characteristic declaration
    [IDX_NIGHT_SHADE_ENABLE_CHAR] =  
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
    [IDX_NIGHT_SHADE_ENABLE_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&nightShadeEnableCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(nightShadeEnable),
            .length      = sizeof(nightShadeEnable),
            .value       = (uint8_t *)&nightShadeEnable,
        }},
    // Characteristic declaration
    [IDX_BRIGHTNESS_LEVEL_CHAR] =  
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
    [IDX_BRIGHTNESS_LEVEL_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&brightnessLevelCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(brightnessLevel),
            .length      = sizeof(brightnessLevel),
            .value       = (uint8_t *)&brightnessLevel,
        }},
    // Characteristic declaration
    [IDX_STANDBY_ENABLE_CHAR] =  
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
    [IDX_STANDBY_ENABLE_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&standbyEnableCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(standbyEnable),
            .length      = sizeof(standbyEnable),
            .value       = (uint8_t *)&standbyEnable,
        }},
    // Characteristic declaration
    [IDX_AMBIENT_SOUND_ENABLE_CHAR] =  
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
    [IDX_AMBIENT_SOUND_ENABLE_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&ambientSoundEnableCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(ambientSoundEnable),
            .length      = sizeof(ambientSoundEnable),
            .value       = (uint8_t *)&ambientSoundEnable,
        }},
    // Characteristic declaration
    [IDX_THERAPY_PRESSURE_CHAR] =  
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
    [IDX_THERAPY_PRESSURE_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&therapyPressureCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(therapyPressure),
            .length      = sizeof(therapyPressure),
            .value       = (uint8_t *)&therapyPressure,
        }},
    // Characteristic declaration
    [IDX_MANUAL_ENABLE_CHAR] =  
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
    [IDX_MANUAL_ENABLE_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&manualEnableCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(manualEnable),
            .length      = sizeof(manualEnable),
            .value       = (uint8_t *)&manualEnable,
        }},
    // Characteristic declaration
    [IDX_APAP_MIN_CHAR] =  
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
    [IDX_APAP_MIN_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&apapMinCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(apapMin),
            .length      = sizeof(apapMin),
            .value       = (uint8_t *)&apapMin,
        }},
    // Characteristic declaration
    [IDX_APAP_MAX_CHAR] =  
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
    [IDX_APAP_MAX_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&apapMaxCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(apapMax),
            .length      = sizeof(apapMax),
            .value       = (uint8_t *)&apapMax,
        }},
    // Characteristic declaration
    [IDX_ALERT_CHAR] =  
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
    [IDX_ALERT_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&alertCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(alert),
            .length      = sizeof(alert),
            .value       = (uint8_t *)&alert,
        }},
    // Characteristic declaration
    [IDX_MAINTENANCE_CHAR] =  
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
    [IDX_MAINTENANCE_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&maintenanceCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(maintenance),
            .length      = sizeof(maintenance),
            .value       = (uint8_t *)&maintenance,
        }},
    // Characteristic declaration
    [IDX_FILTER_CHANGE_CHAR] =  
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
    [IDX_FILTER_CHANGE_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&filterChangeCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(filterChange),
            .length      = sizeof(filterChange),
            .value       = (uint8_t *)&filterChange,
        }},
    // Characteristic declaration
    [IDX_BLOWER_HOUR_METER_CHAR] =  
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
    [IDX_BLOWER_HOUR_METER_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&blowerHourMeterCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(blowerHourMeter),
            .length      = sizeof(blowerHourMeter),
            .value       = (uint8_t *)&blowerHourMeter,
        }},
    // Characteristic declaration
    [IDX_BLOWER_HOUR_METER_RST_CHAR] =  
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
    [IDX_BLOWER_HOUR_METER_RST_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&blowerHourMeterResetCharUuid,
            .perm        = ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(blowerHourMeterReset),
            .length      = sizeof(blowerHourMeterReset),
            .value       = (uint8_t *)&blowerHourMeterReset,
        }},
    // Characteristic declaration
    [IDX_SOS_BUTTON_CHAR] =  
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
    [IDX_SOS_BUTTON_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&sosButtonCharUuid,
            .perm        = ESP_GATT_PERM_READ,
            .max_length  = sizeof(sosButton),
            .length      = sizeof(sosButton),
            .value       = (uint8_t *)&sosButton,
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
    [IDX_REQ_CLEAR_LIST_CHAR] =  
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
    [IDX_REQ_CLEAR_LIST_CHAR_VAL] = 
    (esp_gatts_attr_db_t) {.attr_control.auto_rsp = ESP_GATT_AUTO_RSP,
     .att_desc =
        {
            .uuid_length = ESP_UUID_LEN_128,
            .uuid_p      = (uint8_t *)&reqClearListCharUuid,
            .perm        = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length  = sizeof(reqClearList),
            .length      = sizeof(reqClearList),
            .value       = (uint8_t *)&reqClearList,
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

void DeviceConfig_SRV_EventHandler(esp_gatts_cb_event_t event,
                                   esp_gatt_if_t gatts_if,
                                   esp_ble_gatts_cb_param_t *param) {
    esp_ble_gatts_cb_param_t *p_data = (esp_ble_gatts_cb_param_t *)param;

    switch (event) {
        case ESP_GATTS_REG_EVT:
            esp_ble_gatts_create_attr_tab(deviceConfigAttributeDatabase,
                                          gatts_if,
                                          DB_SIZE,
                                          0);
            break;

        case ESP_GATTS_CONNECT_EVT:
            {
                connectionID  = p_data->connect.conn_id;
                interfaceType = gatts_if;
                isConnected   = 1;
                break;
            }

        case ESP_GATTS_DISCONNECT_EVT:
            {
                connectionID  = 0;
                interfaceType = ESP_GATT_IF_NONE;
                isConnected   = 0;
                break;
            }

        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            {
                if ((param->add_attr_tab.status == ESP_GATT_OK)
                    && (param->add_attr_tab.num_handle == DB_SIZE)) {
                    memcpy(serviceHandleTable,
                           param->add_attr_tab.handles,
                           sizeof(serviceHandleTable));
                    esp_ble_gatts_start_service(
                        serviceHandleTable[IDX_DEVICE_CONFIG_SRV]);
                } else {
                    ESP_LOGE(__FUNCTION__, "CREATE ATTRIBUTE TABLE ERROR!");
                }
                break;
            }

        case ESP_GATTS_WRITE_EVT:
            {
                if (p_data->write.len < 255) {
                    uint16_t j;
                    for (j = 0; j < DB_SIZE; ++j) {
                        if (p_data->write.handle == serviceHandleTable[j])
                            break;
                    }

                    ESP_LOGI(__FUNCTION__,
                             "ESP_GATTS_WRITE_EVT: %s",
                             indexNames[j]);

                    // ESP_LOG_BUFFER_HEX("Bluetooth Request",
                    //                    p_data->write.value,
                    //                    p_data->write.len);

                    uint8_t charId = 0;
                    for (charId = 0; charId < GET_SIZE_ARRAY(charValueIdx);
                         ++charId) {
                        if (p_data->write.handle
                            == serviceHandleTable[charValueIdx[charId]]) {
                            MainUc_SRV_AppWriteEvtCB(
                                (uint8_t)DEVICE_CONFIG_PROFILE_APP,
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
                for (charId = 0; charId < GET_SIZE_ARRAY(charValueIdx);
                     ++charId) {
                    if (p_data->read.handle
                        == serviceHandleTable[charValueIdx[charId]]) {
                        DeviceConfig_SRV_GetCharValue(
                            (E_DeviceInfoCharacteristicsTypeDef)charId,
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

void DeviceConfig_SRV_SetCharValue(uint8_t charId,
                                   uint8_t size,
                                   const uint8_t *value) {
    if (charId >= GET_SIZE_ARRAY(charValueIdx)) {
        return;
    }

    esp_ble_gatts_set_attr_value(serviceHandleTable[charValueIdx[charId]],
                                 size,
                                 value);

    /**
     * ToDo: Retirar este if(charId < DEV_INFO_CHAR_SIZE - 2) e implementar na
     * parte do BLE as duas características que estão faltando, a saber:
     * DEV_INFO_MANUAL_ENABLE_CHAR e DEV_INFO_MASK_LEAK_ENABLE_CHAR
     */

    uint16_t deviceConfigPos = charValueIdx[charId];

    if (*deviceConfigAttributeDatabase[deviceConfigPos - 1].att_desc.value
        & ESP_GATT_CHAR_PROP_BIT_INDICATE) {
        esp_ble_gatts_send_indicate(interfaceType,
                                    connectionID,
                                    serviceHandleTable[deviceConfigPos],
                                    size,
                                    (uint8_t *)value,
                                    false);
    } else if (*deviceConfigAttributeDatabase[deviceConfigPos - 1]
                    .att_desc.value
               & ESP_GATT_CHAR_PROP_BIT_NOTIFY) {
        esp_ble_gatts_send_indicate(interfaceType,
                                    connectionID,
                                    serviceHandleTable[deviceConfigPos],
                                    size,
                                    (uint8_t *)value,
                                    true);
    }
}

void DeviceConfig_SRV_GetCharValue(E_DeviceInfoCharacteristicsTypeDef id,
                                   uint8_t size,
                                   const uint8_t *value) {
    uint16_t size16         = size;
    const uint8_t *valuePtr = value;
    esp_ble_gatts_get_attr_value(serviceHandleTable[charValueIdx[id]],
                                 &size16,
                                 &valuePtr);
}
