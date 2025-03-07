/******************************************************************************
 * @file         : WifiProvisionService.c
 * @authors      : Thales Mol Vinhal
 * @copyright    : Copyright (c) 2023
 * @brief        : Serviço para provisão de credenciais WiFi
 * @attention    : Baseado no exemplo WiFi Provision Manager da Espressif
 * disponível no esp-idf, ou através do link abaixo:
 * https://github.com/espressif/esp-idf/tree/master/examples/provisioning/wifi_prov_mgr
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/

#include "Services/WifiProvisionService.h"

/* Private define ------------------------------------------------------------*/

#define PROV_QR_VERSION  "v1"
#define PROV_TRANSPORT   "softap"
#define QRCODE_BASE_URL  "https://espressif.github.io/esp-jumpstart/qrcode.html"
#define SRV_MAC_ADDR_LEN 6
#define SRV_NAME_MAX_LEN 32
#define SRV_POP_MAX_LEN  21

#define QR_CODE_SIZE 154

#define BIT_0 (1 << 0)

/**
 * @attention Descomente esta macro para gerar qrCodes distintos
 * para o mesmo Device!!
 * @warning Não faça isto até estar integrado com o APP!!!
 */
// #define PROPRIETARY_CRYPTO

/* Private function prototypes -----------------------------------------------*/

static void ClearNetworkStatus(void);

static char *AddBackslashes(const char *p_jsonString);

static bool provisioned = false;

static void EventHandler(const void *const p_arg,
                         const esp_event_base_t eventBase,
                         const int32_t eventId,
                         const void *const p_eventData);

static void GenerateServiceSsidName(char *const p_serviceName,
                                    const size_t max);

static void PrintAcessPointQrCode(const char *const p_name,
                                  const char *const p_pop,
                                  const char *const p_transport);

static void RotateChararacter(char *const p_character, const int times);

static void WifiStation_Init(void);

/* Private variables ---------------------------------------------------------*/

static const char *p_moduleName = "WifiProvisionService";

/* Signal Wi-Fi events on this event-group */
const int WIFI_CONNECTED_EVENT = BIT_0;
static EventGroupHandle_t wifiEventGroup;

/* Public functions ----------------------------------------------------------*/

/******************************************************************************
 * @brief Inicialização do serviço de provisão de Credenciais WiFi.
 * @param p_parameter Ponteiro para um parâmetro que pode ser passado para esta
 * função. Esta assinatura foi usada para compatibilizar a função abaixo com
 * chamadas assíncronas, mas a princípio não será utilizada desta forma.
 * Para chamá-la síncronamente basta passar este parâmetro como NULL.
 ******************************************************************************/
void SRV_WifiProvision_Init(void *p_parameter) {
    /* Initialize TCP/IP */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Initialize the event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifiEventGroup = xEventGroupCreate();

    ClearNetworkStatus();

    /* Register the event handler for Wi-Fi, IP and Provisioning */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &EventHandler,
                                               NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &EventHandler,
                                               NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                               IP_EVENT_STA_GOT_IP,
                                               &EventHandler,
                                               NULL));

    /* Initialize Wi-Fi including netif with default config */
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Configuration for the provisioning manager */
    wifi_prov_mgr_config_t config = {
        .scheme               = wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE};

    /* Initialize provisioning manager with the
     * configuration parameters set above */
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    /* Let's find out if the device is provisioned */
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned) {
        ESP_LOGI(p_moduleName, "Starting provisioning");

        char serviceName[SRV_NAME_MAX_LEN] = {
            [0 ...(GET_SIZE_ARRAY(serviceName) - 1)] = 0};

        GenerateServiceSsidName(serviceName, sizeof(serviceName));

        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;

        uint8_t ethernetMacAdress[SRV_MAC_ADDR_LEN] = {
            [0 ...(GET_SIZE_ARRAY(ethernetMacAdress) - 1)] = 0};

        esp_wifi_get_mac(WIFI_IF_STA, ethernetMacAdress);

        char popString[SRV_POP_MAX_LEN] = {
            [0 ...(GET_SIZE_ARRAY(popString) - 1)] = 0};

        snprintf(popString,
                 sizeof(popString),
                 "%02X%s%02X%s%02X%s%02X%s",
                 ethernetMacAdress[0],
                 "S2Y",
                 ethernetMacAdress[1],
                 "N3E",
                 ethernetMacAdress[2],
                 "G5I",
                 ethernetMacAdress[3],
                 "X7O");

#ifdef PROPRIETARY_CRYPTO
        for (int i = 0; i < sizeof(popString); i++) {
            RotateChararacter(&popString[i], i);
        }
#endif

        const char *p_proofOfPossesion                 = popString;
        wifi_prov_security1_params_t *p_securityParams = p_proofOfPossesion;

        /* What is the service key (could be NULL) */
        const char *p_serviceKey = NULL;

        /* Start provisioning service */
        ESP_ERROR_CHECK(
            wifi_prov_mgr_start_provisioning(security,
                                             (const void *)p_securityParams,
                                             serviceName,
                                             p_serviceKey));

        PrintAcessPointQrCode(serviceName, p_proofOfPossesion, PROV_TRANSPORT);
    } else {
        ESP_LOGI(p_moduleName, "Already provisioned, starting Wi-Fi STA");
        wifi_prov_mgr_deinit();

        /* Start Wi-Fi station */
        WifiStation_Init();
    }
}
/******************************************************************************
 * @fn          SRV_WifiProvision_IsConnected()
 *
 * @brief       Informa se o dispositivo está provisionado e conectado à rede
 * WiFi.
 * @return      Booleano - Status de retorno.
 *              @retval False - Não provisionado e/ou não conectado.
 *              @retval True - Provisionado e conectado com sucesso.
 ******************************************************************************/
