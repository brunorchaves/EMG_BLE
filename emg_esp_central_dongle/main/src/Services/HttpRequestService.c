/******************************************************************************
 * @file         : HttpRequestService.c
 * @authors      : Thales Mol Vinhal
 * @copyright    : Copyright (c) 2023 CMOS Drake
 * @brief        : Serviço para provisão de Requisições Http
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/

#include "Services/HttpRequestService.h"

/* Private define ------------------------------------------------------------*/

#define TAG "HTTP_CLIENT_SERVICE"

#define MAX_REQUEST_FAILS_COUNT 10

#define HTTP_RESPONSE_BUFFER_LENGTH 8192UL

#if defined(ENABLE_VERBOSE_HTTP_REQUEST_SERVICE)
/**
 * @brief Habilita a exibição de comandos 'printf()'.
 */
// #define ENABLE_PRINT_VERBOSE

// #define ENABLE_VERBOSE_CONFIGR_LOGIN

// #define ENABLE_VERBOSE_CORE_LOGIN

// #define ENABLE_VERBOSE_EXTRACT_CORE_TOKEN

    #define ENABLE_VERBOSE_UPDATE_THERAPY_DEVICE

    #define ENABLE_VERBOSE_UPDATE_DEVICE_TRACK_POSITION

    #define ENABLE_VERBOSE_HTTP_EXECUTE
/**
 * @brief Habilita a exibição da resposta HTTP obtida.
 */
// #define ENABLE_PRINT_HTTP_RESPONSE_DATA
#endif

/**
 * @brief Obtém o semáforo.
 */
#define __TAKE_SEMAPHORE(semaphore) (TAKE_SEMAPHORE(semaphore, portMAX_DELAY))

// Descomente esta macro caso queira ver prints informativos na tela
// #define VERBOSE_LEVEL_MAX

/* Private typedef -----------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static void ResetRequestFails(void);
static void IncrementRequestFails(void);

static char *const ExtractConfigRToken(const char *const p_data,
                                       uint16_t *const p_tokenLength);

static char *const ExtractCoreToken(const char *const p_data,
                                    uint16_t *const p_tokenLength);

static E_AppStatusTypeDef ExtractEquipmentParameterSettingsValues(
    uint64_t serialNumber,
    S_TherapyParameteresConfigurationTypeDef *ps_therapyParameters,
    const uint8_t therapyLength);

static void SaveValues(
    char *const configuredValueString,
    char *const externalParameterName,
    char *const lastUpdatedByString,
    S_TherapyParameteresConfigurationTypeDef *ps_therapyParameters,
    const uint8_t therapyLength);

static bool SRV_HttpRequest_Execute(const char *const p_url,
                                    const char *const p_headers,
                                    const char *const p_body,
                                    const bool useConfigRToken,
                                    const bool useCoreToken,
                                    const esp_http_client_method_t method);

static bool CheckSuccessfulResponse(void);
static esp_err_t HttpRequestEventHandler(esp_http_client_event_t *p_evt);

/* Private variables ---------------------------------------------------------*/

static char url[256]      = {[0 ...(GET_SIZE_ARRAY(url) - 1)] = 0};
static char body[4096]    = {[0 ...(GET_SIZE_ARRAY(body) - 1)] = 0};
static char headers[1024] = {[0 ...(GET_SIZE_ARRAY(headers) - 1)] = 0};

static bool settedCoreToken = false;
static char coreToken[512]  = {[0 ...(GET_SIZE_ARRAY(coreToken) - 1)] = 0};

static bool settedConfigRToken = false;
static char configrToken[512] = {[0 ...(GET_SIZE_ARRAY(configrToken) - 1)] = 0};

static volatile uint8_t httpRequestFailsCount = 0;

static volatile uint16_t totalContentLength = 0;
static bool lastRequestWasSucessfull        = false;

static char *const volatile httpResponseBuffer = NULL;

static const ApiCredentialsTypeDef *const volatile ps_apiCredentials = NULL;

static esp_http_client_handle_t ps_httpClient = NULL;
static esp_http_client_config_t clientConfig  = {
     .url            = NULL,
     .event_handler  = HttpRequestEventHandler,
     .buffer_size_tx = 4096,
     .timeout_ms     = 7000,
};

static SemaphoreHandle_t xHttpRequestSemaphore = NULL;

/* Public functions ----------------------------------------------------------*/

/******************************************************************************
 * @fn          DRV_HttpClient_Init()
 *
 * @brief       Inicializa o driver.
 ******************************************************************************/
