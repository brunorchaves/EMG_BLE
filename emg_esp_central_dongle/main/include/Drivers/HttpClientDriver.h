/******************************************************************************
 * @file         : HttpClientDriver.h
 * @author       : CMOS Drake.
 * @brief        : Driver responsável por gerenciar o acesso à funções de baixo
 * nível da API HTTP da Espressif, com controle de acesso à regiões críticas.
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef __HTTP_CLIENT_DRIVER_H__
#define __HTTP_CLIENT_DRIVER_H__

/* Includes ------------------------------------------------------------------*/

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "App/AppConfig.h"
#include "App/AppReturnStatus.h"
#include "Utils/TimerUtils.h"
#include "esp_http_client.h"
#include "freertos/semphr.h"
#include <esp_log.h>

/* Define --------------------------------------------------------------------*/
/* Typedef -------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/

extern SemaphoreHandle_t xhttpClientDriverSync;

/* Public objects ------------------------------------------------------------*/
/* Public functions ----------------------------------------------------------*/

void DRV_HttpClient_Init(void);
bool DRV_HttpClient_IsInitializated(void);

bool DRV_HttpClient_CreateInstance(
    esp_http_client_handle_t *pps_httpClientHandler,
    esp_http_client_config_t *const ps_httpClientConfig);
bool DRV_HttpClient_DestroyInstance(
    esp_http_client_handle_t *pps_httpClientHandler);

bool DRV_HttpClient_Perform(
    esp_http_client_handle_t *const pps_httpClientHandler,
    esp_err_t *const p_errorEsp);

#if defined(__cplusplus)
}
#endif /** __cplusplus */

#endif /** __HTTP_CLIENT_DRIVER_H__ */