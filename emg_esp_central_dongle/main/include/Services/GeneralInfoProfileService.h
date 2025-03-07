/**
 * @file GeneralInfoProfileService.h
 * @author Vitor Mendonça (vitor.mendonca@42we.tech)
 * @brief
 * @version 0.1
 * @date 23-11-2022
 *
 * @copyright Copyright (c) 2022"
 *
 */

#ifndef GENERAL_INFO_PROFILE_SRV_H_
#define GENERAL_INFO_PROFILE_SRV_H_

#include <esp_gatts_api.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum GeneralInfoCharacteristic_E {
    GEN_NAME_CHAR,
    GEN_MAIN_FW_VERSION_CHAR,
    GEN_BLE_FW_VERSION_CHAR,
    GEN_PROTOCOL_VERSION_CHAR,
    GEN_SERIAL_NUMBER_CHAR,
    GEN_DEVICE_CODE_CHAR,
    GEN_FREE_STORAGE_CHAR,
    GEN_TOTAL_STORAGE_CHAR,
    GEN_BATTERY_LEVEL_CHAR,

    GEN_INFO_CHAR_SIZE,
} GeneralInfoCharacteristic_E;

// clang-format off
#define GENERAL_INFO_SRV_UUID      {0x06, 0xfd, 0x9f, 0x98, 0xb8, 0x1c, 0x45, 0x9a, 0x42, 0x00, 0x18, 0x3b, 0xe9, 0x6e, 0x4c, 0x0c}
// clang-format on

extern const uint8_t generalInfoSrvUuid[ESP_UUID_LEN_128];

extern void GeneralProfile_SRV_EventHandler(esp_gatts_cb_event_t event,
                                            esp_gatt_if_t gatts_if,
                                            esp_ble_gatts_cb_param_t *param);

extern void GeneralInfoProfile_SRV_SetCharValue(uint8_t id,
                                                uint8_t size,
                                                const uint8_t *value);
extern void GeneralInfoProfile_SRV_GetCharValue(GeneralInfoCharacteristic_E id,
                                                uint8_t size,
                                                const uint8_t *value);
#ifdef __cplusplus
} /*extern "C"*/
#endif
#endif /* GENERAL_INFO_PROFILE_SRV_H_ */