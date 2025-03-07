/******************************************************************************
 * @file         : EspFotaTask.h
 * @author       : CMOS Drake.
 * @brief        : Tarefa responsável por gerenciar a atualização do firmware
 * do ESP.
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef __ESP_FOTA_TASK_H__
#define __ESP_FOTA_TASK_H__

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

/* Includes ------------------------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

#include "Services/EspFotaService.h"
#include "Services/MainUcService.h"
#include "Services/WifiProvisionService.h"

#include "App/AppConfig.h"
#include "App/AppData.h"
#include "App/AppReturnStatus.h"
#include "Tasks/FileUploadTask.h"
#include "Tasks/MainTask.h"
#include "Tasks/STMFotaTask.h"
#include "freertos/task.h"
#include <esp_log.h>
#include <esp_system.h>

/* Define --------------------------------------------------------------------*/
/* Typedef -------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Public objects ------------------------------------------------------------*/
/* Public functions ----------------------------------------------------------*/

void Task_EspFota_Create(const uint8_t priority, void *p_parameters);
void Task_EspFota_Destroy(void);

void Task_EspFota_Resume(void);
void Task_EspFota_Supend(void);

#if defined(__cplusplus)
}
#endif /** __cplusplus */

#endif /** __ESP_FOTA_TASK_H__ */