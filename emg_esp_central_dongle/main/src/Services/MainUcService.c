/* Includes ------------------------------------------------------------------*/

#include "Services/MainUcService.h"

/* Private define ------------------------------------------------------------*/

/**
 * @brief Tempo limite para atualização dos parâmetros na plataforma.
 */
#define TIMEOUT_UPDATE_PARAMS_MS 7000UL

/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

static const uint8_t APP_WRITE_OVERHEAD = 4;

static profileCallbackTypeDef tableProfileCallbacks[PROFILE_NUM] = {
    GeneralInfoProfile_SRV_SetCharValue,
    DeviceConfig_SRV_SetCharValue,
    TherapyProfile_SRV_SetCharValue,
    CalibrationProfile_SRV_SetCharValue,
    WirelessConnectionsProfile_SRV_SetCharValue,
    MainUc_FileUploadProfile_ResponseCallback,
    MainUc_STMFotaProfile_ResponseCallback,
    MainUc_EspFotaProfile_ResponseCallback,
    TrackPositionProfile_SRV_SetCharValue,
    MainUc_EspProfile_ResponseCallback,
    DebugProfile_SRV_SetCharValue,
};

static volatile S_TherapyParameteresConfigurationTypeDef
    therapyConfiguration[] = {
#define ENTRY_CONFIGURATION(type,                      \
                            externalName,              \
                            translatedName_PT_BR,      \
                            charID,                    \
                            profileID)                 \
    {                                                  \
        .e_type                = type,                 \
        .externalParameterName = externalName,         \
        .name                  = translatedName_PT_BR, \
        .configuredValue       = 0,                    \
        .charId                = charID,               \
        .profileId             = profileID,            \
        .lastUpdatedBy         = "0",                  \
        .uploaded              = false,                \
        .firstSettings         = false,                \
    },
        LIST_THERAPY_CONFIGURATIONS
#undef ENTRY_CONFIGURATION
};

/* Private function prototypes -----------------------------------------------*/

static void MainUc_SRV_MainReceiveCallback(
    S_ProcessedPayloadTypeDef *const ps_processedPayload);

static void SetTherapyConfigurationCallback(
    uint8_t profileId,
    uint8_t charId,
    uint8_t charSize,
    const uint8_t *const parameterValue);

static void MainUc_DeviceConfigProfile_ResponseCallback(
    uint8_t charId,
    uint8_t charSize,
    const uint8_t *const parameterValue);

static void MainUc_GeneralProfile_ResponseCallback(
    uint8_t charId,
    uint8_t charSize,
    const uint8_t *const parameterValue);

/* Public functions ----------------------------------------------------------*/

/******************************************************************************
 * @fn          MainUc_SRV_Init()
 *
 * @brief       Inicialização do serviço de controle principal.
 * @return      E_AppStatusTypeDef - Status de retorno.
 *              @retval APP_GENERAL_ERROR - Erro durante inicialização
 * do serviço.
 *              @retval APP_OK - Serviço inicializado com sucesso.
 ******************************************************************************/
E_AppStatusTypeDef MainUc_SRV_Init(void) {
    IF_TRUE_RETURN(!DRV_STMUsart_Init(), APP_GENERAL_ERROR);

    IF_TRUE_RETURN(
        !DRV_STMUsart_RegisterRxCallback(MAIN_CHAR_WRITE,
                                         MainUc_SRV_MainReceiveCallback),
        APP_GENERAL_ERROR);

    return APP_OK;
}
/******************************************************************************
 * @fn          MainUc_SRV_DeInit()
 *
 * @brief       Deinicialização do serviço de controle principal.
 * @return      E_AppStatusTypeDef - Status de retorno.
 *              @retval APP_GENERAL_ERROR - Erro durante inicialização
 * do serviço.
 *              @retval APP_OK - Serviço deinicializado com sucesso.
 ******************************************************************************/
E_AppStatusTypeDef MainUc_SRV_DeInit(void) {
    IF_TRUE_RETURN(!DRV_STMUsart_DeInit(), APP_GENERAL_ERROR);
    return APP_OK;
}
/******************************************************************************
 * @fn          MainUc_SRV_Handler()
 *
 * @brief       Processa os dados de entrada e saída do barramento de
 *comunicação com o STM32.
 ******************************************************************************/
