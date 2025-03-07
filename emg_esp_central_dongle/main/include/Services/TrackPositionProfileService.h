/******************************************************************************
 * @file         : TrackPositionProfileService.h
 * @authors      : Thales Mol Vinhal
 * @copyright    : Copyright (c) 2024 CMOS Drake
 * @brief        : Serviço para gerenciamento de informações de localização.
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef TRACK_POSITION_PROFILE_SERVICE_H
#define TRACK_POSITION_PROFILE_SERVICE_H

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Services/BluetoothService.h"

#include "App/AppData.h"
#include <esp_log.h>

/* Typedef--------------------------------------------------------------------*/

typedef struct S_TrackPosition {
    char time[10];       // Hora da localização (GMT)
    float latitude;      // Latitude com direção
    float longitude;     // Longitude com direção
    float altitude;      // Altitude
    float imprecision;   // Medida de imprecisão (quanto menor melhor!)
    uint8_t satellites;  // Número de satélites
} S_TrackPositionTypeDef;

#include "Services/MainUcService.h"

/* Public functions ----------------------------------------------------------*/

void TrackPositionProfile_SRV_SetCharValue(uint8_t charId,
                                           uint8_t size,
                                           const uint8_t *value);

bool TrackPositionProfile_SRV_GetPendingUpload();

void TrackPositionProfile_SRV_SetPendingUpload(bool pending);

void TrackPositionProfile_SRV_GetTrackPosition(
    S_TrackPositionTypeDef *trackPosition);

#if defined(__cplusplus)
}
#endif /** __cplusplus */

#endif /* TRACK_POSITION_PROFILE_SERVICE_H */
