/* Includes ------------------------------------------------------------------*/

#include "Services/STMFotaService.h"

/* Private define ------------------------------------------------------------*/

/**
 * @brief Identificador único universal para aplicações OXYGENIS.
 */
#define BOOTLOADER_OXYGENIS_UUID 0x7E292A76747D7824ULL

/**
 * @brief Quantidade máxima de tentativas.
 */
#define MAX_ATTEMPTS 3

/**
 * @brief Tamanho máximo para vetor de caracteres interno.
 */
#define RANGE_TEXT_MAX_LENGTH 32U

/**
 * @brief Endereço inicial, que identifica o tipo de equipamento pelo código
 * mágico.
 */
#define START_ADDRESS_STM_MAGIC_CODE 0x00UL

/**
 * @brief Tamaho do código mágico, em bytes.
 */
#define MAGIC_CODE_LENGTH 8UL

/**
 * @brief Endereço inicial, no arquivo binário do firmware, em que está
 * localizado a versão do firmware.
 */
#define START_ADDRESS_STM_VERSION 0xA00UL

/* Private typedef -----------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static bool EndConnection(void);
static bool BeginConnection(void);
static bool ConfirmMagicCode(void);
static esp_err_t STM32HttpEventHandler(esp_http_client_event_t *evt);

/* Private variables ---------------------------------------------------------*/

static uint8_t *const volatile p_response     = NULL;
static esp_http_client_handle_t ps_httpClient = NULL;

static esp_http_client_config_t config = {
    .url                         = STM_URL_UPDATE_FIRMWARE,
    .timeout_ms                  = 7000,
    .event_handler               = STM32HttpEventHandler,
    .skip_cert_common_name_check = true,
    .user_data                   = NULL,
    .buffer_size_tx              = 1024,                                 // TX
    .buffer_size                 = SRV_STM_FOTA_DOWNLOAD_BUFFER_LENGTH,  // RX
};

/* Public functions ----------------------------------------------------------*/

