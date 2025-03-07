/******************************************************************************
 * @file         : NTPService.h
 * @author       : Thales Mol Vinhal
 * @copyright    : Copyright (c) 2023 CMOS Drake
 * @brief        : Serviço para provisão de data / hora via NTP (Network Time
 * Protocol)
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/

#include "Services/NTPService.h"

/* Private define ------------------------------------------------------------*/

#define TAG "NTP_SERVICE"

/* Private function prototypes -----------------------------------------------*/

static char *LongLongToCharArray(long long num);

/* Private variables ---------------------------------------------------------*/

static bool isInitialized = false;

/* Public functions ----------------------------------------------------------*/

/******************************************************************************
 * @brief Inicializa o serviço de NTP para aquisição de data/hora via servidor
 * web.
 ******************************************************************************/
void SRV_NTP_Init(void) {
    if (isInitialized)
        return;

    ESP_LOGI(TAG, "Initializing SNTP");

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

    isInitialized = true;
}
/******************************************************************************
 * @brief Obtém a data/hora atualizada via servidor web
 ******************************************************************************/
void SRV_NTP_ObtainTime(void) {
    if (!isInitialized) {
        SRV_NTP_Init();
    }

    // wait for time to be set
    time_t now         = 0;
    struct tm timeinfo = {0};
    while (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Waiting for system time to be set...");
        time(&now);
        localtime_r(&now, &timeinfo);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "Time is set");
}
/******************************************************************************
 * @brief Imprime no console o horário local (GMT-00)
 ******************************************************************************/
void SRV_NTP_PrintLocalTime(void) {
    time_t now;
    struct tm timeinfo;
    char formattedTimeBuffer[64];

    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(formattedTimeBuffer, sizeof(formattedTimeBuffer), "%c", &timeinfo);
    ESP_LOGI(TAG, "Current time: %s", formattedTimeBuffer);
}
/******************************************************************************
 * @brief Função para calcular o UnixTimestamp a partir do horário local.
 *
 * @return time_t UnixTimestamp
 ******************************************************************************/
time_t SRV_NTP_GetUnixTimestamp(void) {
    time_t now = 0;
    time(&now);

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    time_t unixTimestamp = 0;
    unixTimestamp        = mktime(&timeinfo);

    return unixTimestamp;
}
/******************************************************************************
 * @brief Função para formatar o unixtimestamp numérico
 ******************************************************************************/
void SRV_NTP_FormatTimestamp(time_t currentTime,
                             char *p_data,
                             uint8_t dataLen) {
    struct tm *timeinfo;

    // Converte time_t para struct tm em UTC
    timeinfo = gmtime(&currentTime);

    // 2024-07-19T17:16:25.990Z
    //  Formata a data e hora no formato ISO 8601, sem milissegundos
    strftime(p_data, dataLen, "%Y-%m-%dT%H:%M:%S.000Z", timeinfo);
}
/******************************************************************************
 * @brief Função para calcular o UnixTimestamp a partir do horário local.
 *
 * @return result Ponteiro que receberá a string do UnixTimestamp
 *
 * @attention Após usar o vetor de caracteres é preciso liberar a memória,
 *usando free()
 ******************************************************************************/
char *SRV_NTP_GetUnixTimestampString() {
    long long unixTimestamp = SRV_NTP_GetUnixTimestamp();

    return LongLongToCharArray(unixTimestamp);
}

/* Private functions ---------------------------------------------------------*/

/******************************************************************************
 * @brief Função para converter um long long para um vetor de caracteres
 *
 * @param num UnixTimestamp
 *
 * @return result Ponteiro que receberá a string do UnixTimestamp
 ******************************************************************************/
static char *LongLongToCharArray(long long num) {
    int size = snprintf(NULL, 0, "%lld", num);

    // Aloca memória para o vetor de caracteres
    char *result = (char *)malloc(size + 1);

    snprintf(result, size + 1, "%lld", num);

    return result;
}
/*****************************END OF FILE**************************************/