void MainUc_SRV_Handler(void) { DRV_STMUsart_TransactionsHandler(); }
/******************************************************************************
 * @fn          MainUc_SRV_UpdateParametersTherapy()
 *
 * @brief       Atualiza os parâmetros da terapia alterados pelo usuário através
 * do display.
 ******************************************************************************/
void MainUc_SRV_UpdateParametersTherapy(void) {
    static uint32_t updateTimer = 0;

    IF_TRUE_RETURN(
        !UTILS_Timer_WaitTimeMSec(updateTimer, TIMEOUT_UPDATE_PARAMS_MS));

    updateTimer = UTILS_Timer_Now();

    SRV_HttpRequest_UpdateTherapyDeviceParameters(
        App_AppData_GetSerialNumber(),
        (S_TherapyParameteresConfigurationTypeDef *const)&therapyConfiguration,
        GET_SIZE_ARRAY(therapyConfiguration));
}
/******************************************************************************
 * @fn          MainUc_SRV_AppWriteEvtCB()
 *
 * @brief       Monta uma requisição de operação única e a coloca na fila
 * para ser enviada para o STM32.
 * @return      E_AppStatusTypeDef - Status de retorno.
 *              @retval APP_GENERAL_ERROR - Filha cheia.
 *              @retval APP_OK - Operação executada com sucesso.
 ******************************************************************************/
E_AppStatusTypeDef MainUc_SRV_AppWriteEvtCB(uint8_t profileId,
                                            uint8_t charId,
                                            uint8_t size,
                                            const uint8_t *value) {
    if (size > (UTILS_WE_PROTOCOL_MAX_PAYLOAD_SIZE - APP_WRITE_OVERHEAD)) {
        return APP_INVALID_PARAMETER_ERROR;
    }

    uint16_t pos = 0;

    uint8_t payload[UTILS_WE_PROTOCOL_MAX_PAYLOAD_SIZE] = {
        [0 ...(GET_SIZE_ARRAY(payload) - 1)] = 0};

    payload[pos++] = (uint8_t)APP_CHAR_WRITE;
    payload[pos++] = profileId;
    payload[pos++] = charId;
    payload[pos++] = size;

    if (size > 0 && value != NULL) {
        memcpy((void *)&payload[pos], (void *)value, size);
        pos += size;
    }

    if (!DRV_STMUsart_PutPayloadInTxQueue(payload, pos) != APP_OK) {
        ESP_LOGW(__FUNCTION__, "Fila de transmissão cheia!");
        return APP_GENERAL_ERROR;
    }

    return APP_OK;
}
/******************************************************************************
 * @fn          MainUc_SRV_ReqAllChars()
 *
 * @brief       Monta uma requisição de operação múltipla e  a coloca na fila
 * para ser enviada para o STM32.
 ******************************************************************************/
bool MainUc_SRV_ReqAllChars(uint8_t profileId) {
    uint8_t payload[] = {(uint8_t)REQ_ALL_CHARS, profileId};
    return DRV_STMUsart_PutPayloadInTxQueue(payload, sizeof(payload));
}
/******************************************************************************
 * @fn          MainUc_SRV_GetTherapyConfiguration()
 *
 * @brief       Recupera as configurações atuais de todos os parâmetros da
 * terapia.
 * @return      E_AppStatusTypeDef - Status de retorno.
 *              @retval APP_GENERAL_ERROR - Parâmetro inválido.
 *              @retval APP_OK - Operação executada com sucesso.
 ******************************************************************************/
E_AppStatusTypeDef MainUc_SRV_GetTherapyConfiguration(
    S_TherapyParameteresConfigurationTypeDef *therapyParameters) {
    IF_TRUE_RETURN(!therapyParameters, APP_GENERAL_ERROR);

    memcpy((void *)therapyParameters,
           (void *)&therapyConfiguration,
           sizeof(therapyConfiguration));

    return APP_OK;
}
/******************************************************************************
 * @fn          MainUc_SRV_SetTherapyConfiguration()
 *
 * @brief       Define as configurações de todos os parâmetros da terapia.
 * @return      E_AppStatusTypeDef - Status de retorno.
 *              @retval APP_GENERAL_ERROR - Parâmetro inválido.
 *              @retval APP_OK - Operação executada com sucesso.
 ******************************************************************************/
