/* Includes ------------------------------------------------------------------*/

#include "Services/HttpFileUploadService.h"

/* Private define ------------------------------------------------------------*/

/**
 * @brief Apelido utilizado para identificar as saídas de logs do ESP.
 */
#define TAG __FILE__

/**
 * @brief Delimitador para requisições multipart/form-data.
 */
#define HTTP_BOUNDARY "X-ESPIDF_MULTIPART"

/* Private typedef -----------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static bool AddBasicHeaders(esp_http_client_handle_t client);

static bool SendFile(esp_http_client_handle_t client,
                     const char *const p_fileName,
                     const uint8_t *const p_blockData,
                     const uint16_t blockLen);

/* Private variables ---------------------------------------------------------*/
/* Public functions ----------------------------------------------------------*/

/******************************************************************************
 * @fn          SRV_HttpFileUpload_Send()
 *
 * @brief       Realiza o envio em partes do arquivo.
 * @param       client Objetvo de conexão do cliente.
 * @param       p_fileName Ponteiro para o nome do arquivo.
 * @param       p_blockData Ponteiro para vetor com os dados a serem enviados.
 * @param       blockLen Tamanho do vetor, em bytes.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool SRV_HttpFileUpload_Send(esp_http_client_handle_t client,
                             const char *const p_fileName,
                             const uint8_t *const p_blockData,
                             const uint16_t blockLen) {
    if (esp_http_client_set_method(client, HTTP_METHOD_POST) != ESP_OK) {
        return false;
    }

    IF_TRUE_RETURN(!AddBasicHeaders(client), false);

    return SendFile(client, p_fileName, p_blockData, blockLen);
}

/* Private functions ---------------------------------------------------------*/

/******************************************************************************
 * @fn          AddBasicHeaders()
 *
 * @brief       Adiciona cabeçalhos específicos.
 * @param       client Objetvo de conexão do cliente.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
static bool AddBasicHeaders(esp_http_client_handle_t client) {
    esp_app_desc_t *ps_appDescription = esp_app_get_description();

    char text[UINT8_MAX / 4] = {[0 ...(GET_SIZE_ARRAY(text) - 1)] = 0};

    if (esp_http_client_set_header(client, "Accept", "*/*") != ESP_OK) {
        goto setHeaderFail;
    }

    snprintf(text, sizeof(text), "esp-idf/%s", ps_appDescription->version);
    if (esp_http_client_set_header(client, "User-Agent", text) != ESP_OK) {
        goto setHeaderFail;
    }

    snprintf(text,
             sizeof(text),
             "multipart/form-data; boundary=%s",
             HTTP_BOUNDARY);

    if (esp_http_client_set_header(client, "Content-Type", text) != ESP_OK) {
        goto setHeaderFail;
    }

    if (esp_http_client_set_header(client,
                                   "Accept-Encoding",
                                   "gzip, deflate, br")
        != ESP_OK) {
        goto setHeaderFail;
    }

    if (esp_http_client_set_header(client, "Accept-Charset", "ISO-8859-1,utf-8")
        != ESP_OK) {
        goto setHeaderFail;
    }

    if (!SRV_HttpRequest_SetApplicationHeader(client)) {
        goto setHeaderFail;
    }

    if (!SRV_HttpRequest_SetAuthorizationHeader(client)) {
        goto setHeaderFail;
    }

    if (!SRV_HttpRequest_SetUserTokenHeader(client)) {
        goto setHeaderFail;
    }

    if (!SRV_HttpRequest_SetLocaleHeader(client)) {
        goto setHeaderFail;
    }

    return true;