void SRV_HttpRequest_Init(void) {
    if (!DRV_HttpClient_IsInitializated()) {
        DRV_HttpClient_Init();
    }

    IF_TRUE_RETURN(xHttpRequestSemaphore);

    xHttpRequestSemaphore = xSemaphoreCreateBinary();
    configASSERT(xHttpRequestSemaphore);

    if (xHttpRequestSemaphore) {
        GIVE_SEMAPHORE(xHttpRequestSemaphore);
    }

    size_t *ps_aux = (size_t *)&httpResponseBuffer;
    *ps_aux        = (size_t)pvPortMalloc(HTTP_RESPONSE_BUFFER_LENGTH
                                   * sizeof(*httpResponseBuffer));
    configASSERT(httpResponseBuffer);

    if (!ps_apiCredentials) {
        ps_aux  = (size_t *)&ps_apiCredentials;
        *ps_aux = (size_t)App_AppData_GetApiCredentials();
    }
}
/******************************************************************************
 * @fn          SRV_HttpRequest_ExecuteConfigRLogin()
 *
 * @brief       Executa a autenticação do ConfigR.
 * @param       p_secreyKey Ponteiro para a chave de segurança criptografada.
 * @param       p_id Ponteiro para o identificador aleatório.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool SRV_HttpRequest_ExecuteConfigRLogin(const char *const p_secreyKey,
                                         const char *const p_id) {
    if (!__TAKE_SEMAPHORE(xHttpRequestSemaphore)) {
        return false;
    }

    snprintf(url,
             sizeof(url),
             "%s/authentication/getApiToken/%s",
             ps_apiCredentials->configrUrl,
             p_id);

    snprintf(body,
             sizeof(body),
             "{\"clientId\":\"%s\",\"secretKey\":\"%s\"}",
             ps_apiCredentials->cpapClientId,
             p_secreyKey);

    snprintf(headers,
             sizeof(headers),
             "%s",
             ps_apiCredentials->cpapApplicationKey);

#if defined(ENABLE_VERBOSE_CONFIGR_LOGIN)
    printf("%s: URL: %s\r\n", TAG, url);
    printf("%s: BODY: %s\r\n", TAG, body);
    printf("%s: HEADERS: %s\r\n", TAG, headers);
#endif

    bool requestStatus = SRV_HttpRequest_Execute(url,
                                                 headers,
                                                 body,
                                                 false,
                                                 false,
                                                 HTTP_METHOD_POST);

    if (!requestStatus) {
        IncrementRequestFails();
        GIVE_SEMAPHORE(xHttpRequestSemaphore);
        return false;
    }

    uint16_t configRTokenLength = 0;
    char *p_token =
        ExtractConfigRToken(httpResponseBuffer, &configRTokenLength);

    if (configRTokenLength && p_token) {
        snprintf(configrToken, sizeof(configrToken), "Bearer %s", p_token);
        printf("ConfigR Token: %s\r\n", configrToken);
        settedConfigRToken = true;
    }

    if (p_token) {
        vPortFree((void *)p_token);
        p_token = NULL;
    }

    ResetRequestFails();

    GIVE_SEMAPHORE(xHttpRequestSemaphore);
    return settedConfigRToken;
}
/******************************************************************************
 * @fn          SRV_HttpRequest_ExecuteCoreLogin()
 *
 * @brief       Executa a autenticação do Core.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool SRV_HttpRequest_ExecuteCoreLogin(void) {
    if (!settedConfigRToken) {
#if defined(ENABLE_VERBOSE_CORE_LOGIN)
        ESP_LOGI(TAG, "CoreToken não foi obtido!");
#endif
        return false;
    }

    if (!__TAKE_SEMAPHORE(xHttpRequestSemaphore)) {
        return false;
    }

    snprintf(url, sizeof(url), "%s/auth/login", ps_apiCredentials->coreUrl);

    snprintf(body,
             sizeof(body),
             "{\"username\":\"%s\",\"password\":\"%s\"}",
             ps_apiCredentials->cpapClientId,
             ps_apiCredentials->password);

    snprintf(headers, sizeof(headers), ps_apiCredentials->coreApplicationKey);

#if defined(ENABLE_VERBOSE_CORE_LOGIN)
    printf("%s: URL: %s\r\n", TAG, url);
    printf("%s: BODY: %s\r\n", TAG, body);
    printf("%s: HEADERS: %s\r\n", TAG, headers);
#endif

    bool requestStatus = SRV_HttpRequest_Execute(url,
                                                 headers,
                                                 body,
                                                 true,
                                                 false,
                                                 HTTP_METHOD_POST);

    bool responseStatus = CheckSuccessfulResponse();

    if (!requestStatus || !responseStatus) {
        IncrementRequestFails();

        GIVE_SEMAPHORE(xHttpRequestSemaphore);
        return false;
    }

    uint16_t coreTokenLength = 0;
    char *p_token = ExtractCoreToken(httpResponseBuffer, &coreTokenLength);

    if (coreTokenLength && p_token) {
        memcpy(coreToken, p_token, coreTokenLength);
        printf("Core Token: %s\r\n", p_token);
        settedCoreToken = true;
    }

    if (p_token) {
        vPortFree((void *)p_token);
        p_token = NULL;
    }

    ResetRequestFails();

    GIVE_SEMAPHORE(xHttpRequestSemaphore);
    return settedCoreToken;
}
/******************************************************************************
 * @fn          SRV_HttpRequest_SetApplicationHeader()
 *
 * @brief       Define a chave e o valor do cabeçalho "application".
 * @param       client Objetvo de conexão do cliente.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool SRV_HttpRequest_SetApplicationHeader(esp_http_client_handle_t client) {
    esp_err_t espStatusCode =
        esp_http_client_set_header(client,
                                   "application",
                                   ps_apiCredentials->coreApplicationKey);
    return espStatusCode == ESP_OK;
}
/******************************************************************************
 * @fn          SRV_HttpRequest_SetAuthorizationHeader()
 *
 * @brief       Define a chave e o valor do cabeçalho "authorization".
 * @param       client Objetvo de conexão do cliente.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool SRV_HttpRequest_SetAuthorizationHeader(esp_http_client_handle_t client) {
    esp_err_t espStatusCode =
        esp_http_client_set_header(client, "authorization", configrToken);
    return espStatusCode == ESP_OK;
}
/******************************************************************************
 * @fn          SRV_HttpRequest_SetUserTokenHeader()
 *
 * @brief       Define a chave e o valor do cabeçalho "usertoken".
 * @param       client Objetvo de conexão do cliente.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool SRV_HttpRequest_SetUserTokenHeader(esp_http_client_handle_t client) {
    esp_err_t espStatusCode =
        esp_http_client_set_header(client, "usertoken", coreToken);
    return espStatusCode == ESP_OK;
}
/******************************************************************************
 * @fn          SRV_HttpRequest_SetLocaleHeader()
 *
 * @brief       Define a chave e o valor do cabeçalho "locale".
 * @param       client Objetvo de conexão do cliente.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool SRV_HttpRequest_SetLocaleHeader(esp_http_client_handle_t client) {
    esp_err_t espStatusCode =
        esp_http_client_set_header(client, "locale", "BR");
    return espStatusCode == ESP_OK;
}
/******************************************************************************
 * @fn          SRV_HttpRequest_GetTxSize()
 *
 * @brief       Informa o tamanho do vetor de transmissão de dados HTTP.
 * @return      int - Valor de retorno.
 ******************************************************************************/
