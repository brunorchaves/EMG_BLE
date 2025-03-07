/**
 * @file BluetoothService.h
 * @author Davi Marriel (davi.marriel@42we.tech)
 * @brief
 * @version 0.1
 * @date 19-10-2022
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef BLUETOOTH_SRV_H_
#define BLUETOOTH_SRV_H_
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

#include "App/AppData.h"

// "ESP_GATT_UUID_xxx" Attribute Types
extern const uint16_t primaryServiceUuid;
extern const uint16_t characteristicUuid;
extern const uint16_t configurationUuid;

// Characteristic Properties
extern const uint8_t characteristicPropRead;
extern const uint8_t characteristicPropWrite;
extern const uint8_t characteristicPropReadNotify;
extern const uint8_t characteristicPropReadWrite;
extern const uint8_t characteristicPropReadWriteNotify;

/**
 * @brief Characteristic write event callback to be implemented by the service
 * that will handle the characteristic actions and data.
 *
 * @param size Size of the characteristic value
 * @param value Pointer to the received value
 */
typedef void (*CharWriteEventCB)(uint16_t size,
                                 uint8_t *value,
                                 uint16_t profIdx,
                                 uint16_t charIdx);
/**
 * @brief Initializes Bluetooth
 *
 * @return RETURN_STATUS
 */
int32_t Bluetooth_SRV_Init();

int32_t Bluetooth_SRV_Deinit();

/**
 * @brief Sends data to client
 *
 * @param size Amount of data
 * @param data pointer to data
 */
int32_t Bluetooth_SRV_SendData(uint8_t size, uint8_t *data);

/**
 * @brief Gets the state of connection
 *
 * @return uint8_t
 */
uint8_t Bluetooth_SRV_ConnectionState(void);

uint8_t Bluetooth_SRV_EnabledState(void);
#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* BLUETOOTH_SRV_H_ */