/* Includes ------------------------------------------------------------------*/

#include "Services/EspFotaService.h"

/* Private define ------------------------------------------------------------*/

/**
 * @brief Tempo limite para que o cliente possa obter uma respota após o envio
 * de uma requisição.
 */
#define CLIENT_REQUEST_TIMEOUT_MS 7000U

/**
 * @brief Obtém o semáforo.
 */
#define __TAKE_SEMAPHORE(semaphore) (TAKE_SEMAPHORE(semaphore, portMAX_DELAY))

/* Private typedef -----------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static esp_err_t HttpClientInitCallback(esp_http_client_handle_t http_client);
static void EspFotaEventHandler(void *arg,
                                esp_event_base_t event_base,
                                int32_t event_id,
                                void *event_data);

/* Private variables ---------------------------------------------------------*/

static esp_https_ota_handle_t httpOtaHandle = NULL;

static esp_http_client_config_t httpClientConfig = {
    .url               = ESP_URL_UPDATE_FIRMWARE,
    .timeout_ms        = CLIENT_REQUEST_TIMEOUT_MS,
    .auth_type         = HTTP_AUTH_TYPE_NONE,
    .method            = HTTP_METHOD_GET,
    .event_handler     = NULL,
    .keep_alive_enable = true,
    .keep_alive_count  = 5,  // Tentativas
    // .client_cert_pem             = (char *)NULL,
    // .cert_pem                    = (char *)"",
    .skip_cert_common_name_check = true,
    .crt_bundle_attach           = esp_crt_bundle_attach,
    // .transport_type              = HTTP_TRANSPORT_OVER_TCP,
    .buffer_size    = 4096,
    .buffer_size_tx = 4096,
};

static esp_https_ota_config_t otaConfig = {
    .http_config           = &httpClientConfig,
    .http_client_init_cb   = NULL,  // HttpClientInitCallback,
    .partial_http_download = true,
    // .max_http_request_size = 4 * 4096,
};

static bool alreadyRegister = false;

/* Public functions ----------------------------------------------------------*/

/******************************************************************************
 * @fn          SRV_EspFota_GetMutex()
 *
 * @brief       Obtém o Mutex do driver de requisições HTTP do cliente.
 * @return      Booleano - Status de retorno.
 *              @retval False - Não obtido.
 *              @retval True - Obtido com sucesso.
 ******************************************************************************/
const bool SRV_EspFota_GetMutex(void) {
    if (__TAKE_SEMAPHORE(xhttpClientDriverSync)) {
        return true;
    }

    return false;
}
/******************************************************************************
 * @fn          SRV_EspFota_GiveMutex()
 *
 * @brief       Libera o Mutex do driver de requisições HTTP do cliente.
 ******************************************************************************/
void SRV_EspFota_GiveMutex(void) { GIVE_SEMAPHORE(xhttpClientDriverSync); }
/******************************************************************************
 * @fn          SRV_EspFota_Begin()
 *
 * @brief       Inicializa o serviço do Fota do ESP.
 * @return      Booleano - Status de retorno.
 *              @retval False - Serviço não inicializado.
 *              @retval True - Serviço inicializado com sucesso.
 ******************************************************************************/
const bool SRV_EspFota_Begin(void) {
    if (!alreadyRegister) {
        if (esp_event_handler_register(ESP_HTTPS_OTA_EVENT,
                                       ESP_EVENT_ANY_ID,
                                       EspFotaEventHandler,
                                       NULL)
            == ESP_OK) {
            alreadyRegister = true;
        }
    }

    esp_err_t espStatusCode = ESP_FAIL;

    if (__TAKE_SEMAPHORE(xhttpClientDriverSync)) {
        espStatusCode = esp_https_ota_begin(&otaConfig, &httpOtaHandle);
        GIVE_SEMAPHORE(xhttpClientDriverSync);
    }

    return espStatusCode == ESP_OK;
}
/******************************************************************************
 * @fn          SRV_EspFota_GetTotalImageLength()
 *
 * @brief       Informa o tamanho total da imagem disponível.
 * @return      uint32_t - Valor de retorno.
 ******************************************************************************/
const uint32_t SRV_EspFota_GetTotalImageLength(void) {
    int totalImageLength = 0;

    if (__TAKE_SEMAPHORE(xhttpClientDriverSync)) {
        totalImageLength = esp_https_ota_get_image_size(httpOtaHandle);
        GIVE_SEMAPHORE(xhttpClientDriverSync);
    }

    IF_TRUE_RETURN(totalImageLength <= 0, 0);
    return (uint32_t)totalImageLength;
}
/******************************************************************************
 * @fn          SRV_EspFota_GetCurrentImageLengthDownload()
 *
 * @brief       Informa o tamanho atual já baixado da imagem.
 * @return      uint32_t - Valor de retorno.
 ******************************************************************************/