int SRV_HttpRequest_GetTxSize(void) { return clientConfig.buffer_size_tx; }
/******************************************************************************
 * @fn          SRV_HttpRequest_GetRxSize()
 *
 * @brief       Informa o tamanho do vetor de recepção de dados HTTP.
 * @return      int - Valor de retorno.
 ******************************************************************************/
int SRV_HttpRequest_GetRxSize(void) { return clientConfig.buffer_size; }
/******************************************************************************
 * @brief Função que faz a requisição Http POST para upload de arquivos de Log
 * no endpoint "common/uploadEquipmentFile"
 *
 * @param       p_fileName Ponteiro para o nome do arquivo.
 * @param       p_blockData Ponteiro para vetor com os dados a serem enviados.
 * @param       blockLen Tamanho do vetor, em bytes.
 ******************************************************************************/
bool SRV_HttpRequest_SendLogFile(const char *const p_fileName,
                                 const uint8_t *const p_blockData,
                                 const uint16_t blockLen) {
    if (!__TAKE_SEMAPHORE(xHttpRequestSemaphore)) {
        return false;
    }

    if (ps_httpClient) {
        if (!DRV_HttpClient_DestroyInstance(&ps_httpClient)) {
#if defined(ENABLE_VERBOSE_HTTP_REQUEST_SERVICE)
            ESP_LOGE(TAG, "Falha em %s", __func__);
#endif
            IncrementRequestFails();
            return false;
        }
    }

    snprintf(url,
             sizeof(url),
             "%s/common/uploadEquipmentFile",
             ps_apiCredentials->coreUrl);

    // Initialize the HTTP client
    clientConfig.url = url;

    if (!DRV_HttpClient_CreateInstance(&ps_httpClient, &clientConfig)) {
        IncrementRequestFails();
        return false;
    }

    bool status = SRV_HttpFileUpload_Send(ps_httpClient,
                                          p_fileName,
                                          p_blockData,
                                          blockLen);

    if (status) {
        status &= CheckSuccessfulResponse();
    }

    DRV_HttpClient_DestroyInstance(&ps_httpClient);
    GIVE_SEMAPHORE(xHttpRequestSemaphore);

    if (!status) {
        IncrementRequestFails();
    } else {
        ResetRequestFails();
    }

    return status;
}
/******************************************************************************
 * @fn          SRV_HttpRequest_GetEquipmentParameters()
 *
 * @brief       Obtém os parâmetros de configuração do equipamento, em nuvem.
 * @param       serialNumber Número serial do equipamento.
 * @param       ps_therapyParameters Ponteiro para vetor, contendo os valores
 * mais recentes dentro de cada estrutura.
 * @param       therapyLength Quantidade de objetos dentro da estrutura.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool SRV_HttpRequest_GetEquipmentParameters(
    const uint64_t serialNumber,
    S_TherapyParameteresConfigurationTypeDef *ps_therapyParameters,
    const uint8_t therapyLength) {
    if (!__TAKE_SEMAPHORE(xHttpRequestSemaphore)) {
        return false;
    }

    snprintf(url,
             sizeof(url),
             "%s/equipmentParameterSettings/firmware/getDeviceData/%llu",
             ps_apiCredentials->coreUrl,
             serialNumber);

    snprintf(headers, sizeof(headers), ps_apiCredentials->coreApplicationKey);

    bool requestStatus = SRV_HttpRequest_Execute(url,
                                                 headers,
                                                 NULL,
                                                 true,
                                                 true,
                                                 HTTP_METHOD_GET);

    bool responseStatus = CheckSuccessfulResponse();

    if (!requestStatus || !responseStatus) {
        IncrementRequestFails();

        GIVE_SEMAPHORE(xHttpRequestSemaphore);
        return false;
    }

    ResetRequestFails();

    ExtractEquipmentParameterSettingsValues(serialNumber,
                                            ps_therapyParameters,
                                            therapyLength);

    GIVE_SEMAPHORE(xHttpRequestSemaphore);
    return true;
}
/******************************************************************************
 * @fn          SRV_HttpRequest_UpdateTherapyDeviceParameters()
 *
 * @brief       Envia os dados de terapia para a plataforma.
 * @param       equipmentSerial Número serial do equipamento.
 * @param       ps_therapyParameters Ponteiro para vetor, contendo os valores
 * mais recentes dentro de cada estrutura.
 * @param       therapyLength Quantidade de objetos dentro da estrutura.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool SRV_HttpRequest_UpdateTherapyDeviceParameters(
    const uint64_t equipmentSerial,
    volatile S_TherapyParameteresConfigurationTypeDef *const ps_therapyBuffer,
    const uint8_t therapyLength) {
    if (!__TAKE_SEMAPHORE(xHttpRequestSemaphore)) {
        return false;
    }

    bool hasAnyParamToBeUpdated = false;

    for (uint8_t i = 0; i < therapyLength; i++) {
        if (ps_therapyBuffer[i].uploaded) {
            hasAnyParamToBeUpdated = true;
            break;
        }
    }

    if (!hasAnyParamToBeUpdated) {
        GIVE_SEMAPHORE(xHttpRequestSemaphore);
        return true;
    }

    char equipmentSerialString[UINT8_MAX / 12] = {
        [0 ...(GET_SIZE_ARRAY(equipmentSerialString) - 1)] = 0};

    snprintf(equipmentSerialString,
             sizeof(equipmentSerialString),
             "%llu",
             equipmentSerial);

    // Definição da URL
    snprintf(url,
             sizeof(url),
             "%s/equipmentParameterSettings/updateByFirmware",
             ps_apiCredentials->coreUrl);

    // Definição do header
    snprintf(headers, sizeof(headers), ps_apiCredentials->coreApplicationKey);

    int16_t therapyBackupValuesBuffer[therapyLength];
    memset((void *)therapyBackupValuesBuffer,
           -1,
           sizeof(therapyBackupValuesBuffer));

    // Obtém o timestamp
    uint64_t timestamp = SRV_NTP_GetUnixTimestamp();

    // Cria o objeto raiz
    cJSON *objectRoot = cJSON_CreateObject();

    if (!objectRoot) {
        goto failRequest;
    }

    // Cria o array "payload[]" e o adiciona ao objeto raiz
    cJSON *payload = cJSON_AddArrayToObject(objectRoot, "payload");

    if (!payload) {
        goto failRequest;
    }

    for (uint8_t i = 0; i < therapyLength; i++) {
        if (!ps_therapyBuffer[i].uploaded) {
            continue;
        }

        therapyBackupValuesBuffer[i] =
            (int16_t)ps_therapyBuffer[i].configuredValue;

        double value = therapyBackupValuesBuffer[i];

        // Cria o item de objeto
        cJSON *item = cJSON_CreateObject();

        if (!item) {
            continue;
        }

        // Adiciona o valor ao item de objeto
        if (!cJSON_AddStringToObject(item,
                                     "equipmentSerialNumber",
                                     equipmentSerialString)) {
            goto failRequest;
        }

        switch (ps_therapyBuffer[i].charId) {
            case DEV_INFO_THERAPY_PRESSURE_CHAR:
            case DEV_INFO_APAP_MIN_CHAR:
            case DEV_INFO_APAP_MAX_CHAR:
                {
                    value /= 10.0;
                    break;
                }
        }

        if (cJSON_AddNumberToObject(item, "configuredValue", value) == NULL) {
            goto failRequest;
        }

        // Adiciona o valor ao item de objeto
        if (!cJSON_AddStringToObject(
                item,
                "externalParameterName",
                ps_therapyBuffer[i].externalParameterName)) {
            goto failRequest;
        }

        if (cJSON_AddNumberToObject(item, "timestamp", timestamp) == NULL) {
            goto failRequest;
        }

        // Adiciona o item de objeto ao "payload[]"
        cJSON_AddItemToArray(payload, item);
    }

    if (!cJSON_PrintPreallocated(objectRoot, body, sizeof(body), false)) {
        goto failRequest;
    }

    if (objectRoot) {
        cJSON_Delete(objectRoot);
        objectRoot = NULL;
    }

#if defined(ENABLE_VERBOSE_UPDATE_THERAPY_DEVICE)
    printf("[%s] - BODY: (%d bytes). %s\r\n", __FUNCTION__, strlen(body), body);
#endif

    // // Execute the HTTP POST request with the specific URL
    bool requestStatus = SRV_HttpRequest_Execute(url,
                                                 headers,
                                                 body,
                                                 true,
                                                 true,
                                                 HTTP_METHOD_POST);

    bool responseStatus = CheckSuccessfulResponse();

    if (!requestStatus || !responseStatus) {
        goto failRequest;
    }

    for (uint8_t i = 0; i < sizeof(therapyBackupValuesBuffer); i++) {
        if (therapyBackupValuesBuffer[i] < 0) {
            continue;
        }

        if (ps_therapyBuffer[i].configuredValue
            == therapyBackupValuesBuffer[i]) {
            ps_therapyBuffer[i].uploaded = false;
        }
    }

    ResetRequestFails();

    GIVE_SEMAPHORE(xHttpRequestSemaphore);
    return true;

failRequest:
    {
#if defined(ENABLE_VERBOSE_UPDATE_THERAPY_DEVICE)
        ESP_LOGW(TAG,
                 "Falhou na concatenação dos dados ao atualizar os parâmetros "
                 "de terapia.");
#endif
        IncrementRequestFails();

        if (objectRoot) {
            cJSON_Delete(objectRoot);
        }

        GIVE_SEMAPHORE(xHttpRequestSemaphore);
        return false;
    }
}
/******************************************************************************
 * @fn          SRV_HttpRequest_UpdateDeviceTrackPosition()
 *
 * @brief       Envia os dados de terapia para a plataforma.
 * @param       equipmentSerial Número serial do equipamento.
 * @param       ps_trackPosition Ponteiro para estrutura, contendo os dados
 * da localização do dispositivo.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool SRV_HttpRequest_UpdateDeviceTrackPosition(
    const uint64_t equipmentSerial,
    S_TrackPositionTypeDef *const ps_trackPosition) {
    if (!__TAKE_SEMAPHORE(xHttpRequestSemaphore)) {
        return false;
    }

    char timestampString[32] = {[0 ...(GET_SIZE_ARRAY(timestampString) - 1)] =
                                    0};

    time_t timestamp = SRV_NTP_GetUnixTimestamp();
    SRV_NTP_FormatTimestamp(timestamp,
                            timestampString,
                            sizeof(timestampString));

    snprintf(url,
             sizeof(url),
             "%s/equipmentTrackPosition/create",
             ps_apiCredentials->coreUrl);

    // Fill the headers - application
    snprintf(headers, sizeof(headers), ps_apiCredentials->coreApplicationKey);

    // Fill the body with the required parts
    snprintf(body,
             sizeof(body),
             "{"
             "\"equipmentSerialNumber\":\"%llu\","
             "\"timestampAt\":%lld,"
             "\"trackPositionAt\":\"%s\","
             "\"latitude\":\"%f\","
             "\"longitude\":\"%f\","
             "\"altitude\":\"%f\","
             "\"trackPositionData\":"
             "{"
             "\"gpsTime\":\"%s\","
             "\"imprecision\":\"%f\","
             "\"numberOfSatellites\":\"%d\""
             "}"
             "}",
             equipmentSerial,
             timestamp,
             timestampString,
             ps_trackPosition->latitude,
             ps_trackPosition->longitude,
             ps_trackPosition->altitude,
             ps_trackPosition->time,
             ps_trackPosition->imprecision,
             ps_trackPosition->satellites);

#if defined(ENABLE_VERBOSE_UPDATE_DEVICE_TRACK_POSITION)
    printf("[%s] - BODY: (%d bytes). %s\r\n", __FUNCTION__, strlen(body), body);
#endif

    // Execute the HTTP POST request with the specific URL
    bool requestStatus = SRV_HttpRequest_Execute(url,
                                                 headers,
                                                 body,
                                                 true,
                                                 true,
                                                 HTTP_METHOD_POST);

    bool responseStatus = CheckSuccessfulResponse();

    if (!requestStatus || !responseStatus) {
        goto failRequest;
    }

    ResetRequestFails();

    GIVE_SEMAPHORE(xHttpRequestSemaphore);
    return true;

failRequest:
    {
        IncrementRequestFails();

        GIVE_SEMAPHORE(xHttpRequestSemaphore);
        return false;
    }
}

/******************************************************************************
 * @brief Função que faz a requisição Http GET no endpoint
 *"EquipmentController_findOneByExactSerialNumber"
 ******************************************************************************/
