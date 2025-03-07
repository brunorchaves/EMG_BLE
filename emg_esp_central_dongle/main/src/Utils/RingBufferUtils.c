/* Includes ------------------------------------------------------------------*/

#include "Utils/RingBufferUtils.h"

/* Private define ------------------------------------------------------------*/

struct S_RingBuffer {
    uint16_t writePos;
    uint16_t readPos;
    uint16_t peekPos;
    uint8_t sizeOfItem;
    uint16_t maxQuantity;
    void *p_vector;
    uint16_t countItems;
};

/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Public functions ----------------------------------------------------------*/

/******************************************************************************
 * @fn          UTILS_RingBuffer_Create()
 *
 * @brief       Cria uma nova estrutura de controle buffer circular.
 * @param       pps_ringBuffer Ponteiro do ponteiro da estrutura declarada.
 * @param       p_vector Ponteiro para o vetor.
 * @param       maxQuantity Quantidade máxima de posições.
 * @param       sizeOfItem Tamanho de um item do vetor.
 * @return	    E_AppStatusTypeDef - Status de retorno.
 *              @retval RETURN_STATUS_INVALID_PARAM_ERROR - Um ou mais
 *parâmetros inválidos.
 *              @retval APP_GENERAL_ERROR - Não foi possível alocar
 *dinamicamente um espaço para a estrutura.
 *              @retval APP_OK - Estrutura criada com sucesso.
 ******************************************************************************/
E_AppStatusTypeDef UTILS_RingBuffer_Create(
    S_RingBufferTypeDef **const pps_ringBuffer,
    void *const p_vector,
    const uint16_t maxQuantity,
    const uint8_t sizeOfItem) {
    IF_TRUE_RETURN(*pps_ringBuffer || !p_vector, APP_INVALID_PARAMETER_ERROR);
    IF_TRUE_RETURN(!maxQuantity || !sizeOfItem, APP_INVALID_PARAMETER_ERROR);

    S_RingBufferTypeDef s_ringBuffer = {
        .countItems  = 0,
        .peekPos     = 0,
        .readPos     = 0,
        .writePos    = (maxQuantity - 1),
        .sizeOfItem  = sizeOfItem,
        .maxQuantity = maxQuantity,
        .p_vector    = p_vector,
    };

    *pps_ringBuffer =
        (S_RingBufferTypeDef *)pvPortMalloc(sizeof(S_RingBufferTypeDef));
    memset((void *)*pps_ringBuffer, 0, sizeof(**pps_ringBuffer));

    IF_TRUE_RETURN(!(*pps_ringBuffer), APP_GENERAL_ERROR);

    memcpy((void *)*pps_ringBuffer,
           (void *)&s_ringBuffer,
           sizeof(S_RingBufferTypeDef));
    return APP_OK;
}
/******************************************************************************
 * @fn          UTILS_RingBuffer_Destroy()
 *
 * @brief       Destrói a estrutura de controle buffer circular.
 * @param       pps_ringBuffer Ponteiro do ponteiro da estrutura declarada.
 * @return	    E_AppStatusTypeDef - Status de retorno.
 *              @retval APP_GENERAL_ERROR - Não foi possível desalocar
 *dinamicamente o espaço da estrutura na memória.
 *              @retval APP_OK - Estrutura destruída com sucesso.
 ******************************************************************************/
E_AppStatusTypeDef UTILS_RingBuffer_Destroy(
    S_RingBufferTypeDef **const pps_ringBuffer) {
    IF_TRUE_RETURN(!(*pps_ringBuffer), APP_OK);

    vPortFree((void *)*pps_ringBuffer);
    // configASSERT(!(*pps_ringBuffer));

    IF_TRUE_RETURN(*pps_ringBuffer, APP_GENERAL_ERROR);

    return APP_OK;
}
/******************************************************************************
 * @fn          UTILS_RingBuffer_GetStatusVolume()
 *
 * @brief       Informa o status do volume.
 * @param       ps_ringBuffer Ponteiro para a estrutura.
 * @return	    E_AppStatusTypeDef - Status de retorno.
 *              @retval APP_INVALID_PARAMETER_ERROR - Um ou mais
 *parâmetros inválidos.
 *              @retval APP_BUFFER_EMPTY - Vetor circular vazio.
 *              @retval APP_BUFFER_FULL - Vetor circular cheio.
 *              @retval APP_OK - Vetor circular com dados.
 ******************************************************************************/
