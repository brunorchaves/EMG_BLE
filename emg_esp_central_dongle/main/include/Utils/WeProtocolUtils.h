/******************************************************************************
 * @file         : WeProtocolUtils.h
 * @author       : CMOS Drake.
 * @brief        : Utilidade responsável por funções de comunicação utilizando
 * protocolo padrão interno estabelecido para os projetos da empresa.
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef __WE_PROTOCOL_UTILS_H__
#define __WE_PROTOCOL_UTILS_H__

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

/* Includes ------------------------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "App/AppConfig.h"
#include "App/AppReturnStatus.h"
#include "Utils/RingBufferUtils.h"

/* Define --------------------------------------------------------------------*/

/**
 * @brief Quantidade de bytes padrão do cabeçalho, no protocolo WE.
 */
#define UTILS_WE_PROTOCOL_OVERHEAD 5UL

/**
 * @brief Tamanho máximo do payload, no protocolo WE.
 */
#define UTILS_WE_PROTOCOL_MAX_PAYLOAD_SIZE __UINT8_MAX__

/**
 * @brief Tamanho máximo do pacote, no protocolo WE.
 */
#define UTILS_WE_PROTOCOL_MAX_PACKET_SIZE \
    (UTILS_WE_PROTOCOL_OVERHEAD + UTILS_WE_PROTOCOL_MAX_PAYLOAD_SIZE)

/**
 * @brief Identificador de byte inicial do pacote, no protocolo WE.
 */
#define UTILS_WE_PROTOCOL_HEADER_BYTE 0x5A

/**
 * @brief Identificador de byte final do pacote, no protocolo WE.
 */
#define UTILS_WE_PROTOCOL_TAIL_BYTE 0xA5

/**
 * @brief Posição inicial em que o payload começa, no protocolo WE.
 */
#define UTILS_WE_PROTOCOL_START_PAYLOAD_POS 2U

/**
 * @brief Identificador de recepção/transmissão de pacote, no protocolo WE.
 */
#define UTILS_WE_PROTOCOL_ACK_COMMAND 0x00

/* Typedef -------------------------------------------------------------------*/

/**
 * @brief Enumerador que identifica os estados para o processo de decodificação
 * de um pacote.
 */
typedef enum E_WeProtocolDecodeMachineState {
    E_WE_PROTOCOL_DECODE_HEADER_BYTE_STATE = 0,
    E_WE_PROTOCOL_DECODE_PAYLOAD_LENGTH_STATE,
    E_WE_PROTOCOL_DECODE_PAYLOAD_DATA_STATE,
    E_WE_PROTOCOL_DECODE_CHECKSUM_1_STATE,
    E_WE_PROTOCOL_DECODE_CHECKSUM_2_STATE,
    E_WE_PROTOCOL_DECODE_TAIL_STATE,
    E_WE_PROTOCOL_REMOVE_ITEM,
} E_WeProtocolDecodeMachineStateTypeDef;

/**
 * @brief Enumerador que identifica o tipo de processo de decodificação.
 */
typedef enum E_WeProtocolTypeDecodeProcess {
    E_DECODE_TYPE_ONE_CICLE = 0,
    E_DECODE_TYPE_UNTIL_EMPTY,
} E_WeProtocolTypeDecodeProcessTypeDef;

/**
 * @brief Estrutura que armazena informações para gerenciamento do processo
 * de decodificação de pacotes.
 */
typedef struct S_WeProtocolDecode {
    uint16_t payloadLen;
    volatile uint8_t payload[UTILS_WE_PROTOCOL_MAX_PAYLOAD_SIZE];

    struct {
        E_WeProtocolDecodeMachineStateTypeDef decodeState;
    } s_control;
} S_WeProtocolDecodeTypeDef;

/* Exported types ------------------------------------------------------------*/
/* Public objects ------------------------------------------------------------*/
/* Public functions ----------------------------------------------------------*/

uint16_t UTILS_WeProtocol_GetPacketSpaceRequired(uint16_t payloadLen);

uint16_t UTILS_WeProtocol_CreatePacket(uint8_t *const p_payload,
                                       uint16_t payloadLen,
                                       uint8_t *const vectorPacket,
                                       uint16_t vectorPacketLen);

void UTILS_WeProtocol_ResetDecodeState(
    S_WeProtocolDecodeTypeDef *const ps_weProtocolDecode);

E_AppStatusTypeDef UTILS_WeProtocol_DecodePacket(
    S_WeProtocolDecodeTypeDef *const ps_weProtocolDecode,
    S_RingBufferTypeDef *const ps_ringBuffer,
    E_WeProtocolTypeDecodeProcessTypeDef typeDecodeProcess,
    bool show);

#if defined(__cplusplus)
}
#endif /** __cplusplus */

#endif /** __WE_PROTOCOL_UTILS_H__ */