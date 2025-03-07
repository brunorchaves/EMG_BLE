#ifndef MAIN_TASK_H
#define MAIN_TASK_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Services/BluetoothService.h"
#include "Services/CryptoService.h"
#include "Services/HttpRequestService.h"
#include "Services/MainUcService.h"
#include "Services/NTPService.h"
#include "Services/TrackPositionProfileService.h"
#include "Services/WifiProvisionService.h"

#include "App/AppConfig.h"
#include "App/AppData.h"
#include "App/AppReturnStatus.h"
#include "Tasks/EspFotaTask.h"
#include "Tasks/FileUploadTask.h"
#include "Tasks/STMFotaTask.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/task.h"
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

void Task_Main_Create(const uint8_t priority, void *const p_parameters);
void Task_Main_Destroy(void);

void Task_Main_Resume(void);
void Task_Main_Suspend(void);

#if defined(__cplusplus)
}
#endif /** __cplusplus */

#endif /** MAIN_TASK_H */