E_AppStatusTypeDef MainUc_SRV_SetTherapyConfiguration(
    S_TherapyParameteresConfigurationTypeDef *therapyParameters) {
    IF_TRUE_RETURN(!therapyParameters, APP_GENERAL_ERROR);

    memcpy((void *)&therapyConfiguration,
           therapyParameters,
           sizeof(therapyConfiguration));

    return APP_OK;
}
/******************************************************************************
 * @fn          MainUc_SRV_SendEspRunningVersion()
 *
 * @brief       Envia a versão atualmente em execução no Esp.
 ******************************************************************************/
bool MainUc_SRV_SendEspRunningVersion(void) {
    char espRunningVersion[ESP_VERSION_LENGTH] = {
        [0 ...(GET_SIZE_ARRAY(espRunningVersion) - 1)] = 0};

    IF_TRUE_RETURN(SRV_EspFota_GetRunningVersion(espRunningVersion) != APP_OK,
                   false);

    const uint32_t version = App_AppData_GetVersionFirmware(espRunningVersion);

    E_AppStatusTypeDef statusCode =
        MainUc_SRV_AppWriteEvtCB(ESP_PROFILE_APP,
                                 ESP_VERSION_CHAR,
                                 sizeof(version),
                                 (uint8_t *)&version);

    return statusCode == APP_OK;
}
/******************************************************************************
 * @fn          MainUc_SRV_FirstSettingsParamDefinedAll()
 *
 * @brief       Informa se a recuperação de todos os parâmetros de terapia
 * do dispositivo foram bem-sucedidos ou não.
 * @return      Booleano - Status de retorno.
 *              @retval False - Nem todos os parâmetros foram recuperados.
 *              @retval True - Todos os parâmetros recuperados.
 ******************************************************************************/
bool MainUc_SRV_FirstSettingsParamDefinedAll(void) {
    bool firstSettingsDefinedAll = true;
    for (uint8_t i = 0; i < GET_SIZE_ARRAY(therapyConfiguration); i++) {
        if (!therapyConfiguration[i].firstSettings) {
            firstSettingsDefinedAll = false;
            break;
        }
    }

    return firstSettingsDefinedAll;
}
/******************************************************************************
 * @fn          MainUc_SRV_SendEspRunningVersion()
 *
 * @brief       Envia a versão atualmente em execução no Esp.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de enviar a requisição.
 *              @retval True - Requisição enviada com sucesso.
 ******************************************************************************/
bool MainUc_SRV_RequestTherapyDeviceParametersAll(void) {
    return MainUc_SRV_ReqAllChars(DEVICE_CONFIG_PROFILE_APP);
}

/* Weak public functions -----------------------------------------------------*/

__weak void MainUc_STMFotaProfile_ResponseCallback(
    uint8_t charId,
    uint8_t charSize,
    uint8_t *const parameterValue) {}

__weak void MainUc_EspFotaProfile_ResponseCallback(
    uint8_t charId,
    uint8_t charSize,
    uint8_t *const parameterValue) {}

__weak void MainUc_FileUploadProfile_ResponseCallback(
    uint8_t charId,
    uint8_t charSize,
    uint8_t *const parameterValue) {}

/* Private functions ---------------------------------------------------------*/

/******************************************************************************
 * @fn          MainUc_SRV_MainReceiveCallback()
 *
 * @brief       Callback que é notificado ao receber dados do STM32.
 ******************************************************************************/
static void MainUc_SRV_MainReceiveCallback(
    S_ProcessedPayloadTypeDef *const ps_processedPayload) {
    // ESP_LOGW(__FUNCTION__, "Received");

    uint16_t pos = 0;

    const uint8_t profileId   = ps_processedPayload->p_payload[pos++];
    const uint8_t charId      = ps_processedPayload->p_payload[pos++];
    const uint8_t dataLen     = ps_processedPayload->p_payload[pos++];
    const uint8_t *const data = &ps_processedPayload->p_payload[pos];

    if (profileId < PROFILE_NUM) {
        tableProfileCallbacks[profileId](charId, dataLen, (uint8_t *)data);
        SetTherapyConfigurationCallback(profileId,
                                        charId,
                                        dataLen,
                                        (uint8_t *)data);
    }
}
/******************************************************************************
 * @fn          SetTherapyConfigurationCallback()
 *
 * @brief       Define um mais configurações da terapia, segundo a requisição
 * recebida.
 * @param       profileId Identificação do perfil.
 * @param       charId Subidentificação do perfil.
 * @param       charSize Tamanho total dos dados recebidos, em bytes.
 * @param       parameterValue Vetor (ou parâmetro) com os dados.
 ******************************************************************************/
