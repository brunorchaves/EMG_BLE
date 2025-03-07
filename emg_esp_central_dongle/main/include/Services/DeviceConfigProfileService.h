/**
 * @file DeviceConfigProfileService.h
 * @author Vitor Mendonça (vitor.mendonca@42we.tech)
 * @brief
 * @version 0.1
 * @date 23-11-2022
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef DEVICE_CONFIG_PROFILE_SRV_H_
#define DEVICE_CONFIG_PROFILE_SRV_H_

#include <esp_gatts_api.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "App/AppData.h"

extern void DeviceConfig_SRV_EventHandler(esp_gatts_cb_event_t event,
                                          esp_gatt_if_t gatts_if,
                                          esp_ble_gatts_cb_param_t *param);

extern void DeviceConfig_SRV_SetCharValue(uint8_t id,
                                          uint8_t size,
                                          const uint8_t *value);

extern void DeviceConfig_SRV_GetCharValue(E_DeviceInfoCharacteristicsTypeDef id,
                                          uint8_t size,
                                          const uint8_t *value);
#ifdef __cplusplus
} /*extern "C"*/
#endif
#endif /* DEVICE_CONFIG_PROFILE_SRV_H_ */