const uint32_t SRV_EspFota_GetCurrentImageLengthDownload(void) {
    int currentImageLength = 0;

    if (__TAKE_SEMAPHORE(xhttpClientDriverSync)) {
        currentImageLength = esp_https_ota_get_image_len_read(httpOtaHandle);
        GIVE_SEMAPHORE(xhttpClientDriverSync);
    }

    IF_TRUE_RETURN(currentImageLength <= 0, 0);
    return (uint32_t)currentImageLength;
}
/******************************************************************************
 * @fn          SRV_EspFota_GetRunningVersion()
 *
 * @brief       Obtém a versão atualmente em execução no Esp.
 * @param       p_runningVersion Ponteiro para vetor de cadeia de caracteres.
 * @return      E_AppStatusTypeDef - Status de retorno.
 *              @retval APP_INVALID_PARAMETER_ERROR - Um ou mais parâmetros
 *inválidos.
 *              @retval APP_GENERAL_ERROR - Erro durante tentativa de executar
 * uma operação.
 *              @retval APP_OK - Operação executada com sucesso.
 ******************************************************************************/
const E_AppStatusTypeDef SRV_EspFota_GetRunningVersion(
    char *const p_runningVersion) {
    IF_TRUE_RETURN(!p_runningVersion, APP_GENERAL_ERROR);

    const esp_partition_t *espPartitionRunning =
        esp_ota_get_running_partition();

    IF_TRUE_RETURN(!espPartitionRunning, APP_GENERAL_ERROR);

    esp_app_desc_t appRunningDescription;
    IF_TRUE_RETURN(esp_ota_get_partition_description(espPartitionRunning,
                                                     &appRunningDescription)
                       != ESP_OK,
                   APP_GENERAL_ERROR);

    strcpy((char *)p_runningVersion, (char *)appRunningDescription.version);

    return APP_OK;
}
/******************************************************************************
 * @fn          SRV_EspFota_CheckVersion()
 *
 * @brief       Verifica a versão disponível e se a mesma é uma versão mais
 * recente.
 * @param       p_isNewVersion Ponteiro para variável cujo status será
 *preenchido.
 * @param       p_versionAvailable Ponteiro para cadeia de caracteres que será
 * preenchida com a versão disponível em nuvem
 * @return      E_AppStatusTypeDef - Status de retorno.
 *              @retval APP_INVALID_PARAMETER_ERROR - Um ou mais parâmetros
 *inválidos.
 *              @retval APP_GENERAL_ERROR - Erro durante tentativa de executar
 * uma operação.
 *              @retval APP_OK - Operação executada com sucesso.
 ******************************************************************************/
const E_AppStatusTypeDef SRV_EspFota_CheckVersion(
    bool *const p_isNewVersion,
    char *const p_versionAvailable) {
    IF_TRUE_RETURN(!p_isNewVersion || !p_versionAvailable,
                   APP_INVALID_PARAMETER_ERROR);

    esp_app_desc_t imageAppDescription;

    IF_TRUE_RETURN(
        esp_https_ota_get_img_desc(httpOtaHandle, &imageAppDescription)
            != ESP_OK,
        APP_GENERAL_ERROR);

    const esp_partition_t *espPartitionRunning =
        esp_ota_get_running_partition();

    IF_TRUE_RETURN(!espPartitionRunning, APP_GENERAL_ERROR);

    esp_app_desc_t appRunningDescription;
    IF_TRUE_RETURN(esp_ota_get_partition_description(espPartitionRunning,
                                                     &appRunningDescription)
                       != ESP_OK,
                   APP_GENERAL_ERROR);

    if (p_versionAvailable) {
        strcpy(p_versionAvailable, imageAppDescription.version);
    }

    const esp_partition_t *lastInvalidPartition =
        esp_ota_get_last_invalid_partition();

    if (lastInvalidPartition) {
        esp_app_desc_t appLastInvalidPartition;
        if (esp_ota_get_partition_description(lastInvalidPartition,
                                              &appLastInvalidPartition)
            == ESP_OK) {
            if (strcmp(imageAppDescription.version,
                       appLastInvalidPartition.version)
                == 0) {
                *p_isNewVersion = false;
                return APP_OK;
            }
        }
    }

    if (strcmp(imageAppDescription.version, appRunningDescription.version)
        <= 0) {
        *p_isNewVersion = false;
    } else {
        *p_isNewVersion = true;
    }

    return APP_OK;
}
/******************************************************************************
 * @fn          SRV_EspFota_DownloadPartialImage()
 *
 * @brief       Efetua o download parcial da imagem. Essa função deve ser
 *chamada constantemente.
 * @param       p_statusCode Status do código obtido.
 * @return      E_AppStatusTypeDef - Status de retorno.
 *              @retval APP_INVALID_PARAMETER_ERROR - Um ou mais parâmetros
 *inválidos.
 *              @retval APP_GENERAL_ERROR - Erro durante tentativa de executar
 * uma operação.
 *              @retval APP_IS_BUSY - Baixando imagem parcialmente.
 *              @retval APP_OK - Operação executada com sucesso.
 ******************************************************************************/
