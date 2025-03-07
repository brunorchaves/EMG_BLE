/******************************************************************************
 * @file         : MainUcService.h
 * @author       : CMOS Drake.
 * @brief        : Serviço responsável por gerenciar recursos de escrita/leitura
 * e notificação de dados.
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef __MAIN_UC_SRV_H__
#define __MAIN_UC_SRV_H__

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

/* Includes ------------------------------------------------------------------*/

#include <stdint.h>

#include "Services/BluetoothService.h"
#include "Services/CalibrationProfileService.h"
#include "Services/CryptoService.h"
#include "Services/DebugProfileService.h"
#include "Services/DeviceConfigProfileService.h"
#include "Services/EspFotaService.h"
#include "Services/GeneralInfoProfileService.h"
#include "Services/HttpRequestService.h"
#include "Services/TherapyProfileService.h"
#include "Services/TrackPositionProfileService.h"
#include "Services/WirelessConnectionsProfileService.h"

#include "Drivers/STMUsartDriver.h"

#include "App/AppData.h"
#include "App/AppReturnStatus.h"
#include "Utils/WeProtocolUtils.h"
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>

/* Define --------------------------------------------------------------------*/

/* Typedef -------------------------------------------------------------------*/

/**
 * @brief Tipo de função de chamada de retorno para ser utilizada por um perfil.
 */
typedef void (*profileCallbackTypeDef)(uint8_t,
                                       uint8_t len,
                                       uint8_t *const p_data);

/* Exported types ------------------------------------------------------------*/
/* Public objects ------------------------------------------------------------*/
/* Public functions ----------------------------------------------------------*/

E_AppStatusTypeDef MainUc_SRV_Init(void);
E_AppStatusTypeDef MainUc_SRV_DeInit(void);

void MainUc_SRV_Handler(void);
void MainUc_SRV_UpdateParametersTherapy(void);

E_AppStatusTypeDef MainUc_SRV_AppWriteEvtCB(uint8_t profileId,
                                            uint8_t charId,
                                            uint8_t size,
                                            const uint8_t *value);

bool MainUc_SRV_ReqAllChars(uint8_t profileId);

E_AppStatusTypeDef MainUc_SRV_GetTherapyConfiguration(
    S_TherapyParameteresConfigurationTypeDef *therapyParameters);

E_AppStatusTypeDef MainUc_SRV_SetTherapyConfiguration(
    S_TherapyParameteresConfigurationTypeDef *therapyParameters);

bool MainUc_SRV_SendEspRunningVersion(void);

bool MainUc_SRV_FirstSettingsParamDefinedAll(void);
bool MainUc_SRV_RequestTherapyDeviceParametersAll(void);

void MainUc_EspProfile_ResponseCallback(int8_t charId,
                                        uint8_t charSize,
                                        uint8_t *const parameterValue);

void MainUc_STMFotaProfile_ResponseCallback(uint8_t charId,
                                            uint8_t charSize,
                                            uint8_t *const parameterValue);

void MainUc_EspFotaProfile_ResponseCallback(uint8_t charId,
                                            uint8_t charSize,
                                            uint8_t *const parameterValue);

void MainUc_FileUploadProfile_ResponseCallback(uint8_t charId,
                                               uint8_t charSize,
                                               uint8_t *const parameterValue);

#if defined(__cplusplus)
}
#endif /** __cplusplus */

#endif /** __MAIN_UC_SRV_H__ */