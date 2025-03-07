/* Includes ------------------------------------------------------------------*/

#include "Drivers/HttpClientDriver.h"

/* Private define ------------------------------------------------------------*/

/**
 * @brief Obtém o semáforo.
 */
#define __TAKE_SEMAPHORE(semaphore) (TAKE_SEMAPHORE(semaphore, portMAX_DELAY))

/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

SemaphoreHandle_t xhttpClientDriverSync                       = NULL;
static volatile esp_http_client_handle_t ps_httpClientToClose = NULL;

/* Private function prototypes -----------------------------------------------*/
/* Public functions ----------------------------------------------------------*/

/******************************************************************************
 * @fn          DRV_HttpClient_Init()
 *
 * @brief       Inicializa o driver.
 ******************************************************************************/
void DRV_HttpClient_Init(void) {
    IF_TRUE_RETURN(xhttpClientDriverSync);

    xhttpClientDriverSync = xSemaphoreCreateBinary();
    configASSERT(xhttpClientDriverSync);

    if (xhttpClientDriverSync) {
        GIVE_SEMAPHORE(xhttpClientDriverSync);
    }
}
/******************************************************************************
 * @fn          DRV_HttpClient_IsInitializated()
 *
 * @brief       Informa se o driver já foi inicializado.
 * @return      Booleano - Status de retorno.
 *              @retval False - Não inicializado.
 *              @retval True - Já inicializado.
 ******************************************************************************/
bool DRV_HttpClient_IsInitializated(void) {
    return xhttpClientDriverSync != NULL;
}
/******************************************************************************
 * @fn          DRV_HttpClient_CreateInstance()
 *
 * @brief       Cria uma instância de cliente para comunicações HTTP.
 * @param       pps_httpClientHandler Ponteiro para o variável do cliente.
 * @param       ps_httpClientConfig Ponteiro para objeto com as configurações
 * para o cliente.
 * @return      Booleano - Status de retorno.
 *              @retval False - Um ou mais parâmetros inválidos ou falha durante
 * tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool DRV_HttpClient_CreateInstance(
    esp_http_client_handle_t *pps_httpClientHandler,
    esp_http_client_config_t *const ps_httpClientConfig) {
    IF_TRUE_RETURN(*pps_httpClientHandler, true);
    IF_TRUE_RETURN(!ps_httpClientConfig, false);

    if (ps_httpClientToClose) {
        IF_TRUE_RETURN(!DRV_HttpClient_DestroyInstance(&ps_httpClientToClose),
                       false);
        ps_httpClientToClose = NULL;
    }

    if (__TAKE_SEMAPHORE(xhttpClientDriverSync)) {
        *pps_httpClientHandler = esp_http_client_init(ps_httpClientConfig);
        vTaskDelay(pdMS_TO_TICKS(10));

        if (!(*pps_httpClientHandler)) {
            GIVE_SEMAPHORE(xhttpClientDriverSync);
            return false;
        }
    }

    return true;
}
/******************************************************************************
 * @fn          DRV_HttpClient_DestroyInstance()
 *
 * @brief       Destrói a instância do cliente criado para comunicações HTTP.
 * @param       pps_httpClientHandler Ponteiro para objeto do cliente criado.
 * @return      Booleano - Status de retorno.
 *              @retval False - Falha durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool DRV_HttpClient_DestroyInstance(
    esp_http_client_handle_t *pps_httpClientHandler) {
    IF_TRUE_RETURN(!*pps_httpClientHandler, true);

    esp_http_client_close(*pps_httpClientHandler);
    if (esp_http_client_cleanup(*pps_httpClientHandler) != ESP_OK) {
        ps_httpClientToClose = *pps_httpClientHandler;
        return false;
    }

    *pps_httpClientHandler = NULL;
    vTaskDelay(pdMS_TO_TICKS(10));
    GIVE_SEMAPHORE(xhttpClientDriverSync);

    return true;
}
/******************************************************************************
 * @fn          DRV_HttpClient_Perform()
 *
 * @brief       Executa as seguintes operações HTTP: abertura de conexão,
 * envio de requisição e fechamento de conexão.
 * @param       ps_httpClientHandler Objeto do cliente criado.
 * @param       p_errorEsp Ponteiro para variável que seja preenchida com o
 * status do enumerador retornado.
 * @return      Booleano - Status de retorno.
 *              @retval False - Um ou mais parâmetros inválidos ou falha
 *durante tentativa de executar a operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool DRV_HttpClient_Perform(
    esp_http_client_handle_t *const pps_httpClientHandler,
    esp_err_t *const p_errorEsp) {
    IF_TRUE_RETURN(!*pps_httpClientHandler, false);

    esp_err_t espStatus = esp_http_client_perform(*pps_httpClientHandler);

    if (p_errorEsp) {
        *p_errorEsp = espStatus;
    }

    return espStatus == ESP_OK;
}
/*****************************END OF FILE**************************************/