// bool SRV_HttpRequest_GetfindOneByExactSerialNumber(const uint64_t
// serialNumber,
//                                                    char *p_equipmentID) {
//     if (__TAKE_SEMAPHORE(xHttpRequestSemaphore)) {
//         uint16_t buffer_size = 48;

//         // Construct the specific URL for the desired endpoint
//         snprintf(url,
//                  sizeof(url),
//                  "%s/equipment/findOneByExactSerialNumber/%llu",
//                  ps_apiCredentials->coreUrl,
//                  serialNumber);

//         // Fill the headers - application
//         snprintf(headers,
//                  sizeof(headers),
//                  ps_apiCredentials->coreApplicationKey);

//         // Execute the HTTP GET request with the specific URL
//         SRV_HttpRequest_Execute(url,
//                                 headers,
//                                 NULL,
//                                 true,
//                                 true,
//                                 HTTP_METHOD_GET);

//         cJSON *root = cJSON_Parse(httpResponseBuffer);

//         if (root == NULL) {
// #if defined(ENABLE_VERBOSE_HTTP_REQUEST_SERVICE)
//             ESP_LOGE(TAG, "Error parsing JSON 1");
// #endif

//             GIVE_SEMAPHORE(xHttpRequestSemaphore);
//             return false;
//         }