const E_AppStatusTypeDef SRV_EspFota_DownloadPartialImage(
    int *const p_statusCode) {
    esp_err_t espStatusCode = ESP_FAIL;

    if (__TAKE_SEMAPHORE(xhttpClientDriverSync)) {
        espStatusCode = esp_https_ota_perform(httpOtaHandle);
        GIVE_SEMAPHORE(xhttpClientDriverSync);
    }

    *p_statusCode = esp_https_ota_get_status_code(httpOtaHandle);

    IF_TRUE_RETURN(espStatusCode == ESP_ERR_HTTPS_OTA_IN_PROGRESS, APP_IS_BUSY);

    if (espStatusCode != ESP_OK) {
#if defined(ENABLE_VERBOSE_ESP_FOTA_SERVICE)
        ESP_LOGW(__FUNCTION__,
                 "Esp status: %s. StatusCode: %d",
                 esp_err_to_name(espStatusCode),
                 *p_statusCode);
#endif
    }

    return APP_OK;
}
/******************************************************************************
 * @fn          SRV_EspFota_IsDownloadImageCompleted()
 *
 * @brief       Informa se a imagem foi baixada por completo.
 * @return      Booleano - Status de retorno.
 *              @retval False - Ainda não foi baixada por completo.
 *              @retval True - Imagem baixada por completo.
 ******************************************************************************/
const bool SRV_EspFota_IsDownloadImageCompleted(void) {
    bool status = false;

    if (__TAKE_SEMAPHORE(xhttpClientDriverSync)) {
        status = esp_https_ota_is_complete_data_received(httpOtaHandle);
        GIVE_SEMAPHORE(xhttpClientDriverSync);
    }

    return status;
}
/******************************************************************************
 * @fn          SRV_EspFota_Finish()
 *
 * @brief       Finaliza o Fota do Esp.
 * @return      Booleano - Status de retorno.
 *              @retval False - Erro durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
const bool SRV_EspFota_Finish(void) {
    esp_err_t espStatusCode = ESP_FAIL;

    if (__TAKE_SEMAPHORE(xhttpClientDriverSync)) {
        espStatusCode = esp_https_ota_finish(httpOtaHandle);
        GIVE_SEMAPHORE(xhttpClientDriverSync);
    }

    if (alreadyRegister) {
        if (esp_event_handler_unregister(ESP_HTTPS_OTA_EVENT,
                                         ESP_EVENT_ANY_ID,
                                         EspFotaEventHandler)
            == ESP_OK) {
            alreadyRegister = false;
        }
    }

    IF_TRUE_RETURN(espStatusCode != ESP_OK, false);
    return true;
}
/******************************************************************************
 * @fn          SRV_EspFota_Abort()
 *
 * @brief       Aborta o download da imagem por completo.
 * @return      Booleano - Status de retorno.
 *              @retval False - Erro durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
const bool SRV_EspFota_Abort(void) {
    esp_err_t espStatusCode = ESP_FAIL;

    if (__TAKE_SEMAPHORE(xhttpClientDriverSync)) {
        espStatusCode = esp_https_ota_abort(httpOtaHandle);
        GIVE_SEMAPHORE(xhttpClientDriverSync);
    }

    if (alreadyRegister) {
        if (esp_event_handler_unregister(ESP_HTTPS_OTA_EVENT,
                                         ESP_EVENT_ANY_ID,
                                         EspFotaEventHandler)
            == ESP_OK) {
            alreadyRegister = false;
        }
    }

    IF_TRUE_RETURN(espStatusCode != ESP_OK, false);
    return true;
}
/******************************************************************************
 * @fn          SRV_EspFota_IsRequiredValidationImage()
 *
 * @brief       Informa se é requerido a validação da imagem, após o Esp ter
 * reiniciado, validando a imagem e evitando um rollback.
 * @return      Booleano - Status de retorno.
 *              @retval False - Erro durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
const bool SRV_EspFota_IsRequiredValidationImage(void) {
#if defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE)
    const esp_partition_t *ps_appPartition = esp_ota_get_running_partition();

    esp_ota_img_states_t espOtaImageState = {0};

    IF_TRUE_RETURN(
        esp_ota_get_state_partition(ps_appPartition, &espOtaImageState)
            != ESP_OK,
        false);

    IF_TRUE_RETURN(espOtaImageState != ESP_OTA_IMG_PENDING_VERIFY, false);

    return true;
#else
    return false;
#endif
}
/******************************************************************************
 * @fn          SRV_EspFota_SetIValidImageStatus()
 *
 * @brief       Define a imagem atual como uma válida, evitando um rollback.
 * @return      Booleano - Status de retorno.
 *              @retval False - Erro durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
const bool SRV_EspFota_SetIValidImageStatus(const bool status) {
#if defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE)
    if (status) {
        IF_TRUE_RETURN(esp_ota_mark_app_valid_cancel_rollback() != ESP_OK,
                       false);
    } else {
        IF_TRUE_RETURN(esp_ota_mark_app_invalid_rollback_and_reboot() != ESP_OK,
                       false);
    }
#endif
    return true;
}

/* Private functions ---------------------------------------------------------*/