const bool SRV_WifiProvision_IsConnected(void) {
    EventBits_t uxBits = xEventGroupGetBits(wifiEventGroup);
    return (uxBits & WIFI_CONNECTED_EVENT) > 0;
}
/******************************************************************************
 * @fn          SRV_WifiProvision_WaitingConnect()
 *
 * @brief       Aguarda até que o dispositivo esteja provisionado e conectado
 * à rede WiFi.
 ******************************************************************************/
void SRV_WifiProvision_WaitingConnect(void) {
    xEventGroupWaitBits(wifiEventGroup, BIT_0, pdFALSE, pdTRUE, portMAX_DELAY);
}
/******************************************************************************
 * @brief Desconecta da rede WiFi atual e esquece as credenciais da rede.
 * @param None
 ******************************************************************************/
void SRV_WifiProvision_WifiDisconnectAndForgetNetwork() {
    wifi_mode_t wifi_mode;
    esp_err_t err;
    ESP_LOGI(p_moduleName,
             "Disconnecting from current WiFi network and forgetting network "
             "credentials...");

    // Check if WiFi is already disconnected
    err = esp_wifi_get_mode(&wifi_mode);
    if (wifi_mode == WIFI_MODE_NULL) {
        ESP_LOGI(p_moduleName, "WiFi is already disconnected.");
        return;
    }

    // Disconnect WiFi
    err = esp_wifi_disconnect();
    if (err != ESP_OK) {
        ESP_LOGE(p_moduleName,
                 "Failed to disconnect WiFi. Error code: %d",
                 err);
        return;
    }

    // Forget the previously connected network
    provisioned = false;
    wifi_prov_mgr_is_provisioned(&provisioned);

    ESP_ERROR_CHECK(wifi_prov_mgr_reset_provisioning());
    ESP_LOGI(p_moduleName,
             "WiFi disconnected and network forgotten successfully.");
    esp_restart();
}

/******************************************************************************
 * @brief Função para obter o QRCode a ser provisionado ao STM!
 * @returns char * ponteiro para o payload do QRCode a ser provisionado.
 ******************************************************************************/
uint8_t *const SRV_WifiProvision_GetQrCode() {
    char *payload = (char *)pvPortMalloc(QR_CODE_SIZE * sizeof(char));

    if (payload == NULL) {
        // Lidar com erro de alocação de memória
        return NULL;
    }

    char serviceName[SRV_NAME_MAX_LEN] = {
        [0 ...(GET_SIZE_ARRAY(serviceName) - 1)] = 0};

    GenerateServiceSsidName(serviceName, sizeof(serviceName));

    uint8_t ethernetMacAdress[SRV_MAC_ADDR_LEN] = {
        [0 ...(GET_SIZE_ARRAY(ethernetMacAdress) - 1)] = 0};

    esp_wifi_get_mac(WIFI_IF_STA, ethernetMacAdress);

    char popString[SRV_POP_MAX_LEN] = {[0 ...(GET_SIZE_ARRAY(popString) - 1)] =
                                           0};

    snprintf(popString,
             sizeof(popString),
             "%02X%s%02X%s%02X%s%02X%s",
             ethernetMacAdress[0],
             "S2Y",
             ethernetMacAdress[1],
             "N3E",
             ethernetMacAdress[2],
             "G5I",
             ethernetMacAdress[3],
             "X7O");

    snprintf(payload,
             QR_CODE_SIZE,
             "{\"ver\":\"%s\",\"name\":\"%s\""
             ",\"pop\":\"%s\",\"transport\":\"%s\"}",
             PROV_QR_VERSION,
             serviceName,
             popString,
             PROV_TRANSPORT);

    printf("qrCode : %s\n", payload);

    char *data = AddBackslashes(payload);

    if (data) {
        vPortFree((void *)data);
    }

    printf("qrCode formatado : %s\n", payload);
    return (uint8_t *)payload;
}

