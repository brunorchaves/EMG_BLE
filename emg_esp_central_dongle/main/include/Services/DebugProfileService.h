/******************************************************************************
 * @file         : DebugProfileService.h
 * @authors      : Bruno Ribeiro Chaves
 * @copyright    : Copyright (c) 2024 CMOS Drake
 * @brief        : Serviço para Upload de Arquivos
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef __DEBUG_PROFILE_SERVICE_H__
#define __DEBUG_PROFILE_SERVICE_H__

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

/* Includes ------------------------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "Services/BluetoothService.h"
#include "Services/MainUcService.h"

#include "App/AppData.h"
#include "string.h"
#include <esp_log.h>

/* Define --------------------------------------------------------------------*/
/* Typedef -------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Public functions ----------------------------------------------------------*/

void DebugProfile_SRV_SetCharValue(uint8_t charId,
                                  uint8_t size,
                                  const uint8_t *p_value);

void DebugProfile_SRV_PrintValues(void);

#if defined(__cplusplus)
}
#endif /** __cplusplus */

#endif /* __DEBUG_PROFILE_SERVICE_H__ */
