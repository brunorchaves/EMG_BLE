/******************************************************************************
 * @file         : HttpRequestService.h
 * @authors      : Thales Mol Vinhal
 * @copyright    : Copyright (c) 2023 CMOS Drake
 * @brief        : Serviço para provisão de Requisições Http
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef HTTP_REQUEST_SERVICE_H
#define HTTP_REQUEST_SERVICE_H

#include "Services/BluetoothService.h"
#include "Services/DeviceConfigProfileService.h"

#ifdef __cplusplus
extern "C" {
#endif /** __cplusplus */

/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>

#include "Services/HttpFileUploadService.h"
#include "Services/NTPService.h"
#include "Services/TherapyProfileService.h"
#include "Services/TrackPositionProfileService.h"

#include "Drivers/HttpClientDriver.h"

#include "App/AppConfig.h"
#include "App/AppReturnStatus.h"
#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_http_client.h"
#include "esp_log.h"

/* Public functions ----------------------------------------------------------*/

void SRV_HttpRequest_Init(void);

bool SRV_HttpRequest_ExecuteConfigRLogin(const char *const p_secreyKey,
                                         const char *const p_id);

// bool SRV_HttpRequest_UpdateTherapyParameter(const uint64_t serialNumber,
//                                             uint8_t profileId,
//                                             uint8_t parameterID,
//                                             uint8_t value);

bool SRV_HttpRequest_ExecuteCoreLogin(void);


bool SRV_HttpRequest_SetApplicationHeader(esp_http_client_handle_t client);
bool SRV_HttpRequest_SetAuthorizationHeader(esp_http_client_handle_t client);
bool SRV_HttpRequest_SetUserTokenHeader(esp_http_client_handle_t client);
bool SRV_HttpRequest_SetLocaleHeader(esp_http_client_handle_t client);
int SRV_HttpRequest_GetTxSize(void);
int SRV_HttpRequest_GetRxSize(void);

bool SRV_HttpRequest_UpdateTherapyDeviceParameters(
    const uint64_t equipmentSerial,
    volatile S_TherapyParameteresConfigurationTypeDef *const ps_therapyBuffer,
    const uint8_t therapyLength);

bool SRV_HttpRequest_GetEquipmentParameters(
    const uint64_t serialNumber,
    S_TherapyParameteresConfigurationTypeDef *ps_therapyParameters,
    const uint8_t therapyLength);

// bool SRV_HttpRequest_GetfindOneByExactSerialNumber(const uint64_t
// serialNumber,
//                                                    char *p_equipmentID);
bool SRV_HttpRequest_UpdateDeviceTrackPosition(
    const uint64_t equipmentSerial,
    S_TrackPositionTypeDef *const ps_trackPosition);

bool SRV_HttpRequest_SendLogFile(const char *const p_fileName,
                                 const uint8_t *const p_blockData,
                                 const uint16_t blockLen);

#ifdef __cplusplus
}
#endif /** __cplusplus */

#endif /* HTTP_REQUEST_SERVICE_H */