/******************************************************************************
 * @fn          HttpClientInitCallback()
 *
 * @brief       Evento gerado durante inicialização do cliente do Fota do ESP.
 ******************************************************************************/
static esp_err_t HttpClientInitCallback(esp_http_client_handle_t http_client) {
    esp_err_t err = ESP_OK;
    /* Uncomment to add custom headers to HTTP request */
    // err = esp_http_client_set_header(http_client,
    //                                  "Content-Type",
    //                                  "application/octet-stream");
    return err;
}
/******************************************************************************
 * @fn          EspFotaEventHandler()
 *
 * @brief       Evento gerado pelas transações do Fota do ESP.
 ******************************************************************************/
static void EspFotaEventHandler(void *arg,
                                esp_event_base_t event_base,
                                int32_t event_id,
                                void *event_data) {
    if (event_base == ESP_HTTPS_OTA_EVENT) {
        switch (event_id) {
            case ESP_HTTPS_OTA_START:
                {
#if defined(ENABLE_VERBOSE_ESP_FOTA_SERVICE)
                    ESP_LOGI(__FILE__, "OTA inicializado!");
#endif
                    break;
                }

            case ESP_HTTPS_OTA_CONNECTED:
                {
#if defined(ENABLE_VERBOSE_ESP_FOTA_SERVICE)
                    ESP_LOGI(__FILE__, "OTA conectado ao servidor.");
#endif
                    break;
                }

            case ESP_HTTPS_OTA_GET_IMG_DESC:
                {
#if defined(ENABLE_VERBOSE_ESP_FOTA_SERVICE)
                    ESP_LOGI(__FILE__, "Lendo descripção da imagem...");
#endif
                    break;
                }

            case ESP_HTTPS_OTA_VERIFY_CHIP_ID:
                {
#if defined(ENABLE_VERBOSE_ESP_FOTA_SERVICE)
                    ESP_LOGI(__FILE__,
                             "Verificando ID do chip da nova imagem: %d",
                             *(esp_chip_id_t *)event_data);
#endif
                    break;
                }

            case ESP_HTTPS_OTA_DECRYPT_CB:
                {
#if defined(ENABLE_VERBOSE_ESP_FOTA_SERVICE)
                    ESP_LOGI(__FILE__, "Callback to decrypt function");
#endif
                    break;
                }

            case ESP_HTTPS_OTA_WRITE_FLASH:
                {
#if defined(ENABLE_VERBOSE_ESP_FOTA_SERVICE)
                    ESP_LOGD(__FILE__,
                             "Escrevendo na flash: %d",
                             *(int *)event_data);
#endif
                    break;
                }

            case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
                {
#if defined(ENABLE_VERBOSE_ESP_FOTA_SERVICE)
                    ESP_LOGI(__FILE__,
                             "Partição de inicialização atualizada. Próxima "
                             "partição: %d",
                             *(esp_partition_subtype_t *)event_data);
#endif
                    break;
                }

            case ESP_HTTPS_OTA_FINISH:
                {
#if defined(ENABLE_VERBOSE_ESP_FOTA_SERVICE)
                    ESP_LOGI(__FILE__, "OTA finalizado com sucesso!");
#endif
                    break;
                }

            case ESP_HTTPS_OTA_ABORT:
                {
#if defined(ENABLE_VERBOSE_ESP_FOTA_SERVICE)
                    ESP_LOGI(__FILE__, "OTA Abortado!");
#endif
                    break;
                }
        }
    }
}
/*****************************END OF FILE**************************************/