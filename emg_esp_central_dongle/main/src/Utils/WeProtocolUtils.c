/* Includes ------------------------------------------------------------------*/

#include "Utils/WeProtocolUtils.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <esp_log.h>
/* Private define ------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static bool IsChecksumValid(
    S_WeProtocolDecodeTypeDef *const ps_weProtocolDecode,
    uint16_t checksum);
static uint16_t ComputeChecksum(uint8_t *const p_payload, uint16_t payloadLen);

/* Public functions ----------------------------------------------------------*/

/******************************************************************************
 * @fn          UTILS_WeProtocol_GetPacketSpaceRequired()
 *
 * @brief       Obtém o espaço necessário para criar um pacote, segundo o
 *tamanho do payload informado.
 * @param       payloadLen Tamanho do vetor, em bytes.
 * @return      uint16_t - Status de retorno.
 ******************************************************************************/
uint16_t UTILS_WeProtocol_GetPacketSpaceRequired(uint16_t payloadLen) {
    return payloadLen + UTILS_WE_PROTOCOL_OVERHEAD;
}
/******************************************************************************
 * @fn          UTILS_WeProtocol_CreatePacket()
 *
 * @brief       Cria um pacote, segundo o modelo do protocolo.
 *
 * @param       p_payload Ponteiro para o vetor com os dados.
 * @param       payloadLen Tamanho do vetor, em bytes.
 * @param       vectorPacket Ponteiro para o vetor que armazenará o pacote com
 * os dados.
 * @param       vectorPacketLen Tamanho máximo do vetor de pacote, em bytes.
 * @return      uint16_t - Status de retorno.
 *              @retval Se 0 - O tamanho do vetor de pacote é insuficient.
 *              @retval Se > 0 - Pacote criado com sucesso.
 ******************************************************************************/
uint16_t UTILS_WeProtocol_CreatePacket(uint8_t *const p_payload,
                                       uint16_t payloadLen,
                                       uint8_t *const vectorPacket,
                                       uint16_t vectorPacketLen) {
    IF_TRUE_RETURN(
        !p_payload || !payloadLen || !vectorPacket || !vectorPacketLen,
        0);

    if (vectorPacketLen < (payloadLen + UTILS_WE_PROTOCOL_OVERHEAD)) {
        return 0;
    }

    uint16_t pos      = 0;
    uint16_t checksum = ComputeChecksum(p_payload, payloadLen);

    vectorPacket[pos++] = UTILS_WE_PROTOCOL_HEADER_BYTE;
    vectorPacket[pos++] = (uint8_t)payloadLen;

    memcpy((void *)&vectorPacket[pos], (void *)p_payload, payloadLen);
    pos += payloadLen;

    vectorPacket[pos++] = LOW_BYTE(checksum);
    vectorPacket[pos++] = HIGH_BYTE(checksum);

    vectorPacket[pos++] = UTILS_WE_PROTOCOL_TAIL_BYTE;

    return pos;
}
/******************************************************************************
 * @fn          UTILS_WeProtocol_ResetDecodeState()
 *
 * @brief       Reseta o estado de decodificação.
 * @param       ps_weProtocolDecode Ponteiro para a estrutura do protocolo WE.
 ******************************************************************************/
void UTILS_WeProtocol_ResetDecodeState(
    S_WeProtocolDecodeTypeDef *const ps_weProtocolDecode) {
    memset((void *)ps_weProtocolDecode, 0, sizeof(S_WeProtocolDecodeTypeDef));
    ps_weProtocolDecode->s_control.decodeState =
        E_WE_PROTOCOL_DECODE_HEADER_BYTE_STATE;
}
/******************************************************************************
 * @fn          UTILS_WeProtocol_DecodePacket()
 *
 * @brief       Decodifica um pacote usando uma estrutura com controle de
 * vetor circular.
 *
 * @param       ps_weProtocolDecode Ponteiro para a estrutura do protocolo WE.
 * @param       ps_ringBuffer Ponteiro para a estrurura de controle de vetor
 * circular.
 * @return      E_AppStatusTypeDef - Status de retorno.
 *              @retval APP_INVALID_PARAMETER_ERROR - Parâmetro
 *inválido.
 *              @retval APP_GENERAL_ERROR - Pacote não encontrado.
 *              @retval APP_BUFFER_EMPTY - Vetor circular vazio.
 *              @retval APP_IS_BUSY - Processamento um possível
 *pacote.
 *              @retval APP_OK - Processamento decodificado com
 *sucesso.
 ******************************************************************************/