setHeaderFail:
    { return false; }
}
/******************************************************************************
 * @fn          AddBasicHeaders()
 *
 * @brief       Envia o arquivo em partes.
 * @param       client Objetvo de conexão do cliente.
 * @param       p_fileName Ponteiro para o vetor contendo o nome do arquivo.
 * @param       p_blockData Ponteiro para o vetor com os dados a serem enviados.
 * @param       blockLen Tamanho do vetor, em bytes.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
static bool SendFile(esp_http_client_handle_t client,
                     const char *const p_fileName,
                     const uint8_t *const p_blockData,
                     const uint16_t blockLen) {
    // request header
    char BODY[UINT8_MAX]    = {[0 ...(GET_SIZE_ARRAY(BODY) - 1)] = 0};
    char END[UINT8_MAX / 8] = {[0 ...(GET_SIZE_ARRAY(END) - 1)] = 0};

    snprintf(
        BODY,
        sizeof(BODY),
        "--%s\r\nContent-Disposition: form-data; name=\"file\"; "
        "filename=\"%s\"\r\nContent-Type: application/octet-stream\r\n\r\n",
        HTTP_BOUNDARY,
        p_fileName);

    snprintf(END, sizeof(END), "\r\n--%s--\r\n\r\n", HTTP_BOUNDARY);

// BODY and END
#if defined(ENABLE_VERBOSE_HTTP_FILE_UPLOAD_SERVICE)
    ESP_LOGI(TAG, "BODY: %s", BODY);
    ESP_LOGI(TAG, "END: %s", END);
#endif

    // Set Content-Length
    int contentLength = strlen(BODY) + strlen(END) + blockLen;

    {
        char contentLengthText[UINT8_MAX / 20] = {
            [0 ...(GET_SIZE_ARRAY(contentLengthText) - 1)] = 0};

        snprintf(contentLengthText,
                 sizeof(contentLengthText),
                 "%d",
                 contentLength);
#if defined(ENABLE_VERBOSE_HTTP_FILE_UPLOAD_SERVICE)
        ESP_LOGI(TAG, "Content-Length: %s", contentLengthText);
#endif

        if (esp_http_client_set_header(client,
                                       "Content-Length",
                                       contentLengthText)
            != ESP_OK) {
            goto sendFail;
        }
    }

    int quantity            = 0;
    esp_err_t espStatusCode = ESP_FAIL;

#if defined(ENABLE_VERBOSE_HTTP_FILE_UPLOAD_SERVICE)
    ESP_LOGI(TAG, "Enviando arquivo: %s...", p_fileName);
#endif

    if ((espStatusCode = esp_http_client_open(client, contentLength))
        != ESP_OK) {
#if defined(ENABLE_VERBOSE_HTTP_FILE_UPLOAD_SERVICE)
        ESP_LOGE(TAG,
                 "Falha ao abrir conexão! Erro: %s",
                 esp_err_to_name(espStatusCode));
#endif
        goto sendFail;
    }

#if defined(ENABLE_VERBOSE_HTTP_FILE_UPLOAD_SERVICE)
    ESP_LOGI(TAG, "Conexão aberta!");
#endif

    if ((quantity = esp_http_client_write(client, BODY, strlen(BODY))) <= 0) {
#if defined(ENABLE_VERBOSE_HTTP_FILE_UPLOAD_SERVICE)
        ESP_LOGE(TAG,
                 "Corpo não enviado por completo. %d de %d bytes",
                 quantity,
                 sizeof(BODY));
#endif
        goto sendFail;
    }

#if defined(ENABLE_VERBOSE_HTTP_FILE_UPLOAD_SERVICE)
    ESP_LOGI(TAG, "Corpo enviado!");
#endif

    uint16_t offset          = 0;
    const int TX_SIZE_BUFFER = SRV_HttpRequest_GetTxSize();

    while (offset < blockLen) {
        uint32_t sendTimer   = UTILS_Timer_Now();
        uint16_t bytesToSend = MIN(TX_SIZE_BUFFER, blockLen - offset);

        if ((quantity = esp_http_client_write(client,
                                              (char *)(p_blockData + offset),
                                              bytesToSend))
            <= 0) {
#if defined(ENABLE_VERBOSE_HTTP_FILE_UPLOAD_SERVICE)
            ESP_LOGE(TAG,
                     "Falha ao enviar parte do arquivo: %d de %d bytes",
                     quantity,
                     bytesToSend);
#endif
            goto sendFail;
        }

        offset += bytesToSend;

        time_t now;
        time(&now);

        uint32_t elapsedTime = UTILS_Timer_GetElapsedTime(sendTimer);
        elapsedTime          = elapsedTime == 0 ? 1 : elapsedTime;

        float sendPeriod = ((float)offset / 1024.0f) / (float)elapsedTime;

#if defined(ENABLE_VERBOSE_HTTP_FILE_UPLOAD_SERVICE)
        ESP_LOGI(TAG,
                 "Bytes enviados:%u/%u... (%.02f KB/ms)",
                 offset,
                 blockLen,
                 sendPeriod);
#endif
    }

    if ((quantity = esp_http_client_write(client, END, strlen(END))) <= 0) {
#if defined(ENABLE_VERBOSE_HTTP_FILE_UPLOAD_SERVICE)
        ESP_LOGE(TAG,
                 "Pré-âmbulo final não enviado por completo. %d de %d bytes",
                 quantity,
                 sizeof(END));
#endif
        goto sendFail;
    }

#if defined(ENABLE_VERBOSE_HTTP_FILE_UPLOAD_SERVICE)
    ESP_LOGI(TAG, "Prê-âmbulo final enviado!");
#endif

    // Get response
    if ((quantity = esp_http_client_fetch_headers(client)) <= 0) {
#if defined(ENABLE_VERBOSE_HTTP_FILE_UPLOAD_SERVICE)
        ESP_LOGE(TAG, "Sem cabeçalhos de resposta para ler...");
#endif
    } else {
#if defined(ENABLE_VERBOSE_HTTP_FILE_UPLOAD_SERVICE)
        ESP_LOGI(TAG, "Tamanho total do cabeçalho da resposta: %d", quantity);
#endif
    }

    int64_t responseLength = esp_http_client_get_content_length(client);

    if (responseLength) {
        esp_http_client_flush_response(client, &responseLength);
    }

    int statusCode = esp_http_client_get_status_code(client);

#if defined(ENABLE_VERBOSE_HTTP_FILE_UPLOAD_SERVICE)
    ESP_LOGI(TAG, "Status code:\t%d", statusCode);
#endif

    return statusCode < 400;

sendFail:
    { return false; }
}
/*****************************END OF FILE**************************************/