//         cJSON *registers = cJSON_GetObjectItemCaseSensitive(root,
//         "registers");

//         if (registers == NULL || !cJSON_IsArray(registers)) {
// #if defined(ENABLE_VERBOSE_HTTP_REQUEST_SERVICE)
//             ESP_LOGE(TAG, "Error getting 'registers' array from JSON");
// #endif

//             cJSON_Delete(root);

//             GIVE_SEMAPHORE(xHttpRequestSemaphore);
//             return false;
//         }
//         int arraySize = cJSON_GetArraySize(registers);

//         for (int i = 0; i < arraySize; ++i) {
//             cJSON *registerItem = cJSON_GetArrayItem(registers, i);
//             cJSON *equipmentID =
//                 cJSON_GetObjectItemCaseSensitive(registerItem, "id");

//             if (equipmentID != NULL && cJSON_IsString(equipmentID)) {
// #if defined(ENABLE_PRINT_VERBOSE)
//                 printf("\n \n equipmentID Value found : %s \n \n ",
//                        equipmentID->valuestring);
// #endif
//                 strncpy(p_equipmentID, equipmentID->valuestring,
//                 buffer_size);
//             }
//         }
//         // Free the cJSON root object
//         cJSON_Delete(root);
//         GIVE_SEMAPHORE(xHttpRequestSemaphore);
//     }
//     return true;
// }

/* Private functions ---------------------------------------------------------*/

/******************************************************************************
 * @fn          ResetRequestFails()
 *
 * @brief       Redefine a quantidade de tentativas.
 ******************************************************************************/
static void ResetRequestFails(void) { httpRequestFailsCount = 0; }
/******************************************************************************
 * @fn          IncrementRequestFails()
 *
 * @brief       Incrementa a quantidade de tentativas, e se for maior que
 * 'MAX_REQUEST_FAILS_COUNT' reinicia o dispositivo.
 ******************************************************************************/
static void IncrementRequestFails(void) {
    httpRequestFailsCount = httpRequestFailsCount + 1;

    if (httpRequestFailsCount >= MAX_REQUEST_FAILS_COUNT) {
        esp_restart();
    }
}
/******************************************************************************
 * @fn          ExtractConfigRToken()
 *
 * @brief       Efetua a busca e extração do valor do "configRToken".
 * @param       p_data Vetor contendo a resposta obtida.
 * @param       p_tokenLength Ponteiro para variável cujo valor será preenchido
 * com o tamanho do token encontrado (Opcional).
 * @return      Cadeia de caracteres alocada dinamicamente.
 ******************************************************************************/
static char *const ExtractConfigRToken(const char *const p_data,
                                       uint16_t *const p_tokenLength) {
    const char *const LOOK_FOR_STANDARD = "\"token\":\"";
    const char *tokenStartPos           = strstr(p_data, LOOK_FOR_STANDARD);

    if (tokenStartPos == NULL) {
#if defined(ENABLE_VERBOSE_EXTRACT_CORE_TOKEN)
        ESP_LOGW(TAG, "Parte inicial do ConfigRToken não encontrado no JSON.");
#endif
        return NULL;
    }

    tokenStartPos += strlen("\"token\":\"");
    const char *tokenEndPos = strchr(tokenStartPos, '\"');

    if (tokenEndPos == NULL) {
#if defined(ENABLE_VERBOSE_EXTRACT_CORE_TOKEN)
        ESP_LOGW(TAG, "Parte final do ConfigRToken não encontrado no JSON.");
#endif
        return NULL;
    }

    uint16_t tokenLength = (uint16_t)(tokenEndPos - tokenStartPos);

    if (p_tokenLength) {
        *p_tokenLength = tokenLength;
    }

    return strndup(tokenStartPos, tokenEndPos - tokenStartPos);
}
/******************************************************************************
 * @fn          ExtractCoreToken()
 *
 * @brief       Efetua a busca e extração do valor do "coreToken".
 * @param       p_data Vetor contendo a resposta obtida.
 * @param       p_tokenLength Ponteiro para variável cujo valor será preenchido
 * com o tamanho do token encontrado (Opcional).
 * @return      Cadeia de caracteres alocada dinamicamente.
 ******************************************************************************/
