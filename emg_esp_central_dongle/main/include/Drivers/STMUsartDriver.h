/******************************************************************************
 * @file         : STMUsartDriver.h
 * @author       : CMOS Drake.
 * @brief        : Driver responsável por gerenciar funções para o barramento
 * da Uart que comunica com o STM32.
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef __STM_USART_DRIVER_H__
#define __STM_USART_DRIVER_H__

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

/* Includes ------------------------------------------------------------------*/

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "App/AppConfig.h"
#include "App/AppReturnStatus.h"
#include "Utils/RingBufferUtils.h"
#include "Utils/TimerUtils.h"
#include "Utils/WeProtocolUtils.h"
#include "driver/uart.h"
#include "hal/uart_types.h"
#include <driver/gpio.h>
#include <esp_log.h>
// #include "Drivers/HttpClientDriver.h"

/* Define --------------------------------------------------------------------*/

/**
 * @brief Tempo limite para receber um ACK.
 */
#define DRV_STM_USART_ACK_TIMEOUT_MS 100U

/* Typedef -------------------------------------------------------------------*/

typedef enum E_RxCommand {
    RX_ACK = 0,
    MAIN_CHAR_WRITE,
    RX_SIZE,
} E_RxCommandTypeDef;

typedef enum E_TxCommand {
    TX_ACK = 0,
    APP_CHAR_WRITE,
    REQ_ALL_CHARS,
    TX_SIZE,
} E_TxCommandTypeDef;

typedef struct S_ProcessedPayload {
    const uint8_t *p_payload;
    uint16_t payloadLen;
    E_RxCommandTypeDef cmd;
} S_ProcessedPayloadTypeDef;

typedef void (*rxCallbackTypeDef)(
    S_ProcessedPayloadTypeDef *const ps_processedPayload);

/* Exported types ------------------------------------------------------------*/
/* Public objects ------------------------------------------------------------*/
/* Public functions ----------------------------------------------------------*/

bool DRV_STMUsart_Init(void);
bool DRV_STMUsart_DeInit(void);

bool DRV_STMUsart_IsInitializated(void);

bool DRV_STMUsart_RegisterRxCallback(E_RxCommandTypeDef command,
                                     rxCallbackTypeDef rxCallback);

bool DRV_STMUsart_PutPayloadInTxQueue(uint8_t *const p_payload, uint16_t len);
void DRV_STMUsart_TransactionsHandler(void);

void DRV_STMUsart_SetLastPackageStatus(bool status);
bool DRV_STMUsart_GetLastPackageStatus(void);

bool DRV_STMUsart_GetPacketSendStatus(void);

#if defined(__cplusplus)
}
#endif /** __cplusplus */

#endif /** __STM_USART_DRIVER_H__ */