E_AppStatusTypeDef UTILS_WeProtocol_DecodePacket(
    S_WeProtocolDecodeTypeDef *const ps_weProtocolDecode,
    S_RingBufferTypeDef *const ps_ringBuffer,
    E_WeProtocolTypeDecodeProcessTypeDef typeDecodeProcess,
    bool show) {
    IF_TRUE_RETURN(!ps_weProtocolDecode || !ps_ringBuffer,
                   APP_INVALID_PARAMETER_ERROR);

    IF_TRUE_RETURN(
        UTILS_RingBuffer_GetStatusVolume(ps_ringBuffer) == APP_BUFFER_EMPTY,
        APP_BUFFER_EMPTY);

    uint8_t item = 0;

    static uint16_t packetChecksum   = 0;
    static uint16_t payloadReadBytes = 0;

    // UTILS_RingBuffer_ResetPeek(ps_ringBuffer);

    do {
        if (ps_weProtocolDecode->s_control.decodeState
            == E_WE_PROTOCOL_DECODE_HEADER_BYTE_STATE) {
            E_AppStatusTypeDef appStatusCode =
                UTILS_RingBuffer_Pop(ps_ringBuffer, (void *)&item);
            UTILS_RingBuffer_ResetPeek(ps_ringBuffer);

            IF_TRUE_RETURN(appStatusCode != APP_OK, APP_BUFFER_EMPTY);
        } else {
            UTILS_RingBuffer_Peek(ps_ringBuffer, (void *)&item);
        }

        switch (ps_weProtocolDecode->s_control.decodeState) {
            default:
            case E_WE_PROTOCOL_DECODE_HEADER_BYTE_STATE:
                {
                    if (item == (uint8_t)UTILS_WE_PROTOCOL_HEADER_BYTE) {
                        ps_weProtocolDecode->s_control.decodeState =
                            E_WE_PROTOCOL_DECODE_PAYLOAD_LENGTH_STATE;
                    }

                    break;
                }

            case E_WE_PROTOCOL_DECODE_PAYLOAD_LENGTH_STATE:
                {
                    if (item) {
                        packetChecksum                  = 0;
                        payloadReadBytes                = 0;
                        ps_weProtocolDecode->payloadLen = item;

                        ps_weProtocolDecode->s_control.decodeState =
                            E_WE_PROTOCOL_DECODE_PAYLOAD_DATA_STATE;
                    } else {
                        ps_weProtocolDecode->s_control.decodeState =
                            E_WE_PROTOCOL_DECODE_HEADER_BYTE_STATE;
                    }
                    break;
                }

            case E_WE_PROTOCOL_DECODE_PAYLOAD_DATA_STATE:
                {
                    ps_weProtocolDecode->payload[payloadReadBytes++] = item;

                    if (payloadReadBytes == ps_weProtocolDecode->payloadLen) {
                        ps_weProtocolDecode->s_control.decodeState =
                            E_WE_PROTOCOL_DECODE_CHECKSUM_1_STATE;
                    }
                    break;
                }

            case E_WE_PROTOCOL_DECODE_CHECKSUM_1_STATE:
                {
                    packetChecksum = (uint16_t)item;
                    ps_weProtocolDecode->s_control.decodeState =
                        E_WE_PROTOCOL_DECODE_CHECKSUM_2_STATE;
                    break;
                }

            case E_WE_PROTOCOL_DECODE_CHECKSUM_2_STATE:
                {
                    packetChecksum |= ((uint16_t)item) << 8;
                    if (IsChecksumValid(ps_weProtocolDecode, packetChecksum)) {
                        ps_weProtocolDecode->s_control.decodeState =
                            E_WE_PROTOCOL_DECODE_TAIL_STATE;
                    } else {
                        ps_weProtocolDecode->s_control.decodeState =
                            E_WE_PROTOCOL_DECODE_HEADER_BYTE_STATE;
                    }
                    break;
                }

            case E_WE_PROTOCOL_DECODE_TAIL_STATE:
                {
                    vTaskDelay(1 / portTICK_PERIOD_MS);

                    ps_weProtocolDecode->s_control.decodeState =
                        E_WE_PROTOCOL_DECODE_HEADER_BYTE_STATE;

                    if (item == (uint8_t)UTILS_WE_PROTOCOL_TAIL_BYTE) {
                        return APP_OK;
                    }
                    break;
                }
        }
    } while (typeDecodeProcess == E_DECODE_TYPE_UNTIL_EMPTY);

    return APP_IS_BUSY;
}
/* Private functions ---------------------------------------------------------*/

/******************************************************************************
 * @fn          IsChecksumValid()
 *
 * @brief       Informa se o checksum para um determinado payload é válido.
 *
 * @param       ps_weProtocolDecode Ponteiro para a estrutura do protocolo
 *WE.
 * @param       checksum Checksum obtido do pacote.
 * @return      Booleano - Status de retorno.
 *              @retval False - Checksum inválido.
 *              @retval True - Checksum válido.
 ******************************************************************************/
static bool IsChecksumValid(
    S_WeProtocolDecodeTypeDef *const ps_weProtocolDecode,
    uint16_t checksum) {
    uint16_t checksumComputed =
        ComputeChecksum(ps_weProtocolDecode->payload,
                        ps_weProtocolDecode->payloadLen);

    return checksumComputed == checksum;
}
/******************************************************************************
 * @fn          ComputeChecksum()
 *
 * @brief       Calcula o valor do checksum para um determinado payload.
 *
 * @param       p_payload Ponteiro para o vetor contendo os dados do
 *payload.
 * @param       payloadLen Tamanho do payload, em bytes.
 * @return      uitn16_t - Valor de retorno.
 ******************************************************************************/
static uint16_t ComputeChecksum(uint8_t *const p_payload, uint16_t payloadLen) {
    uint16_t checksum = 0;

    for (uint8_t i = 0; i < payloadLen; i++) {
        checksum += p_payload[i];
    }

    checksum = ~checksum + 1U;
    return checksum;
}
/*****************************END OF FILE**************************************/