static char *const ExtractCoreToken(const char *const p_data,
                                    uint16_t *const p_tokenLength) {
    const char *const LOOK_FOR_STANDARD = "\"usertoken\":\"";
    const char *tokenStartPos           = strstr(p_data, LOOK_FOR_STANDARD);

    if (tokenStartPos == NULL) {
#if defined(ENABLE_VERBOSE_EXTRACT_CORE_TOKEN)
        ESP_LOGW(TAG, "Parte inicial do Core Token não encontrado no JSON.");
#endif
        return NULL;
    }

    tokenStartPos += strlen("\"usertoken\":\"");
    const char *tokenEndPos = strchr(tokenStartPos, '\"');

    if (tokenEndPos == NULL) {
#if defined(ENABLE_VERBOSE_EXTRACT_CORE_TOKEN)
        ESP_LOGW(TAG, "Parte final do Core Token não encontrado no JSON.");
#endif
        return NULL;
    }

    uint16_t tokenLength = (uint16_t)(tokenEndPos - tokenStartPos);

    if (p_tokenLength) {
        *p_tokenLength = tokenLength;
    }

    return strndup(tokenStartPos, tokenEndPos - tokenStartPos);
}
/******************************************************************************
 * @brief Função responsável por extrair os valores de parâmetros recebidos
 * via requisição GetEquipamentParameterSettings
 *
 * @param serialNumber Número serial do equipamento a ser atualizado com os
 * parâmetros.
 *
 * @param ps_therapyParameters Ponteiro para o array de estruturas do tipo
 *S_TherapyParameteresConfigurationTypeDef que contem informaçãoes de cada
 *parâmetro
 *
 * @retval booleano que indica se a requisição é valida
 ******************************************************************************/
static E_AppStatusTypeDef ExtractEquipmentParameterSettingsValues(
    uint64_t serialNumber,
    S_TherapyParameteresConfigurationTypeDef *ps_therapyParameters,
    const uint8_t therapyLength) {
    char equipmentSerialString[UINT8_MAX / 12] = {
        [0 ...(GET_SIZE_ARRAY(equipmentSerialString) - 1)] = 0};

    snprintf(equipmentSerialString,
             sizeof(equipmentSerialString),
             "%llu",
             serialNumber);

    cJSON *root = cJSON_Parse(httpResponseBuffer);

    if (root == NULL) {
        ESP_LOGE(TAG, "Without device parameters JSON...");
        return APP_GENERAL_ERROR;
    }

    cJSON *registers = cJSON_GetObjectItemCaseSensitive(root, "registers");

    if (registers == NULL || !cJSON_IsArray(registers)) {
#if defined(ENABLE_VERBOSE_HTTP_REQUEST_SERVICE)
        ESP_LOGE(TAG, "Error to trying get 'registers' array from JSON.");
#endif
        goto extractFail;
    }

    // printf("Quantidade de parâmetros obtidos: %d\r\n",
    //        cJSON_GetArraySize(registers));

    const cJSON *registerItem = NULL;

    cJSON_ArrayForEach(registerItem, registers) {
        cJSON *equipmentSerialNumber =
            cJSON_GetObjectItemCaseSensitive(registerItem,
                                             "equipmentSerialNumber");

        if (equipmentSerialNumber == NULL
            || !cJSON_IsString(equipmentSerialNumber)) {
            continue;
        } else if (strcmp(equipmentSerialString,
                          equipmentSerialNumber->valuestring)
                   != 0) {
            continue;
        }

        // Declaração dos arrays.
        char configuredValueString[UINT8_MAX / 12] = {
            [0 ...(GET_SIZE_ARRAY(configuredValueString) - 1)] = 0};

        char externalParameterNameString[UINT8_MAX / 4] = {
            [0 ...(GET_SIZE_ARRAY(externalParameterNameString) - 1)] = 0};

        char lastUpdatedByString[UINT8_MAX / 8] = {
            [0 ...(GET_SIZE_ARRAY(lastUpdatedByString) - 1)] = 0};

        // Obtenção do nome do parâmetro externo
        cJSON *externalParameterName =
            cJSON_GetObjectItemCaseSensitive(registerItem,
                                             "externalParameterName");

        bool externalParameterMatched =
            externalParameterName != NULL
            && cJSON_IsString(externalParameterName);

        if (!externalParameterMatched) {
            continue;
        }

        strcpy(externalParameterNameString, externalParameterName->valuestring);

        // Obtenção do JSON do valor configurado
        cJSON *configuredValue =
            cJSON_GetObjectItemCaseSensitive(registerItem, "configuredValue");

        bool configValueMatched =
            configuredValue != NULL && cJSON_IsString(configuredValue);

        if (configValueMatched) {
            strcpy(configuredValueString, configuredValue->valuestring);
        }

        // Obtenção do JSON com relação a quem atualizou por último
        cJSON *lastUpdatedBy =
            cJSON_GetObjectItemCaseSensitive(registerItem, "lastUpdatedBy");

        // Verificação dos JSONs
        bool lastUpdatedByMatched =
            lastUpdatedBy != NULL && cJSON_IsString(lastUpdatedBy);

        if (lastUpdatedByMatched) {
            strcpy(lastUpdatedByString, lastUpdatedBy->valuestring);
        }

        SaveValues(configuredValueString,
                   externalParameterNameString,
                   lastUpdatedByString,
                   ps_therapyParameters,
                   therapyLength);
    }

    cJSON_Delete(root);
    return APP_OK;

extractFail:
    {
        cJSON_Delete(root);
        return APP_GENERAL_ERROR;
    }
}
/******************************************************************************
 * @brief Função responsável por salvar parâmetros da plataforma na estrutura
 * ps_therapyParameters
 *
 * @param configuredValue string com o valor do parâmetro a ser configurado
 *
 * @param externalParameterName string com o nome do parâmetro a ser configurado
 *
 * @param ps_therapyParameters ponteiro para estrutura de parâmetros
 ******************************************************************************/