/* Private functions ---------------------------------------------------------*/

/******************************************************************************
 * @fn          ClearNetworkStatus()
 *
 * @brief       Limpa o bit que informa o estado de conexão e provisionamento
 * da rede WiFi.
 ******************************************************************************/
static void ClearNetworkStatus(void) {
    xEventGroupClearBits(wifiEventGroup, WIFI_CONNECTED_EVENT);
}

char *AddBackslashes(const char *p_jsonString) {
    size_t jsonLen = strlen(p_jsonString);
    size_t cStringLen =
        jsonLen * 2 + 1;  // Cada aspas pode ser precedida por uma barra
                          // invertida, mais o caractere nulo
    char *modifiedString = (char *)pvPortMalloc(cStringLen);

    if (modifiedString == NULL) {
        printf("Erro: Falha ao alocar memória.\n");
        return NULL;
    }

    size_t j = 0;  // Índice para a string original
    size_t m = 0;  // Índice para a string modificada

    // Percorre a string original e adiciona uma barra invertida antes de cada
    // aspas
    while (j < jsonLen) {
        if (p_jsonString[j] == '"') {
            modifiedString[m++] = '\\';  // Adiciona a barra invertida
        }
        modifiedString[m++] = p_jsonString[j++];
    }

    modifiedString[m] =
        '\0';  // Adiciona o caractere nulo no final da string modificada

    return modifiedString;
}

/******************************************************************************
 * @brief Manipulador para tratar os eventos como conexão/desconexão WiFi.
 * @param p_arg Argumentos vindo do evento invocado (a princípio não utilizado)
 * @param eventBase Qual foi a base (tipo) do evento
 * @param eventId Id do evento
 * @param p_eventData Ponteiro para os dados vindos do evento invocado
 ******************************************************************************/
