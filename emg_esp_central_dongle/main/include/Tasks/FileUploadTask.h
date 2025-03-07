/******************************************************************************
 * @file         : FileUploadTask.h
 * @author       : CMOS Drake.
 * @brief        : Tarefa responsável pelo gerenciamento e envio de dados de
 * logs para a nuvem.
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef __FILE_UPLOAD_TASK_H__
#define __FILE_UPLOAD_TASK_H__

/* Includes ------------------------------------------------------------------*/

#include "Services/MainUcService.h"
#include "Services/WifiProvisionService.h"

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "App/AppConfig.h"
#include "App/AppData.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include "Utils/TimerUtils.h"

/* Define --------------------------------------------------------------------*/
/* Typedef -------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Public objects ------------------------------------------------------------*/
/* Public functions ----------------------------------------------------------*/

void Task_FileUpload_Create(const uint8_t priority, void *p_parameters);
void Task_FileUpload_Destroy(void);

void Task_FileUpload_Resume(void);
void Task_FileUpload_Suspend(void);

#if defined(__cplusplus)
}
#endif /** __cplusplus */

#endif /** __FILE_UPLOAD_TASK_H__ */