static void SaveValues(
    char *const configuredValueString,
    char *const externalParameterName,
    char *const lastUpdatedByString,
    S_TherapyParameteresConfigurationTypeDef *ps_therapyParameters,
    const uint8_t therapyLength) {
    uint8_t therapyPos      = 0;
    bool foundParameterName = false;

    for (uint8_t i = 0; i < therapyLength; i++) {
        if (strcmp(externalParameterName,
                   ps_therapyParameters[i].externalParameterName)
            == 0) {
            foundParameterName = true;
            therapyPos         = i;
            break;
        }
    }

    IF_TRUE_RETURN(!foundParameterName);

    if (ps_therapyParameters[therapyPos].uploaded) {
        bool matched = strcmp(ps_therapyParameters[therapyPos].lastUpdatedBy,
                              UPDATED_BY_FW)
                       == 0;
        IF_TRUE_RETURN(matched);
    }

    snprintf(ps_therapyParameters[therapyPos].lastUpdatedBy,
             sizeof(ps_therapyParameters[therapyPos].lastUpdatedBy),
             "%s",
             lastUpdatedByString);

    switch (ps_therapyParameters[therapyPos].charId) {
        case DEV_INFO_HUMIDIFIER_LEVEL_CHAR:
        case DEV_INFO_RAMP_TIME_CHAR:
        case DEV_INFO_OUT_RELIEF_LEVEL_CHAR:
            {
                uint8_t value = atoi(configuredValueString);
                ps_therapyParameters[therapyPos].configuredValue = value;
                break;
            }

        case DEV_INFO_MANUAL_ENABLE_CHAR:
        case DEV_INFO_AMBIENT_SET_AUDIO_VOLUME_CHAR:
        case DEV_INFO_MASK_LEAK_ENABLE_CHAR:
        case DEV_INFO_FLOW_RESPONSE_ENABLE_CHAR:
            {
                uint8_t value = atoi(configuredValueString);
                ps_therapyParameters[therapyPos].configuredValue =
                    (uint8_t)(bool)value;
                break;
            }

        case DEV_INFO_APAP_MAX_CHAR:
        case DEV_INFO_APAP_MIN_CHAR:
        case DEV_INFO_THERAPY_PRESSURE_CHAR:
            {
                float pressure = atof(configuredValueString);
                ps_therapyParameters[therapyPos].configuredValue =
                    (uint8_t)(pressure * 10);
                break;
            }
    }
}
/******************************************************************************
 * @fn          SRV_HttpRequest_Execute()
 *
 * @brief       Executa um determinada requisição HTTP.
 * @param       p_url URL para requisição (Obrigatório).
 * @param       p_headers Cabeçalho a ser enviado (Obrigatório).
 * @param       p_body Corpo a ser enviado (Opcional).
 * @param       useConfigRToken Deve utilizar o configRToken na requisição.
 * @param       useCoreToken Deve utilizar o coreToken na requisição.
 * @param       method Verbo HTTP.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
static bool SRV_HttpRequest_Execute(const char *const p_url,
                                    const char *const p_headers,
                                    const char *const p_body,
                                    const bool useConfigRToken,
                                    const bool useCoreToken,
                                    const esp_http_client_method_t method) {
    if (ps_httpClient) {
        IF_TRUE_RETURN(!DRV_HttpClient_DestroyInstance(&ps_httpClient), false);
    }

    clientConfig.url = p_url;

    /**
     * @brief Cria a instância do objeto de conexão HTTP.
     */
    IF_TRUE_RETURN(
        !DRV_HttpClient_CreateInstance(&ps_httpClient, &clientConfig),
        false);

    /**
     * @brief Define o verbo HTTP.
     */
    if (esp_http_client_set_method(ps_httpClient, method) != ESP_OK) {
        goto failStatus;
    }

    /**
     * @brief Configura o "configRToken", se houver.
     */
    if (useConfigRToken) {
        if (!SRV_HttpRequest_SetAuthorizationHeader(ps_httpClient)) {
            goto failStatus;
        }
    }

    /**
     * @brief Configura o "p_coreToken", se houver.
     */
    if (useCoreToken) {
        if (!SRV_HttpRequest_SetUserTokenHeader(ps_httpClient)) {
            goto failStatus;
        }
    }

    /**
     * @brief Define os cabeçalhos customizados.
     */
    if (esp_http_client_set_header(ps_httpClient, "application", p_headers)
        != ESP_OK) {
        goto failStatus;
    };

    /**
     * @brief Define o tipo de conteúdo da requisição.
     */
    if (esp_http_client_set_header(ps_httpClient,
                                   "Content-Type",
                                   "application/json")
        != ESP_OK) {
        goto failStatus;
    }

    /**
     * @brief Define a localidade (Brasileira, nesse caso).
     */
    if (!SRV_HttpRequest_SetLocaleHeader(ps_httpClient)) {
        goto failStatus;
    }

    /**
     * @brief Define o corpo a ser enviado, se houver.
     */
    if (p_body != NULL) {
        if (esp_http_client_set_post_field(ps_httpClient,
                                           p_body,
                                           strlen(p_body))
            != ESP_OK) {
            goto failStatus;
        }
    }

    esp_err_t espStatus = ESP_OK;

    if (!DRV_HttpClient_Perform(&ps_httpClient, &espStatus)) {
        goto failStatus;
    }

    if (espStatus != ESP_OK) {
        lastRequestWasSucessfull = false;
#if defined(ENABLE_VERBOSE_HTTP_EXECUTE)
        ESP_LOGE(TAG, "HTTP Request Failed: %s", esp_err_to_name(espStatus));
#endif
        goto failStatus;
    }

    int contentLength = esp_http_client_get_content_length(ps_httpClient);

    if (totalContentLength != contentLength) {
#if defined(ENABLE_VERBOSE_HTTP_EXECUTE)
        ESP_LOGE(TAG,
                 "HTTP Request Failed: %s",
                 "totalContentLength != contentLength");
#endif
        lastRequestWasSucessfull = false;
        goto failStatus;
    }