static void EventHandler(const void *const p_arg,
                         const esp_event_base_t eventBase,
                         const int32_t eventId,
                         const void *const p_eventData) {
#if defined(DEBUG_MODE_ON)
    return;
#else
    static int retries;
    static int diconnectionRetries;

    if (eventBase == WIFI_PROV_EVENT) {
        switch (eventId) {
            case WIFI_PROV_START:
                {
                    ESP_LOGI(p_moduleName, "Provisioning started");

                    /* Envia via Serial para o STM uma indicação de início de
                    Provisionamento de credenciais WiFI*/
                    uint8_t wifiStatus = WIFI_STATUS_ENABLED;

                    MainUc_SRV_AppWriteEvtCB(
                        (uint8_t)WIRELESS_CONNECTIONS_PROFILE_APP,
                        WIRELESS_CONNECTIONS_WIFI_STATE_CHAR,
                        sizeof(wifiStatus),
                        &wifiStatus);

                    break;
                }

            case WIFI_PROV_CRED_RECV:
                {
                    // ClearNetworkStatus();
                    retries = 0;

                    wifi_sta_config_t *p_wifiStationConfig =
                        (wifi_sta_config_t *)p_eventData;

                    ESP_LOGI(p_moduleName,
                             "Received Wi-Fi credentials"
                             "\n\tSSID     : %s\n\tPassword : %s",
                             (const char *)p_wifiStationConfig->ssid,
                             (const char *)p_wifiStationConfig->password);

                    uint8_t wifiStatus = WIFI_STATUS_CONNECTING;

                    MainUc_SRV_AppWriteEvtCB(
                        (uint8_t)WIRELESS_CONNECTIONS_PROFILE_APP,
                        WIRELESS_CONNECTIONS_WIFI_STATE_CHAR,
                        sizeof(wifiStatus),
                        &wifiStatus);
                    break;
                }

            case WIFI_PROV_CRED_FAIL:
                {
                    ClearNetworkStatus();

                    wifi_prov_sta_fail_reason_t *p_reason =
                        (wifi_prov_sta_fail_reason_t *)p_eventData;

                    ESP_LOGE(
                        p_moduleName,
                        "Provisioning failed!\n\tReason : %s"
                        "\n\tPlease reset to factory and retry provisioning",
                        (*p_reason == WIFI_PROV_STA_AUTH_ERROR)
                            ? "Wi-Fi station authentication failed"
                            : "Wi-Fi access-point not found");

                    retries++;
                    if (retries >= CONFIG_APPLICATION_PROV_MGR_MAX_RETRY_CNT) {
                        retries = 0;
                        ESP_LOGI(p_moduleName,
                                 "Failed to connect with provisioned AP, "
                                 "reseting provisioned credentials");
                        wifi_prov_mgr_reset_sm_state_on_failure();
                    }
                    break;
                }

            case WIFI_PROV_CRED_SUCCESS:
                {
                    retries            = 0;
                    uint8_t wifiStatus = WIFI_STATUS_CONNECTING;

                    MainUc_SRV_AppWriteEvtCB(
                        (uint8_t)WIRELESS_CONNECTIONS_PROFILE_APP,
                        WIRELESS_CONNECTIONS_WIFI_STATE_CHAR,
                        sizeof(wifiStatus),
                        &wifiStatus);

                    ESP_LOGI(p_moduleName, "Provisioning successful");
                    break;
                }

            case WIFI_PROV_END:
                {
                    /* De-initialize manager once provisioning is finished */
                    wifi_prov_mgr_deinit();
                    break;
                }
            default:
                break;
        }
    } else if (eventBase == WIFI_EVENT) {
        switch (eventId) {
            case WIFI_EVENT_STA_START:
                {
                    esp_wifi_connect();
                    break;
                }

            case WIFI_EVENT_STA_DISCONNECTED:
                {
                    ClearNetworkStatus();

                    diconnectionRetries++;

                    if (diconnectionRetries
                        >= CONFIG_APPLICATION_PROV_MGR_MAX_RETRY_CNT) {
                        diconnectionRetries = 0;

                        ESP_LOGI(p_moduleName,
                                 "Failed to connect with provisioned AP, "
                                 "reseting provisioned credentials");

                        ESP_ERROR_CHECK(wifi_prov_mgr_reset_provisioning());
                        esp_restart();
                        break;
                    }
                    ESP_LOGI(p_moduleName,
                             "Disconnected. Connecting to the AP again...");
                    esp_wifi_connect();
                    break;
                }

            case WIFI_EVENT_AP_STACONNECTED:
                {
                    uint8_t wifiStatus = WIFI_STATUS_PROVISIONING;

                    MainUc_SRV_AppWriteEvtCB(
                        (uint8_t)WIRELESS_CONNECTIONS_PROFILE_APP,
                        WIRELESS_CONNECTIONS_WIFI_STATE_CHAR,
                        sizeof(wifiStatus),
                        &wifiStatus);

                    ESP_LOGI(p_moduleName, "SoftAP transport: Connected!");
                    break;
                }

            case WIFI_EVENT_AP_STADISCONNECTED:
                {
                    if (!SRV_WifiProvision_IsConnected()) {
                        retries            = 0;
                        uint8_t wifiStatus = WIFI_STATUS_ENABLED;

                        MainUc_SRV_AppWriteEvtCB(
                            (uint8_t)WIRELESS_CONNECTIONS_PROFILE_APP,
                            WIRELESS_CONNECTIONS_WIFI_STATE_CHAR,
                            sizeof(wifiStatus),
                            &wifiStatus);
                    }

                    ESP_LOGI(p_moduleName, "SoftAP transport: Disconnected!");
                    break;
                }

            default:
                break;
        }
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *p_event = (ip_event_got_ip_t *)p_eventData;
        ESP_LOGI(p_moduleName,
                 "Connected with IP Address:" IPSTR,
                 IP2STR(&p_event->ip_info.ip));
        /* Signal main application to continue execution */
        xEventGroupSetBits(wifiEventGroup, WIFI_CONNECTED_EVENT);
    }
#endif
}