E_AppStatusTypeDef UTILS_RingBuffer_GetStatusVolume(
    S_RingBufferTypeDef *const ps_ringBuffer) {
    IF_TRUE_RETURN(!ps_ringBuffer, APP_INVALID_PARAMETER_ERROR);

    if ((uint16_t)ps_ringBuffer->countItems == 0) {
        return APP_BUFFER_EMPTY;
    } else if ((uint16_t)ps_ringBuffer->countItems
               == ps_ringBuffer->maxQuantity) {
        return APP_BUFFER_FULL;
    }

    return APP_OK;
}
/******************************************************************************
 * @fn          UTILS_RingBuffer_GetSpaceAvailable()
 *
 * @brief       Informa o espaço disponível.
 * @param       ps_ringBuffer Ponteiro para a estrutura.
 * @param       p_countItemsAvailable Ponteiro para a variável que será
 * preenchida com o valor.
 * @return	    E_AppStatusTypeDef - Status de retorno.
 *              @retval APP_INVALID_PARAMETER_ERROR - Um ou mais
 *parâmetros inválidos.
 *              @retval APP_OK - Operação executada com sucesso.
 ******************************************************************************/
E_AppStatusTypeDef UTILS_RingBuffer_GetSpaceAvailable(
    S_RingBufferTypeDef *const ps_ringBuffer,
    uint16_t *const p_countItemsAvailable) {
    IF_TRUE_RETURN(!ps_ringBuffer || !p_countItemsAvailable,
                   APP_INVALID_PARAMETER_ERROR);
    uint16_t countItemsAvailable =
        ps_ringBuffer->maxQuantity - (uint16_t)ps_ringBuffer->countItems;

    *p_countItemsAvailable = countItemsAvailable;
    return APP_OK;
}
/******************************************************************************
 * @fn          UTILS_RingBuffer_Push()
 *
 * @brief       Insere um elemento na fila.
 * @param       ps_ringBuffer Ponteiro para a estrutura.
 * @param       p_data Ponteiro para o elemento.
 * @return	    E_AppStatusTypeDef - Status de retorno.
 *              @retval APP_INVALID_PARAMETER_ERROR - Um ou mais
 *parâmetros inválidos.
 *              @retval APP_BUFFER_FULL - Vetor circular cheio.
 *              @retval APP_OK - Operação executada com sucesso.
 ******************************************************************************/
E_AppStatusTypeDef UTILS_RingBuffer_Push(
    S_RingBufferTypeDef *const ps_ringBuffer,
    void *const p_data) {
    IF_TRUE_RETURN(!ps_ringBuffer || !p_data, APP_INVALID_PARAMETER_ERROR);

    E_AppStatusTypeDef appStatusCode =
        UTILS_RingBuffer_GetStatusVolume(ps_ringBuffer);

    IF_TRUE_RETURN(appStatusCode == APP_BUFFER_FULL, APP_BUFFER_FULL);

    ps_ringBuffer->writePos += 1;

    if (ps_ringBuffer->writePos >= ps_ringBuffer->maxQuantity) {
        ps_ringBuffer->writePos = 0;
    }

    volatile void *p_startCopy =
        (void *)ps_ringBuffer->p_vector
        + (ps_ringBuffer->writePos * ps_ringBuffer->sizeOfItem);

    memcpy((void *)p_startCopy, (void *)p_data, ps_ringBuffer->sizeOfItem);

    ps_ringBuffer->countItems += 1;

    return APP_OK;
}
/******************************************************************************
 * @fn          UTILS_RingBuffer_Pop()
 *
 * @brief       Remove um elemento da fila.
 * @param       ps_ringBuffer Ponteiro para a estrutura.
 * @param       p_data Ponteiro para variável cujo valor será preenchido.
 * @return	    E_AppStatusTypeDef - Status de retorno.
 *              @retval APP_INVALID_PARAMETER_ERROR - Um ou mais
 *parâmetros inválidos.
 *              @retval APP_BUFFER_EMPTY - Vetor circular vazio.
 *              @retval APP_OK - Operação executada com sucesso.
 ******************************************************************************/
