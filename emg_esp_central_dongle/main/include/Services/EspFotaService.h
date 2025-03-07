/******************************************************************************
 * @file         : EspFotaService.h
 * @author       : CMOS Drake.
 * @brief        : Serviço responsável por gerenciar funções e recursos para
 * atualização do firmware do ESP.
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef __ESP_FOTA_SERVICE_H__
#define __ESP_FOTA_SERVICE_H__

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

/* Includes ------------------------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "Drivers/HttpClientDriver.h"

#include "App/AppConfig.h"
#include "App/AppReturnStatus.h"
#include "esp_app_format.h"
#include "esp_crt_bundle.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include <esp_log.h>
#include <sdkconfig.h>

/* Define --------------------------------------------------------------------*/

/**
 * @brief MACRO que define o tamanho do vetor que contém a versão do firmware.
 */
#define ESP_VERSION_LENGTH 32U

/* Typedef -------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/

extern SemaphoreHandle_t xhttpClientDriverSync;

/* Public objects ------------------------------------------------------------*/
/* Public functions ----------------------------------------------------------*/

const bool SRV_EspFota_GetMutex(void);
void SRV_EspFota_GiveMutex(void);

const bool SRV_EspFota_Begin(void);

const uint32_t SRV_EspFota_GetTotalImageLength(void);
const uint32_t SRV_EspFota_GetCurrentImageLengthDownload(void);

const E_AppStatusTypeDef SRV_EspFota_GetRunningVersion(
    char *const p_runningVersion);

const E_AppStatusTypeDef SRV_EspFota_CheckVersion(
    bool *const p_isNewVersion,
    char *const p_version_available);

const E_AppStatusTypeDef SRV_EspFota_DownloadPartialImage(
    int *const p_statusCode);

const bool SRV_EspFota_IsDownloadImageCompleted(void);

const bool SRV_EspFota_Finish(void);
const bool SRV_EspFota_Abort(void);

const bool SRV_EspFota_IsRequiredValidationImage(void);
const bool SRV_EspFota_SetIValidImageStatus(const bool status);

#if defined(__cplusplus)
}
#endif /** __cplusplus */

#endif /** __ESP_FOTA_SERVICE_H__ */