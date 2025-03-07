/******************************************************************************
 * @file         : WifiProvisionService.h
 * @authors      : Thales Mol Vinhal
 * @copyright    : Copyright (c) 2023
 * @brief        : Serviço para provisão de credenciais WiFi
 * @attention    : Baseado no exemplo WiFi Provision Manager da Espressif,
 * disponível no esp-idf, ou através do link abaixo:
 * https://github.com/espressif/esp-idf/tree/master/examples/provisioning/wifi_prov_mgr
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef WIFI_PROVISION_SERVICE_H
#define WIFI_PROVISION_SERVICE_H

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Services/BluetoothService.h"
#include "Services/MainUcService.h"

#include "App/AppConfig.h"
#include "App/AppData.h"
#include "qrcode.h"
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_softap.h>

/* Public functions ----------------------------------------------------------*/

void SRV_WifiProvision_Init(void *p_parameter);

const bool SRV_WifiProvision_IsConnected(void);
void SRV_WifiProvision_WaitingConnect(void);

void SRV_WifiProvision_WifiDisconnectAndForgetNetwork();

uint8_t *const SRV_WifiProvision_GetQrCode();

#if defined(__cplusplus)
}
#endif /** __cplusplus */

#endif /* WIFI_PROVISION_SERVICE_H */
