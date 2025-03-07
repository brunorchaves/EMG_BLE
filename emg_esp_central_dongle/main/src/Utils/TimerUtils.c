/* Includes ------------------------------------------------------------------*/

#include "Utils/TimerUtils.h"

/* Private define ------------------------------------------------------------*/

/**
 * @brief Obtém o tempo percorrido pelo sistema, segundo o tipo de
 * sistema/microcontrolador.
 */
// #if defined(ESP_PLATFORM)
#define GET_TIME_ms() (pdTICKS_TO_MS(xTaskGetTickCount()))
#define GET_TICKS()   (xTaskGetTickCount())
// #endif

/**
 * @brief Tempo máximo permitido.
 */
#define MAX_TIME (UINT32_MAX - 1UL)

/**
 * @brief Tempo relativo.
 */
#define RELACTIVE_TIME_MS(startTime, endTime)                        \
    (startTime > endTime) ? (((MAX_TIME - startTime) + endTime) + 1) \
                          : (endTime - startTime)

/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Public functions ----------------------------------------------------------*/

/******************************************************************************
 * @fn          UTILS_Timer_Now()
 *
 * @brief       Obtém o tempo atual, em milisegundos.
 * @return      uint32_t - Valor de retorno.
 ******************************************************************************/
uint32_t UTILS_Timer_Now(void) { return GET_TIME_ms(); }
/******************************************************************************
 * @fn          UTILS_Timer_GetTicks()
 *
 * @brief       Obtém o tempo atual, em ticks.
 * @return      uint32_t - Valor de retorno.
 ******************************************************************************/
TickType_t UTILS_Timer_GetTicks(void) { return GET_TICKS(); }
/******************************************************************************
 * @fn          UTILS_Timer_ConvertToTicks()
 *
 * @brief       Obtém o tempo informado, em ticks.
 * @param       timeout_ms Tempo limite, em milisegundos.
 * @return      uint32_t - Valor de retorno.
 ******************************************************************************/
uint32_t UTILS_Timer_ConvertToTicks(uint32_t timeout_ms) {
    return (uint32_t)pdMS_TO_TICKS(timeout_ms);
}
/******************************************************************************
 * @fn          UTILS_Timer_GetElapsedTime()
 *
 * @brief       Obtém o tempo percorrido mediante a diferença entre o tempo
 *atual e o tempo informado.
 *
 * @param       timer Variável contendo o tempo atual.
 * @return      uint32_t - Valor de retorno.
 ******************************************************************************/
uint32_t UTILS_Timer_GetElapsedTime(uint32_t timer) {
    const uint32_t ft = GET_TIME_ms();
    return (uint32_t)RELACTIVE_TIME_MS(timer, ft);
}
/******************************************************************************
 * @fn          UTILS_Timer_WaitTimeMSec()
 *
 * @brief       Informa se o tempo esperado, em milisegundos, expirou ou não.
 *
 * @param       timer Variável contendo o tempo atual.
 * @param       timeMs Tempo limite, em milisegundos.
 * @return      Booleano - Status de retorno.
 *              @retval False - Tempo não expirado.
 *              @retval True - Tempo expirado.
 ******************************************************************************/
bool UTILS_Timer_WaitTimeMSec(uint32_t timer, uint32_t timeout_ms) {
    const uint32_t ft = GET_TIME_ms();
    return !(RELACTIVE_TIME_MS(timer, ft) < timeout_ms);
}
/******************************************************************************
 * @fn          UTILS_Timer_WaitTimeSec()
 *
 * @brief       Informa se o tempo esperado, em segundos, expirou ou não.
 *
 * @param       timer Variável contendo o tempo atual.
 * @param       timeMs Tempo limite, em segundos.
 * @return      Booleano - Status de retorno.
 *              @retval False - Tempo não expirado.
 *              @retval True - Tempo expirado.
 ******************************************************************************/
bool UTILS_Timer_WaitTimeSec(uint32_t timer, uint16_t timeout_sec) {
    const uint32_t ft = GET_TIME_ms();
    return !(RELACTIVE_TIME_MS(timer, ft) < ((uint32_t)timeout_sec * 1000));
}
/*****************************END OF FILE**************************************/