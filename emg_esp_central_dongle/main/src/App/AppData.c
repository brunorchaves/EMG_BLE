#include "App/AppData.h"

static uint8_t GetNumberInVersion(char *const start, char *const end);

static uint64_t SERIAL_NUMBER = 0;

static const ApiCredentialsTypeDef API_CREDENTIALS = {
#if (ENVIRONMENT_FOTA_TYPE == FOTA_PLATFORM_HOMOLOG)
    .configrUrl = "https://hlg.configr-api.cmosdrake.com.br",
    .coreUrl    = "https://hlg.core-api.cmosdrake.com.br",
#else
    .configrUrl = "https://dev.configr-api.cmosdrake.com.br",
    .coreUrl    = "https://dev.core-api.cmosdrake.com.br",
#endif
    .coreApplicationKey  = "CORE",
    .cpapClientId        = "APP_PHOENIX_API",//ConfigR
    .cpapSecretKey       = "caa45749-6053-4528-b229-03dbe937dc67",
    .cpapApplicationKey  = "PHOENIX",
    .hashCryptoAlgorithm = "aes-256-ctr",
    .password            = "PHo@a1b*55",
    .hashCryptoKey       = "1WnBDkrB5dCEfSIqp8Zsp9CaDvRWKLb0",
};
/******************************************************************************
 * @fn          App_AppData_GetApiCredentials()
 *
 * @brief       Retorna as informações de credenciais da API.
 ******************************************************************************/
ApiCredentialsTypeDef *App_AppData_GetApiCredentials(void) {
    return &API_CREDENTIALS;
}
/******************************************************************************
 * @fn          App_AppData_SetSerialNumber()
 *
 * @brief       Define o valor do número serial recuperado do equipamento.
 * @param       serialNumber Número serial recebido do equipamento.
 ******************************************************************************/
void App_AppData_SetSerialNumber(const uint64_t serialNumber) {
    SERIAL_NUMBER = serialNumber;
    App_AppData_NotifySetSerialNumberCallback();
}
/******************************************************************************
 * @fn          App_AppData_NotifySetSerialNumberCallback()
 *
 * @brief       Notifica o Esp quando receber o número serial advindo do STM.
 ******************************************************************************/
__weak void App_AppData_NotifySetSerialNumberCallback(void) {}
/******************************************************************************
 * @fn          App_AppData_GetSerialNumber()
 *
 * @brief       Retorna o número serial do equipamento.
 * @return      uint64_t - Valor de retorno.
 ******************************************************************************/
const uint64_t App_AppData_GetSerialNumber(void) { return SERIAL_NUMBER; }
/******************************************************************************
 * @fn          GetFirmwareVersion()
 *
 * @brief       Retorna a versão do firmware, que contém 4 bytes, em um
 * único valor.
 * @param       p_version Vetor de cadeia de caracteres com a versão do
 * firmware.
 * @return      uint32_t - Valor de retorno.
 ******************************************************************************/
const uint32_t App_AppData_GetVersionFirmware(char *const p_version) {
    IF_TRUE_RETURN(!p_version, 0);

    char *p_findString = NULL;

    int pos            = 0;
    uint32_t fwVersion = 0;

    for (uint8_t i = 0; i < sizeof(uint32_t); i++) {
        p_findString = strstr(&p_version[pos], ".");

        if (!p_findString) {
            p_findString = strstr(&p_version[pos], "\0") + 1;
        }

        int fwNumber = 0;

        if (p_findString) {
            fwNumber =
                GetNumberInVersion((char *)&p_version[pos], p_findString);

            pos = p_findString - p_version + 1;
        }

        fwVersion |= fwNumber << (i * 8);

        if (pos >= strlen(p_version)) {
            break;
        }
    }

    return fwVersion;
}
/******************************************************************************
 * @fn          GetNumberInVersion()
 *
 * @brief       Obtém o número contido entre a cadeia de caracteres.
 * @param       start Endereço inicial da cadeia de caracteres.
 * @param       end Endereço final da cadeia de caracteres.
 * @return      uint8_t - Valor de retorno.
 ******************************************************************************/
static uint8_t GetNumberInVersion(char *const start, char *const end) {
    char p_value[UINT8_MAX / 24] = {[0 ...(GET_SIZE_ARRAY(p_value) - 1)] = 0};

    memcpy((void *)p_value, (void *)start, (size_t)(end - start));
    return atoi(p_value);
}
/*****************************END OF FILE**************************************/