/******************************************************************************
 * @brief Inicializa o módulo WiFi no modo Station.
 ******************************************************************************/
static void WifiStation_Init(void) {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/******************************************************************************
 * @brief Gera o SSID do serviço de provisão de credenciais WiFi.
 * O serviço tem um nome padrão do tipo 'OXYGENYS_CAFE', onde 0xCA 0xFE seriam
 * os quatro últimos caracteres do MAC ethernet do dispositivo.
 * @param p_serviceName Ponteiro para o SSID (nome) do serviço
 * @param max Tamanho máximo do SSID em bytes
 ******************************************************************************/
static void GenerateServiceSsidName(char *const p_serviceName,
                                    const size_t max) {
    uint8_t ethernetMacAdress[6];
    const char *p_ssidPrefix = "OXYGENYS_";
    esp_wifi_get_mac(WIFI_IF_STA, ethernetMacAdress);
    snprintf(p_serviceName,
             max,
             "%s%02X%02X",
             p_ssidPrefix,
             ethernetMacAdress[4],
             ethernetMacAdress[5]);
}

/******************************************************************************
 * @brief Printa no terminal de monitoração do programa um QR CODE que permite
 * a conexão com o ponto de acesso WiFi gerado pelo ESP para provisão de
 * credenciais.
 * @param p_name Ponteiro para o nome do device a ser conectado, por exemplo,
 * 'OXYGENIS_CAFE'.
 * @param p_pop Proof-of-possesion. Ponteiro para a string gerada pela função
 * de inicialização do serviço de provisão de credenciais WiFi a partir de
 * uma combinação de caracteres rotacionados do endereço MAC do dispositivo,
 * para garantir a não exposição do endereço MAC real e ao mesmo tempo gerar
 * uma string 'pop' única decodificável para cada device.
 * @param p_transport Meio de transporte para a provisão de credenciais WiFi,
 * no nosso caso o meio de transporte é via SoftAP (WiFi Acess Point)
 ******************************************************************************/
static void PrintAcessPointQrCode(const char *const p_name,
                                  const char *const p_pop,
                                  const char *const p_transport) {
    if (!p_name || !p_transport || !p_pop) {
        ESP_LOGW(p_moduleName,
                 "Cannot generate QR code payload. Data missing.");
        return;
    }
    char payload[QR_CODE_SIZE] = {[0 ...(GET_SIZE_ARRAY(payload) - 1)] = 0};

    snprintf(payload,
             sizeof(payload),
             "{\"ver\":\"%s\",\"name\":\"%s\""
             ",\"pop\":\"%s\",\"transport\":\"%s\"}",
             PROV_QR_VERSION,
             p_name,
             p_pop,
             p_transport);

    ESP_LOGI(p_moduleName,
             "Scan this QR code from the provisioning application for "
             "Provisioning.");

    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    esp_qrcode_generate(&cfg, payload);

    ESP_LOGI(p_moduleName,
             "If QR code is not visible, copy paste the below URL in a "
             "browser.\n%s?data=%s",
             QRCODE_BASE_URL,
             payload);
}

/******************************************************************************
 * @brief Rotaciona o caracter recebido em uma determinada quantidade de vezes,
 * não alterando sua característica, isto é, se o caracter é uma letra maíuscula
 * 'A-Z' ele permanece sendo letra maiúscula após a rotação. No extremo, temos
 * a transformação 'Z' -> 'A'.
 * Caso o caracter seja uma letra minúscula, ou um número, a mesma lógica
 * descrita anteriormente se aplica.
 * @param p_character Ponteiro para o caracter a ser rotacionado.
 * @param times Quantidade de rotações que devem ser executadas, sempre no
 * sentido horário, isto é, 'A' -> 'B' ao rotacionar o caracter 'A' em 1 vez.
 ******************************************************************************/
static void RotateChararacter(char *const p_character, const int times) {
    for (int i = 0; i < times; i++) {
        if (*p_character >= 'A' && *p_character <= 'Z') {
            (*p_character)++;
            if (*p_character > 'Z') {
                *p_character = 'A';
            }
        } else if (*p_character >= 'a' && *p_character <= 'z') {
            (*p_character)++;
            if (*p_character > 'z') {
                *p_character = 'a';
            }
        } else if (*p_character >= '0' && *p_character <= '9') {
            (*p_character)++;
            if (*p_character > '9') {
                *p_character = '0';
            }
        }
    }
}

/*****************************END OF FILE**************************************/