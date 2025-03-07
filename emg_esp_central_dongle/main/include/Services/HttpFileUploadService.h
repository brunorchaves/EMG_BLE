/******************************************************************************
 * @file         : EspFotaService.h
 * @author       : CMOS Drake.
 * @brief        : Serviço responsável por gerenciar funções e recursos para
 * atualização do firmware do ESP.
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef __HTTP_FILE_UPLOAD_SERVICE_H__
#define __HTTP_FILE_UPLOAD_SERVICE_H__

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

/* Includes ------------------------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "Services/HttpRequestService.h"

#include "Drivers/HttpClientDriver.h"

#include "App/AppConfig.h"
#include "Utils/TimerUtils.h"
#include "esp_app_desc.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <cJSON.h>

/* Define --------------------------------------------------------------------*/
/* Typedef -------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Public objects ------------------------------------------------------------*/
/* Public functions ----------------------------------------------------------*/

bool SRV_HttpFileUpload_Send(esp_http_client_handle_t client,
                             const char *const p_fileName,
                             const uint8_t *const p_blockData,
                             const uint16_t blockLen);

#if defined(__cplusplus)
}
#endif /** __cplusplus */

#endif /** __HTTP_FILE_UPLOAD_SERVICE_H__ */