/******************************************************************************
 * @fn          SRV_STMFota_Init()
 *
 * @brief       Inicializa o serviço de download de firmware do STM.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool SRV_STMFota_Init(void) {
    IF_TRUE_RETURN(p_response, true);

    size_t *ps_aux = (size_t *)&p_response;
    *ps_aux        = (size_t)pvPortMalloc(SRV_STM_FOTA_DOWNLOAD_BUFFER_LENGTH
                                   * sizeof(*p_response));

    IF_TRUE_RETURN(!p_response, false);

    memset((void *)p_response, 0, SRV_STM_FOTA_DOWNLOAD_BUFFER_LENGTH);

    config.user_data   = p_response;
    config.buffer_size = SRV_STM_FOTA_DOWNLOAD_BUFFER_LENGTH;

    return true;
}
/******************************************************************************
 * @fn          SRV_STMFota_DeInit()
 *
 * @brief       Deinicializa o serviço de download de firmware do STM.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool SRV_STMFota_DeInit(void) {
    IF_TRUE_RETURN(!ps_httpClient && !p_response, true);

    if (ps_httpClient) {
        IF_TRUE_RETURN(!DRV_HttpClient_DestroyInstance(&ps_httpClient), false);
    }

    if (p_response) {
        vPortFree((void *)p_response);
        size_t *ps_aux = (size_t *)&p_response;
        *ps_aux        = 0;

        IF_TRUE_RETURN(p_response, false);

        config.user_data = NULL;
    }

    return true;
}
/******************************************************************************
 * @fn          SRV_STMFota_GetVersionAvailable()
 *
 * @brief       Obtém a versão do STM disponível em nuvem.
 * @param       p_firmwareVersion Ponteiro para a cadeia de caracteres que
 * será preenchida com a versão disponível em nuvem.
 * @param       firmwareVersionLen Tamanho máximo do vetor, em bytes.
 * @param       p_magicCodeValid Ponteiro para confirma se a imagem disponível
 * é uma imagem válida para o tipo de equipamento esperado.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool SRV_STMFota_GetVersionAvailable(uint8_t *const p_firmwareVersion,
                                     const uint16_t firmwareVersionLen,
                                     bool *const p_magicCodeValid) {
    IF_TRUE_RETURN(!p_firmwareVersion || !firmwareVersionLen, false);

    IF_TRUE_RETURN(
        !p_firmwareVersion
            || firmwareVersionLen < SRV_STM_FOTA_VERSION_BYTES_LENGTH,
        false);

    IF_TRUE_RETURN(!BeginConnection(), false);

    memset(p_firmwareVersion, 0, firmwareVersionLen);

    if (esp_http_client_set_method(ps_httpClient, HTTP_METHOD_GET) != ESP_OK) {
        goto fail;
    }

    char bytesRange[RANGE_TEXT_MAX_LENGTH] = {
        [0 ...(GET_SIZE_ARRAY(bytesRange) - 1)] = 0};

    sprintf(bytesRange,
            "bytes=%lu-%lu",
            START_ADDRESS_STM_VERSION,
            START_ADDRESS_STM_VERSION + SRV_STM_FOTA_VERSION_BYTES_LENGTH);

    if (esp_http_client_set_header(ps_httpClient, "Range", bytesRange)
        != ESP_OK) {
        goto fail;
    }

    bool status = DRV_HttpClient_Perform(&ps_httpClient, NULL);

    if (!status) {
        goto fail;
    }

    int maxLength = esp_http_client_get_content_length(ps_httpClient);

    if (!maxLength) {
        goto fail;
    }

    memcpy((void *)p_firmwareVersion,
           (void *)p_response,
           SRV_STM_FOTA_VERSION_BYTES_LENGTH);

    sprintf(bytesRange,
            "bytes=%lu-%lu",
            START_ADDRESS_STM_MAGIC_CODE,
            START_ADDRESS_STM_MAGIC_CODE + MAGIC_CODE_LENGTH);

    if (esp_http_client_set_header(ps_httpClient, "Range", bytesRange)
        != ESP_OK) {
        goto fail;
    }

    status = DRV_HttpClient_Perform(&ps_httpClient, NULL);

    if (!status) {
        goto fail;
    }

    maxLength = esp_http_client_get_content_length(ps_httpClient);

    if (!maxLength) {
        goto fail;
    }

    *p_magicCodeValid = ConfirmMagicCode();
    EndConnection();

    return true;

fail:
    {
        EndConnection();
        return false;
    }
}
/******************************************************************************
 * @fn          SRV_STMFota_GetTotalImageLength()
 *
 * @brief       Obtém o tamanho total da imagem disponível em nuvem.
 * @param       p_totalImageLen Ponteiro para variável que será preenchida
 * com o valor do tamanho da imagem.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool SRV_STMFota_GetTotalImageLength(uint32_t *const p_totalImageLen) {
    IF_TRUE_RETURN(!p_totalImageLen, false);

    *p_totalImageLen = 0;
    IF_TRUE_RETURN(!BeginConnection(), false);

    /**
     * Retira o cabeçalho "Range", caso a função anterior a esta tenho sido
     * utilizada.
     */
    esp_http_client_delete_header(ps_httpClient, "Range");
    esp_http_client_set_method(ps_httpClient, HTTP_METHOD_HEAD);

    bool status   = DRV_HttpClient_Perform(&ps_httpClient, NULL);
    int maxLength = 0;

    if (status) {
        maxLength = esp_http_client_get_content_length(ps_httpClient);

        if (maxLength) {
            *p_totalImageLen = (uint32_t)maxLength;
        }
    }

    EndConnection();

    return status && maxLength;
}
/******************************************************************************
 * @fn          SRV_STMFota_StreamDownload()
 *
 * @brief       Baixa parcialmente parte da imagem do binário do STM.
 * @param       p_buffer Ponteiro para vetor que será preenchido com parte
 * da imagem baixada.
 * @param       offsetImage Valor do deslocamento da imagem.
 * @param       bytesToRead Quantidade de bytes para serem lidos, a partir
 * do valor de deslocamento da imagem.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool SRV_STMFota_StreamDownload(uint8_t *const p_buffer,
                                const uint32_t offsetImage,
                                const uint16_t bytesToRead) {
    IF_TRUE_RETURN(!p_buffer || !bytesToRead, false);

    if (!BeginConnection()) {
#if defined(ENABLE_VERBOSE_STM_FOTA_SERVICE)
        ESP_LOGI(__FILE__, "STM Stream fail");
#endif

        return false;
    }

    uint16_t readBytes                    = 0;
    char rangeText[RANGE_TEXT_MAX_LENGTH] = {
        [0 ...(GET_SIZE_ARRAY(rangeText) - 1)] = 0};

    if (esp_http_client_set_method(ps_httpClient, HTTP_METHOD_GET) != ESP_OK) {
        EndConnection();
        return false;
    }

    while (readBytes < bytesToRead) {
        if (readBytes > 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        uint16_t readBlock =
            MIN(bytesToRead - readBytes, SRV_STM_FOTA_DOWNLOAD_BUFFER_LENGTH);

        sprintf(rangeText,
                "bytes=%lu-%lu",
                (uint32_t)(offsetImage + readBytes),
                (uint32_t)(offsetImage + readBytes + readBlock - 1));

        if (esp_http_client_set_header(ps_httpClient, "Range", rangeText)
            != ESP_OK) {
            break;
        }

        if (!DRV_HttpClient_Perform(&ps_httpClient, NULL)) {
#if defined(ENABLE_VERBOSE_STM_FOTA_SERVICE)
            ESP_LOGI(__FILE__, "STM Stream fail in while loop...");
#endif
            break;
        }

        int contentLength = esp_http_client_get_content_length(ps_httpClient);

        if (contentLength != readBlock) {
            break;
        }

        memcpy(p_buffer + readBytes, (void *)p_response, contentLength);
        readBytes += contentLength;
    }

    EndConnection();
    return readBytes == bytesToRead;
}
/******************************************************************************
 * @fn          SRV_STMFota_CheckFirmwareVersion()
 *
 * @brief       Compara a versão disponível em nuvem com a versão atualmente
 * em execução no STM e informa se a versão em nuvem é ou não superior à
 * versão em execução do STM.
 * @param       p_versionAvailable Ponteiro para cadeia de caracteres que contém
 * o conteúdo da imagem disponível em nuvem.
 * @param       p_versionRunning Ponteiro para cadeia de caracteres que contém
 * o conteúdo da imagem em execução do STM.
 * @return      Booleano - Status de retorno.
 *              @retval False - Versão em nuvem igual ou inferior à versão
 * em execução no STM.
 *              @retval True - Nova versão disponível em nuvem.
 ******************************************************************************/