#if defined(ENABLE_VERBOSE_HTTP_EXECUTE)
    ESP_LOGI(TAG,
             "HTTP Status = %d, content_length = %d",
             esp_http_client_get_status_code(ps_httpClient),
             contentLength);
#endif

    DRV_HttpClient_DestroyInstance(&ps_httpClient);
    return true;

failStatus:
    {
#if defined(ENABLE_VERBOSE_HTTP_EXECUTE)
        ESP_LOGW(TAG, "Falha ao executar requisição HTTP...");
#endif

        DRV_HttpClient_DestroyInstance(&ps_httpClient);
        return false;
    }
}
/******************************************************************************
 * @fn          CheckSuccessfulResponse()
 *
 * @brief       Verifica se determinada requisição, efetuada no Core, foi
 * bem-sucedida.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
static bool CheckSuccessfulResponse(void) {
    if (!lastRequestWasSucessfull) {
        return false;
    }

    bool statusCode = false;
    cJSON *root     = cJSON_Parse(httpResponseBuffer);

    if (root == NULL) {
#if defined(ENABLE_VERBOSE_HTTP_REQUEST_SERVICE)
        ESP_LOGE(TAG, "Não foi possível obter o status da resposta.");
#endif
        return false;
    }

    cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "code");

    if (code == NULL || !cJSON_IsString(code)) {
#if defined(ENABLE_VERBOSE_HTTP_REQUEST_SERVICE)
        ESP_LOGE(TAG, "Erro ao tentar obter 'code' do JSON");
#endif
        statusCode = false;
        goto jsonDelete;
    }

    // Verifica se o código é "000" (sucesso)
    if (strcmp(code->valuestring, "000") == 0) {
#if defined(ENABLE_VERBOSE_HTTP_REQUEST_SERVICE)
        ESP_LOGI(TAG, "Requisição processada com sucesso!");
#endif
        statusCode = true;
        goto jsonDelete;
    } else {
#if defined(ENABLE_VERBOSE_HTTP_REQUEST_SERVICE)
        ESP_LOGE(TAG, "Erro na requisição: %s", code->valuestring);
#endif
    }

jsonDelete:
    { cJSON_Delete(root); }

    return statusCode;
}
/******************************************************************************
 * @brief Função para manipular eventos vindos de requisições Http.
 *
 * @param p_evt Ponteiro para o evento gerado durante a requisição Http.
 ******************************************************************************/
static esp_err_t HttpRequestEventHandler(esp_http_client_event_t *p_evt) {
    switch (p_evt->event_id) {
        case HTTP_EVENT_ERROR:
#if defined(ENABLE_VERBOSE_HTTP_REQUEST_SERVICE)
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
#endif
            lastRequestWasSucessfull = false;
            break;
        case HTTP_EVENT_ON_CONNECTED:
            totalContentLength = 0;
            memset((void *)httpResponseBuffer, 0, HTTP_RESPONSE_BUFFER_LENGTH);
#if defined(ENABLE_VERBOSE_HTTP_REQUEST_SERVICE)
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
#endif
            break;
        case HTTP_EVENT_HEADER_SENT:
#if defined(ENABLE_VERBOSE_HTTP_REQUEST_SERVICE)
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
#endif
            break;
        case HTTP_EVENT_ON_HEADER:
#if defined(ENABLE_VERBOSE_HTTP_REQUEST_SERVICE)
            ESP_LOGI(TAG,
                     "HTTP_EVENT_ON_HEADER, key=%s, value=%s",
                     p_evt->header_key,
                     p_evt->header_value);
#endif

            break;
        case HTTP_EVENT_ON_DATA:
#if defined(ENABLE_VERBOSE_HTTP_REQUEST_SERVICE)
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", p_evt->data_len);
#endif

#if defined(ENABLE_PRINT_HTTP_RESPONSE_DATA)
            if (!esp_http_client_is_chunked_response(p_evt->client)) {
                printf("\n\n%.*s\n\n", p_evt->data_len, (char *)p_evt->data);
            }
#endif

            if (totalContentLength + p_evt->data_len
                < HTTP_RESPONSE_BUFFER_LENGTH) {
                memcpy((char *)(httpResponseBuffer + totalContentLength),
                       (char *)p_evt->data,
                       p_evt->data_len);
                totalContentLength += p_evt->data_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            lastRequestWasSucessfull = true;
#if defined(ENABLE_VERBOSE_HTTP_REQUEST_SERVICE)
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
#endif
            break;
        case HTTP_EVENT_DISCONNECTED:
#if defined(ENABLE_VERBOSE_HTTP_REQUEST_SERVICE)
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
#endif
            break;
        case HTTP_EVENT_REDIRECT:
#if defined(ENABLE_VERBOSE_HTTP_REQUEST_SERVICE)
            ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
#endif
            break;
    }
    return ESP_OK;
}

/*****************************END OF FILE**************************************/