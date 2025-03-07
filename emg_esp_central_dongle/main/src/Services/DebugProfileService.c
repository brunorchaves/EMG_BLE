/* Includes ------------------------------------------------------------------*/

#include "Services/DebugProfileService.h"

/* Private define ------------------------------------------------------------*/
/* Typedef -------------------------------------------------------------------*/

typedef struct S_DebugVariables {
    float rawValue;
    float filtered;
    float baselineSignal;
    float derivativeSignal;
    float debugReference;
} S_DebugVariablesTypeDef;

/* Private function prototypes -----------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

static S_DebugVariablesTypeDef pressureSignal;
static S_DebugVariablesTypeDef flowSignal;

/* Public functions ----------------------------------------------------------*/

/******************************************************************************
 * @brief Função para receber as variáveis de debug vindas do STM32
 *
 * @param charId Id da característica recebida
 * @param size   Tamanho do pacote recebido
 * @param p_value Ponteiro para os dados recebidos
 ******************************************************************************/
void DebugProfile_SRV_SetCharValue(uint8_t charId,
                                   uint8_t size,
                                   const uint8_t *p_value) {
    float value = 0;
    memcpy(&value, p_value, sizeof(value));

    switch (charId) {
        case DEBUG_PRESSURE_VALUE_CHAR:
            {
                pressureSignal.rawValue = value;
                break;
            }

        case DEBUG_FILTERED_PRESSURE_CHAR:
            {
                pressureSignal.filtered = value;
                break;
            }

        case DEBUG_PRESSURE_BASELINE_CHAR:
            {
                pressureSignal.baselineSignal = value;
                break;
            }

        case DEBUG_PRESSURE_DERIVATIVE_CHAR:
            {
                pressureSignal.derivativeSignal = value;
                break;
            }

        case DEBUG_PRESSURE_REFERENCE_CHAR:
            {
                pressureSignal.debugReference = value;
                break;
            }

        case DEBUG_FLOW_VALUE_CHAR:
            {
                pressureSignal.rawValue = value;
                break;
            }
    }
}

void DebugProfile_SRV_PrintValues(void) {
    int32_t currentTimeMs = UTILS_Timer_Now();

    printf("%.02f %.02f 0.0 0.0\n",
           pressureSignal.rawValue,
           flowSignal.rawValue);
}
/*****************************END OF FILE**************************************/