E_AppStatusTypeDef UTILS_RingBuffer_Pop(
    S_RingBufferTypeDef *const ps_ringBuffer,
    void *const p_data) {
    IF_TRUE_RETURN(!ps_ringBuffer || !p_data, APP_INVALID_PARAMETER_ERROR);

    E_AppStatusTypeDef appStatusCode =
        UTILS_RingBuffer_GetStatusVolume(ps_ringBuffer);

    IF_TRUE_RETURN(appStatusCode == APP_BUFFER_EMPTY, APP_BUFFER_EMPTY);

    volatile void *p_startCopy =
        ps_ringBuffer->p_vector
        + (ps_ringBuffer->readPos * ps_ringBuffer->sizeOfItem);

    memcpy((void *)p_data, (void *)p_startCopy, ps_ringBuffer->sizeOfItem);
    memset((void *)p_startCopy, 0, ps_ringBuffer->sizeOfItem);

    ps_ringBuffer->readPos += 1;

    if (ps_ringBuffer->readPos >= ps_ringBuffer->maxQuantity) {
        ps_ringBuffer->readPos = 0;
    }

    ps_ringBuffer->countItems -= 1;

    return APP_OK;
}
/******************************************************************************
 * @fn          UTILS_RingBuffer_Peek()
 *
 * @brief       Obtém um elemento da fila, sem removê-lo, e incrementa
 * internamente a posição para obter o próximo elemento.
 * @param       ps_ringBuffer Ponteiro para a estrutura.
 * @param       p_data Ponteiro para variável cujo valor será preenchido.
 * @return	    E_AppStatusTypeDef - Status de retorno.
 *              @retval APP_INVALID_PARAMETER_ERROR - Um ou mais
 *parâmetros inválidos.
 *              @retval APP_BUFFER_EMPTY - Vetor circular vazio.
 *              @retval APP_OK - Operação executada com sucesso.
 ******************************************************************************/
E_AppStatusTypeDef UTILS_RingBuffer_Peek(
    S_RingBufferTypeDef *const ps_ringBuffer,
    void *const p_data) {
    IF_TRUE_RETURN(!ps_ringBuffer || !p_data, APP_INVALID_PARAMETER_ERROR);

    volatile void *p_startCopy =
        ps_ringBuffer->p_vector
        + (ps_ringBuffer->peekPos * ps_ringBuffer->sizeOfItem);

    memcpy((void *)p_data, (void *)p_startCopy, ps_ringBuffer->sizeOfItem);

    ps_ringBuffer->peekPos += 1;

    if (ps_ringBuffer->peekPos >= ps_ringBuffer->maxQuantity) {
        ps_ringBuffer->peekPos = 0;
    }

    return APP_OK;
}
/******************************************************************************
 * @fn          UTILS_RingBuffer_Peek()
 *
 * @brief       Redefine a posição de observação para a posição atual de leitura
 * dos dados.
 * @param       ps_ringBuffer Ponteiro para a estrutura.
 * @return	    E_AppStatusTypeDef - Status de retorno.
 *              @retval APP_INVALID_PARAMETER_ERROR - Um ou mais
 *parâmetros inválidos.
 *              @retval APP_OK - Operação executada com sucesso.
 ******************************************************************************/
E_AppStatusTypeDef UTILS_RingBuffer_ResetPeek(
    S_RingBufferTypeDef *const ps_ringBuffer) {
    IF_TRUE_RETURN(!ps_ringBuffer, APP_INVALID_PARAMETER_ERROR);

    ps_ringBuffer->peekPos = ps_ringBuffer->readPos;
    return APP_OK;
}

/* Private functions ---------------------------------------------------------*/

/*****************************END OF FILE**************************************/