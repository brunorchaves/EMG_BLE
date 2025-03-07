/******************************************************************************
 * @file         : STMFotaService.h
 * @author       : CMOS Drake.
 * @brief        : Serviço responsável por gerenciar funções e recursos para
 * download de novas imagens disponíveis para o STM.
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef __STM_FOTA_SERVICE_H__
#define __STM_FOTA_SERVICE_H__

/* Includes ------------------------------------------------------------------*/

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Drivers/HttpClientDriver.h"

#include "App/AppConfig.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_https_ota.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

/* Define --------------------------------------------------------------------*/

/**
 * @brief Quantidade de bytes que formam a versão do firmware do STM.
 */
#define SRV_STM_FOTA_VERSION_BYTES_LENGTH 4UL

/**
 * @brief MACRO a ser utilizada para designar o tamanho máximo da versão do
 * firmware do STM, em formato de cadeia de caracteres específico.
 */
#define SRV_STM_FW_VERSION_MAX_LENGTH 18U

/**
 * @brief Tamanho máximo do vetor de download parcial da imagem do STM.
 */
#define SRV_STM_FOTA_DOWNLOAD_BUFFER_LENGTH 2048U

/* Typedef -------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Public objects ------------------------------------------------------------*/

// extern const char drive_google_com_pem_start[] asm(
//     "_binary_drive_google_com_pem_start");
// extern const char drive_google_com_pem_end[] asm(
//     "_binary_drive_google_com_pem_end");

/* Public functions ----------------------------------------------------------*/

bool SRV_STMFota_Init(void);
bool SRV_STMFota_DeInit(void);

bool SRV_STMFota_GetVersionAvailable(uint8_t *const p_firmwareVersion,
                                     const uint16_t firmwareVersionLen,
                                     bool *const p_magicCodeValid);

bool SRV_STMFota_GetTotalImageLength(uint32_t *const p_totalImageLen);

bool SRV_STMFota_StreamDownload(uint8_t *const p_buffer,
                                const uint32_t offsetImage,
                                const uint16_t bytesToRead);

bool SRV_STMFota_CheckFirmwareVersion(const char *const p_versionAvailable,
                                     const char *const p_versionRunning);

bool SRV_STMFota_GetFirmwareVersionFormatted(uint8_t *const p_fwVersionBuffer,
                                             char *const p_fwVersionFormmated);

#if defined(__cplusplus)
}
#endif /** __cplusplus */

#endif /** __STM_FOTA_SERVICE_H__ */