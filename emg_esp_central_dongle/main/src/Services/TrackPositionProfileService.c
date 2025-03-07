/******************************************************************************
 * @file         : TrackPositionProfileService.c
 * @authors      : Thales Mol Vinhal
 * @copyright    : Copyright (c) 2024 CMOS Drake
 * @brief        : Serviço para gerenciamento de informações de localização.
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/

#include "Services/TrackPositionProfileService.h"

/* Private define ------------------------------------------------------------*/
/* Typedefs ------------------------------------------------------------------*/

typedef enum E_TrackPositionProfileCharacteristics {
    TRACK_POSITION_DATA_CHAR,
    TRACK_POSITION_UPLOAD_CONFIRM_CHAR,
    TRACK_POSITION_CHAR_SIZE,
} E_TrackPositionProfileCharacteristicsTypeDefs;

typedef enum E_TrackPositionStates {
    START_SEND,
    FILE_INFO,
    FILE_FRAME,
    FINISH_SEND,
    INIT_STATE,
    WAITING_FOR_CONFIRMATION,
    WAITING_FOR_NEW_DATA,
    SET_NEW_FILE_TO_UPLOAD,
} E_TrackPositionStatesTypeDef;

/* Private function prototypes -----------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

static bool pendingUpload = false;
static S_TrackPositionTypeDef receivedTrackPosition;

/* Public functions ----------------------------------------------------------*/

/******************************************************************************
 * @brief Função para executar o perfil de Upload de Arquivos conforme o identi
 * ficador da característica recebida.
 *
 * @param charId Id da característica recebida
 * @param size   Tamanho do pacote recebido
 * @param p_value Ponteiro para os dados recebidos
 ******************************************************************************/
void TrackPositionProfile_SRV_SetCharValue(uint8_t charId,
                                           uint8_t size,
                                           const uint8_t *p_value) {
    if (charId == TRACK_POSITION_DATA_CHAR) {
        uint8_t i = 0;

        // Extrair time
        memcpy(receivedTrackPosition.time,
               p_value,
               sizeof(receivedTrackPosition.time));
        i += sizeof(receivedTrackPosition.time)
             + 1;  // +1 para pular o byte zero adicional
        ESP_LOGI(__FUNCTION__,
                 "receivedTrackPosition.time : %s",
                 receivedTrackPosition.time);

        // Extrair satellites
        memcpy(&receivedTrackPosition.satellites,
               &p_value[i],
               sizeof(receivedTrackPosition.satellites));
        i += sizeof(receivedTrackPosition.satellites)
             + 1;  // +1 para pular o byte zero adicional
        ESP_LOGI(__FUNCTION__,
                 "receivedTrackPosition.satellites : %d",
                 receivedTrackPosition.satellites);

        // Extrair latitude
        memcpy(&receivedTrackPosition.latitude,
               &p_value[i],
               sizeof(receivedTrackPosition.latitude));
        i += sizeof(receivedTrackPosition.latitude)
             + 1;  // +1 para pular o byte zero adicional
        ESP_LOGI(__FUNCTION__,
                 "receivedTrackPosition.latitude : %f",
                 receivedTrackPosition.latitude);

        // Extrair longitude
        memcpy(&receivedTrackPosition.longitude,
               &p_value[i],
               sizeof(receivedTrackPosition.longitude));
        i += sizeof(receivedTrackPosition.longitude)
             + 1;  // +1 para pular o byte zero adicional
        ESP_LOGI(__FUNCTION__,
                 "receivedTrackPosition.longitude : %f",
                 receivedTrackPosition.longitude);

        // Extrair altitude
        memcpy(&receivedTrackPosition.altitude,
               &p_value[i],
               sizeof(receivedTrackPosition.altitude));
        i += sizeof(receivedTrackPosition.altitude)
             + 1;  // +1 para pular o byte zero adicional
        ESP_LOGI(__FUNCTION__,
                 "receivedTrackPosition.altitude : %f",
                 receivedTrackPosition.altitude);

        // Extrair imprecisão
        memcpy(&receivedTrackPosition.imprecision,
               &p_value[i],
               sizeof(receivedTrackPosition.imprecision));
        ESP_LOGI(__FUNCTION__,
                 "receivedTrackPosition.imprecision : %f",
                 receivedTrackPosition.imprecision);

        pendingUpload = true;
    }
}

/******************************************************************************
 * @brief Função para obter se há dados pendentes para upload
 *
 * @retval True - Caso há dados pendentes para upload
 * @retval False - Caso contrário
 ******************************************************************************/
const bool TrackPositionProfile_SRV_GetPendingUpload() { return pendingUpload; }

/******************************************************************************
 * @brief Função para definir o estado de upload pendente
 *
 * @param pending Valor a ser definido para o estado de upload pendente
 ******************************************************************************/
void TrackPositionProfile_SRV_SetPendingUpload(bool pending) {
    if (pendingUpload && !pending) {
        uint8_t succesBit = 1;
        MainUc_SRV_AppWriteEvtCB(TRACK_POSITION_PROFILE_APP,
                                 TRACK_POSITION_UPLOAD_CONFIRM_CHAR,
                                 sizeof(uint8_t),
                                 &succesBit);

        pendingUpload = false;
    }
}

/******************************************************************************
 * @brief Função para obter os dados de localização recebidos
 ******************************************************************************/
void TrackPositionProfile_SRV_GetTrackPosition(
    S_TrackPositionTypeDef *trackPosition) {
    memcpy(trackPosition,
           &receivedTrackPosition,
           sizeof(S_TrackPositionTypeDef));
}

/* Private functions ---------------------------------------------------------*/

/*****************************END OF FILE**************************************/