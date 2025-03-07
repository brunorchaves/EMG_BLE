/******************************************************************************
 * @file         : RingBufferUtils.h
 * @author       : CMOS Drake.
 * @brief        : Utilidade responsável pelo gerenciamento de vetores
 * circulares.
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef __RING_BUFFER_UTILS_H__
#define __RING_BUFFER_UTILS_H__

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

/* Includes ------------------------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "App/AppConfig.h"
#include "App/AppReturnStatus.h"
#include "freertos/FreeRTOS.h"

/* Define --------------------------------------------------------------------*/
/* Typedef -------------------------------------------------------------------*/

struct S_RingBuffer;
typedef struct S_RingBuffer S_RingBufferTypeDef;

/* Exported types ------------------------------------------------------------*/
/* Public objects ------------------------------------------------------------*/
/* Public functions ----------------------------------------------------------*/

E_AppStatusTypeDef UTILS_RingBuffer_Create(
    S_RingBufferTypeDef **const pps_ringBuffer,
    void *const p_vector,
    const uint16_t maxQuantity,
    const uint8_t sizeOfItem);

E_AppStatusTypeDef UTILS_RingBuffer_Destroy(
    S_RingBufferTypeDef **const pps_ringBuffer);

E_AppStatusTypeDef UTILS_RingBuffer_GetStatusVolume(
    S_RingBufferTypeDef *const ps_ringBuffer);

E_AppStatusTypeDef UTILS_RingBuffer_GetSpaceAvailable(
    S_RingBufferTypeDef *const ps_ringBuffer,
    uint16_t *const p_countItemsAvailable);

E_AppStatusTypeDef UTILS_RingBuffer_Push(
    S_RingBufferTypeDef *const ps_ringBuffer,
    void *const p_data);

E_AppStatusTypeDef UTILS_RingBuffer_Pop(
    S_RingBufferTypeDef *const ps_ringBuffer,
    void *const p_data);

E_AppStatusTypeDef UTILS_RingBuffer_Peek(
    S_RingBufferTypeDef *const ps_ringBuffer,
    void *const p_data);

E_AppStatusTypeDef UTILS_RingBuffer_ResetPeek(
    S_RingBufferTypeDef *const ps_ringBuffer);

#if defined(__cplusplus)
}
#endif /** __cplusplus */

#endif /** __RING_BUFFER_UTILS_H__ */