static void SetTherapyConfigurationCallback(
    uint8_t profileId,
    uint8_t charId,
    uint8_t charSize,
    const uint8_t *const parameterValue) {
    switch (profileId) {
        case DEVICE_CONFIG_PROFILE_APP:
            {
                MainUc_DeviceConfigProfile_ResponseCallback(charId,
                                                            charSize,
                                                            parameterValue);
                break;
            }

        case GENERAL_PROFILE_APP:
            {
                MainUc_GeneralProfile_ResponseCallback(charId,
                                                       charSize,
                                                       parameterValue);
                break;
            }

        default:
            break;
    }
}
/******************************************************************************
 * @fn          MainUc_DeviceConfigProfile_ResponseCallback()
 *
 * @brief       Define as configurações do perfil de configurações do
 * dispositivo.
 * @param       charId Subidentificação do perfil.
 * @param       charSize Tamanho total dos dados recebidos, em bytes.
 * @param       parameterValue Vetor (ou parâmetro) com os dados.
 ******************************************************************************/
static void MainUc_DeviceConfigProfile_ResponseCallback(
    uint8_t charId,
    uint8_t charSize,
    const uint8_t *const parameterValue) {
    for (uint8_t i = 0; i < GET_SIZE_ARRAY(therapyConfiguration); i++) {
        if (therapyConfiguration[i].charId == charId) {
            uint8_t value = *parameterValue;

            if (!therapyConfiguration[i].firstSettings) {
                therapyConfiguration[i].firstSettings = true;
            } else {
                therapyConfiguration[i].uploaded = true;
            }

            therapyConfiguration[i].configuredValue = value;
            strcpy((char *)therapyConfiguration[i].lastUpdatedBy,
                   (char *)UPDATED_BY_FW);

            break;
        }
    }
}
/******************************************************************************
 * @fn          MainUc_GeneralProfile_ResponseCallback()
 *
 * @brief       Define as configurações do perfil de configurações do
 * gerais do dispositivo.
 * @param       charId Subidentificação do perfil.
 * @param       charSize Tamanho total dos dados recebidos, em bytes.
 * @param       parameterValue Vetor (ou parâmetro) com os dados.
 ******************************************************************************/
static void MainUc_GeneralProfile_ResponseCallback(
    uint8_t charId,
    uint8_t charSize,
    const uint8_t *const parameterValue) {
    switch (charId) {
        case GEN_SERIAL_NUMBER_CHAR:
            {
                uint64_t serialNumber = 0;

                if (charSize >= sizeof(serialNumber)) {
                    memcpy((void *)&serialNumber,
                           (void *)parameterValue,
                           sizeof(serialNumber));

                    App_AppData_SetSerialNumber(serialNumber);
                }
                break;
            }
    }
}
/******************************************************************************
 * @fn          MainUc_EspProfile_ResponseCallback()
 *
 * @brief       Define as configurações do perfil de configurações genéricas do
 * Esp.
 * @param       charId Subidentificação do perfil.
 * @param       charSize Tamanho total dos dados recebidos, em bytes.
 * @param       parameterValue Vetor (ou parâmetro) com os dados.
 ******************************************************************************/
void MainUc_EspProfile_ResponseCallback(int8_t charId,
                                        uint8_t charSize,
                                        uint8_t *const parameterValue) {
    switch (charId) {
        case ESP_VERSION_CHAR:
            {
                MainUc_SRV_SendEspRunningVersion();
                break;
            }

        case ESP_REESTART_CHAR:
            {
                esp_restart();
                break;
            }
    }
}

/*****************************END OF FILE**************************************/