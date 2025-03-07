/**
 * @file CalibrationProfileService.h
 * @author Vitor Mendonça (vitor.mendonca@42we.tech)
 * @brief
 * @version 0.1
 * @date 16-01-2023
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef CALIBRATION_PROFILE_SERVICE_H_
#define CALIBRATION_PROFILE_SERVICE_H_

#include <esp_gatts_api.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum CalibProfileCharacteristic_E {
    CALIB_CURRENT_STATE_CHAR,
    CALIB_MOBILE_REQUEST_CHAR,
    CALIB_SENSOR_READ_LEVEL_CHAR,
    CALIB_INPUT_LEVEL_CHAR,
    // TODO (Vitor): Add self test characteristics
    CALIB_CHAR_SIZE,
} CalibProfileCharacteristic_E;

extern void CalibrationProfile_SRV_EventHandler(
    esp_gatts_cb_event_t event,
    esp_gatt_if_t gatts_if,
    esp_ble_gatts_cb_param_t *param);

extern void CalibrationProfile_SRV_SetCharValue(uint8_t id,
                                                uint8_t size,
                                                const uint8_t *value);

extern void CalibrationProfile_SRV_GetCharValue(CalibProfileCharacteristic_E id,
                                                uint8_t size,
                                                const uint8_t *value);
#ifdef __cplusplus
} /*extern "C"*/
#endif
#endif /* CALIBRATION_PROFILE_SERVICE_H_ */