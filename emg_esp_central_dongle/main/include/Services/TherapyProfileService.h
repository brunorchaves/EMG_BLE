/**
 * @file TherapyProfileService.h
 * @author Vitor Mendonça (vitor.mendonca@42we.tech)
 * @brief
 * @version 0.1
 * @date 24-11-2022
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef THERAPY_PROFILE_SRV_H_
    #define THERAPY_PROFILE_SRV_H_

    #include <esp_gatts_api.h>

    #ifdef __cplusplus
extern "C" {
    #endif

typedef enum TherapyCharacteristic_E {
    THERAPY_CURRENT_STATE_CHAR,
    THERAPY_MOBLIE_REQ_STATE_CHAR,
    THERAPY_LEAKAGE_LVL_CHAR,
    THERAPY_HUMIDIFIER_LVL_CHAR,
    THERAPY_RAMP_TIME_LEFT_CHAR,
    THERAPY_REQ_LAST_NIGHT_LIST_CHAR,
    THERAPY_LAST_NIGHT_EMPTY_CHAR,
    THERAPY_REQ_LAST_NIGHT_ITEM_CHAR,
    THERAPY_DATA_CHAR,
    THERAPY_CHAR_SIZE
} TherapyCharacteristic_E;

extern void TherapyProfile_SRV_EventHandler(esp_gatts_cb_event_t event,
                                            esp_gatt_if_t gatts_if,
                                            esp_ble_gatts_cb_param_t *param);

extern void TherapyProfile_SRV_SetCharValue(uint8_t id,
                                            uint8_t size,
                                            const uint8_t *value);

extern void TherapyProfile_SRV_GetCharValue(TherapyCharacteristic_E id,
                                            uint8_t size,
                                            const uint8_t *value);
    #ifdef __cplusplus
} /*extern "C"*/
    #endif
#endif

/* THERAPY_PROFILE_SRV_H_ */