bool SRV_STMFota_CheckFirmwareVersion(const char *const p_versionAvailable,
                                      const char *const p_versionRunning) {
    IF_TRUE_RETURN(!p_versionAvailable || !p_versionRunning, false);

    if (strcmp(p_versionAvailable, p_versionRunning) <= 0) {
        return false;
    }

    return true;
}
/******************************************************************************
 * @fn          SRV_STMFota_GetFirmwareVersionFormated()
 *
 * @brief       Obtém a versão do firmware formatada em cadeia de caracteres,
 * separados pelo caractere "ponto".
 * @param       p_fwVersionBuffer Vetor contendo os números relacionados à
 * versão do firmware.
 * @param       p_fwVersionFormmated Ponteiro para cadeia de caracteres que será
 * preenchida com a formatação esperada.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool SRV_STMFota_GetFirmwareVersionFormatted(uint8_t *const p_fwVersionBuffer,
                                             char *const p_fwVersionFormmated) {
    IF_TRUE_RETURN(!p_fwVersionBuffer || !p_fwVersionFormmated, false);

    uint8_t len = 0;

    uint8_t major    = (uint8_t)p_fwVersionBuffer[len++];
    uint8_t minor    = (uint8_t)p_fwVersionBuffer[len++];
    uint8_t build    = (uint8_t)p_fwVersionBuffer[len++];
    uint8_t revision = (uint8_t)p_fwVersionBuffer[len++];

    sprintf(p_fwVersionFormmated, "%u.%u.%u.%u", major, minor, build, revision);

    return true;
}

/* Private functions ---------------------------------------------------------*/

