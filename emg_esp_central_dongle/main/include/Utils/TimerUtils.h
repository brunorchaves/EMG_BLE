/******************************************************************************
 * @file         : TimerUtils.h
 * @author       : CMOS Drake.
 * @brief        : Utilidade responsável por prover funções relativas ao tempo
 * do sistema operacional do microcontrolador.
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef __TIMER_UTILS_H__
#define __TIMER_UTILS_H__

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

/* Includes ------------------------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Define --------------------------------------------------------------------*/
/* Typedef -------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Public objects ------------------------------------------------------------*/
/* Public functions ----------------------------------------------------------*/

uint32_t UTILS_Timer_Now(void);
TickType_t UTILS_Timer_GetTicks(void);

uint32_t UTILS_Timer_ConvertToTicks(uint32_t timeout_ms);

uint32_t UTILS_Timer_GetElapsedTime(uint32_t timer);

bool UTILS_Timer_WaitTimeMSec(uint32_t timer, uint32_t timeout_ms);
bool UTILS_Timer_WaitTimeSec(uint32_t timer, uint16_t timeout_sec);

#if defined(__cplusplus)
}
#endif /** __cplusplus */

#endif /** __TIMER_UTILS_H__ */
