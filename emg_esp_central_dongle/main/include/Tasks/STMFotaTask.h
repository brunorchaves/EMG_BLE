/******************************************************************************
 * @file         : StmFotaTask.h
 * @author       : CMOS Drake.
 * @brief        : Tarefa responsável por gerenciar a atualização do firmware
 * do STM.
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef __STM_FOTA_TASK_H__
#define __STM_FOTA_TASK_H__

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

/* Includes ------------------------------------------------------------------*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Services/BluetoothService.h"
#include "Services/MainUcService.h"
#include "Services/NTPService.h"
#include "Services/STMFotaService.h"
#include "Services/WifiProvisionService.h"

#include "App/AppConfig.h"
#include "App/AppData.h"
#include "App/AppReturnStatus.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <esp_log.h>

/* Define --------------------------------------------------------------------*/
/* Typedef -------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Public objects ------------------------------------------------------------*/
/* Public functions ----------------------------------------------------------*/

void Task_STMFota_Create(const uint8_t priority, void *p_parameters);
void Task_STMFota_Destroy(void);

void Task_STMFota_Resume(void);
void Task_STMFota_Suspend(void);

#if defined(__cplusplus)
}
#endif /** __cplusplus */

#endif /** __STM_FOTA_TASK_H__ */