/******************************************************************************
 * @fn          BeginConnection()
 *
 * @brief       Destrói o objeto de conexão previamente existente.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
static bool EndConnection(void) {
    if (ps_httpClient) {
        IF_TRUE_RETURN(!DRV_HttpClient_DestroyInstance(&ps_httpClient), false);
    }

    return true;
}
/******************************************************************************
 * @fn          BeginConnection()
 *
 * @brief       Destrói o objeto de conexão previamente existente, se houver,
 * e tenta criar uma nova instância de conexão.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
static bool BeginConnection(void) {
    if (ps_httpClient) {
        IF_TRUE_RETURN(!EndConnection(), false);
    }

    IF_TRUE_RETURN(!DRV_HttpClient_CreateInstance(&ps_httpClient, &config),
                   false);

    return true;
}
/******************************************************************************
 * @fn          ConfirmMagicCode()
 *
 * @brief       Confirma se o código mágico obtido da imagem corresponde ao
 * código da imagem de equipamento esperada para baixar ou não.
 * @return      Booleano - Status de retorno.
 *              @retval False - Imagem não corresponde ao equipamento esperado.
 *              @retval True - Imagem corresponde ao equipamento esperado.
 ******************************************************************************/
static bool ConfirmMagicCode(void) {
    uint64_t magicCode = 0ULL;
    memcpy((void *)&magicCode, (void *)p_response, sizeof(magicCode));

#if defined(ENABLE_VERBOSE_STM_FOTA_SERVICE)
    ESP_LOGI(__FILE__,
             "Código mágico obtido: %llu. Código esperado: %llu",
             magicCode,
             BOOTLOADER_OXYGENIS_UUID);
#endif

    return magicCode == BOOTLOADER_OXYGENIS_UUID;
}
/******************************************************************************
 * @fn          STM32HttpEventHandler()
 *
 * @brief       Manipulador interno para os eventos emitidos para cada operação
 * executada por meio do objeto do cliente.
 * @param       evt Pointeiro para a estrutura que armazena informações sobre
 * o evento ocorrido.
 ******************************************************************************/
static esp_err_t STM32HttpEventHandler(esp_http_client_event_t *evt) {
    static uint16_t outputLen = 0;

    IF_TRUE_RETURN(!evt, ESP_FAIL);

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            break;
        case HTTP_EVENT_ON_CONNECTED:
            outputLen = 0;
            break;
        case HTTP_EVENT_HEADER_SENT:
            break;
        case HTTP_EVENT_ON_HEADER:
            break;

        case HTTP_EVENT_ON_DATA:
            {
                if (outputLen == 0 && evt->user_data) {
                    memset(evt->user_data,
                           0,
                           SRV_STM_FOTA_DOWNLOAD_BUFFER_LENGTH);
                }

                bool responseInFrames =
                    esp_http_client_is_chunked_response(evt->client);

                if (!responseInFrames) {
                    int copy_len = 0;

                    if (evt->user_data) {
                        copy_len = MIN(
                            evt->data_len,
                            (SRV_STM_FOTA_DOWNLOAD_BUFFER_LENGTH - outputLen));
                        if (copy_len) {
                            memcpy(evt->user_data + outputLen,
                                   evt->data,
                                   copy_len);
                        }
                    } else {
                        int contentLen =
                            esp_http_client_get_content_length(evt->client);

                        copy_len =
                            MIN(evt->data_len, (contentLen - outputLen));

                        if (copy_len) {
                            memcpy(evt->user_data + outputLen,
                                   evt->data,
                                   copy_len);
                        }
                    }

                    outputLen += copy_len;
                }
                break;
            }

        case HTTP_EVENT_ON_FINISH:
            {
                // ESP_LOG_BUFFER_HEX("HEXA PRINT:", evt->user_data, outputLen);
                outputLen = 0;
                break;
            }

        case HTTP_EVENT_DISCONNECTED:
            {
                break;
            }

        case HTTP_EVENT_REDIRECT:
            break;
    }

    return ESP_OK;
}
/*****************************END OF FILE**************************************/