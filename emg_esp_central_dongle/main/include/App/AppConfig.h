#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define IF_TRUE_RETURN(expression, status_code...) \
    {                                              \
        if (expression) {                          \
            return status_code;                    \
        }                                          \
    }

/**
 * @brief Obtém o semáforo.
 */
#define TAKE_SEMAPHORE(semaphore, ticks) \
    ((bool)(xSemaphoreTake(semaphore, ticks) == pdTRUE))

/**
 * @brief Libera o semáforo.
 */
#define GIVE_SEMAPHORE(semaphore) (xSemaphoreGive(semaphore))

#define STRINGFY(s)                         #s
#define CONCAT_VARS(var1, var2)             (#var1 "" #var2)
#define CONCAT_STRINGS(str1, str2, args...) (str1 "" str2 "" args)

#ifndef MIN
    #define MIN(a, b) (a) > (b) ? (b) : (a)
#endif /** MIN */

#define GET_SIZE_ARRAY(v) (sizeof((v)) / sizeof((v)[0]))

#define LOW_BYTE(w)      (uint8_t)((w) & 0xFF)
#define HIGH_BYTE(w)     (uint8_t)((w) >> 8)
#define GET_WORD(b1, b2) ((uint16_t)((b1) << 8) | (b2))

#define __weak __attribute__((weak))
/**
 * Descomentar esta macro para habilitar o FOTA do ESP
 */
#define FOTA_DEVELOP          0
#define FOTA_HOMOLOG          1
#define FOTA_MAIN             2
#define FOTA_PLATFORM         3
#define FOTA_PLATFORM_HOMOLOG 4
#define FOTA_TEST             5

/**
 * @brief Essa MACRO habilita a atualização remota do Esp.
 */
#define ENABLE_ESP_FOTA

/**
 * @brief Tipo de ambiente para buscar as novas imagens par atualização dos
 * firmwares.
 */
#define ENVIRONMENT_FOTA_TYPE FOTA_DEVELOP  // FOTA_PLATFORM

#if (ENVIRONMENT_FOTA_TYPE == FOTA_DEVELOP)
    #define ESP_URL_UPDATE_FIRMWARE                                         \
        "https://cms-firmware-updates.s3.us-west-2.amazonaws.com/Oxygenis/" \
        "ESP-binary/develop/OXYGENIS.Firmware.BLE.bin"
#elif (ENVIRONMENT_FOTA_TYPE == FOTA_HOMOLOG)
    #define ESP_URL_UPDATE_FIRMWARE                                         \
        "https://cms-firmware-updates.s3.us-west-2.amazonaws.com/Oxygenis/" \
        "ESP-binary/homolog/OXYGENIS.Firmware.BLE.bin"
#elif (ENVIRONMENT_FOTA_TYPE == FOTA_MAIN)
    #define ESP_URL_UPDATE_FIRMWARE                                         \
        "https://cms-firmware-updates.s3.us-west-2.amazonaws.com/Oxygenis/" \
        "ESP-binary/main/OXYGENIS.Firmware.BLE.bin"
#elif (ENVIRONMENT_FOTA_TYPE == FOTA_PLATFORM)
    #define ESP_URL_UPDATE_FIRMWARE                                         \
        "https://cms-firmware-updates.s3.us-west-2.amazonaws.com/Oxygenis/" \
        "ESP-binary/platform/OXYGENIS.Firmware.BLE.bin"
#elif (ENVIRONMENT_FOTA_TYPE == FOTA_PLATFORM_HOMOLOG)
    #define ESP_URL_UPDATE_FIRMWARE                                         \
        "https://cms-firmware-updates.s3.us-west-2.amazonaws.com/Oxygenis/" \
        "ESP-binary/platform_homolog/OXYGENIS.Firmware.BLE.bin"
#else
    #define ESP_URL_UPDATE_FIRMWARE                                         \
        "https://cms-firmware-updates.s3.us-west-2.amazonaws.com/Oxygenis/" \
        "ESP-binary/test/OXYGENIS.Firmware.BLE.bin"
#endif

#define ESP_CHECK_FIRMWARE_PERIOD_MS (60 * 60 * 1000UL)  // 60 min

/**
 * @brief Essa MACRO habilita a atualização remota do STM.
 */
#define ENABLE_STM_FOTA

#if (ENVIRONMENT_FOTA_TYPE == FOTA_DEVELOP)
    #define STM_URL_UPDATE_FIRMWARE                                         \
        "https://cms-firmware-updates.s3.us-west-2.amazonaws.com/Oxygenis/" \
        "STM-binary/develop/OXYGENIS.Firmware.Main_278KB.bin"
#elif (ENVIRONMENT_FOTA_TYPE == FOTA_HOMOLOG)
    #define STM_URL_UPDATE_FIRMWARE                                         \
        "https://cms-firmware-updates.s3.us-west-2.amazonaws.com/Oxygenis/" \
        "STM-binary/homolog/OXYGENIS.Firmware.Main_278KB.bin"
#elif (ENVIRONMENT_FOTA_TYPE == FOTA_MAIN)
    #define STM_URL_UPDATE_FIRMWARE                                         \
        "https://cms-firmware-updates.s3.us-west-2.amazonaws.com/Oxygenis/" \
        "STM-binary/main/OXYGENIS.Firmware.Main_278KB.bin"
#elif (ENVIRONMENT_FOTA_TYPE == FOTA_PLATFORM)
    #define STM_URL_UPDATE_FIRMWARE                                         \
        "https://cms-firmware-updates.s3.us-west-2.amazonaws.com/Oxygenis/" \
        "STM-binary/platform/OXYGENIS.Firmware.Main_278KB.bin"
#elif (ENVIRONMENT_FOTA_TYPE == FOTA_PLATFORM_HOMOLOG)
    #define STM_URL_UPDATE_FIRMWARE                                         \
        "https://cms-firmware-updates.s3.us-west-2.amazonaws.com/Oxygenis/" \
        "STM-binary/platform_homolog/OXYGENIS.Firmware.Main_278KB.bin"
#else
    #define STM_URL_UPDATE_FIRMWARE                                         \
        "https://cms-firmware-updates.s3.us-west-2.amazonaws.com/Oxygenis/" \
        "STM-binary/test/OXYGENIS.Firmware.Main_278KB.bin"
#endif

#define STM_CHECK_FIRMWARE_PERIOD_MS (60 * 60 * 1000UL)  // 60min

/**
 * VERBOSE ALL
 */
// #define DEBUG_MODE_ON

#ifndef DEBUG_MODE_ON
    #define ENABLE_VERBOSE_ALL

    #if defined(ENABLE_VERBOSE_ALL)
        #define ENABLE_VERBOSE_MAIN_FILE
        #define ENABLE_VERBOSE_MAIN_TASK
        #define ENABLE_VERBOSE_FILE_UPLOAD_TASK
        #define ENABLE_VERBOSE_ESP_FOTA_TASK
        #define ENABLE_VERBOSE_STM_FOTA_TASK
        #define ENABLE_VERBOSE_STM_USART_TASK
        // #define ENABLE_VERBOSE_ESP_FOTA_SERVICE
        // #define ENABLE_VERBOSE_STM_FOTA_SERVICE
        #define ENABLE_VERBOSE_HTTP_REQUEST_SERVICE
        // #define ENABLE_VERBOSE_HTTP_FILE_UPLOAD_SERVICE
        #define ENABLE_VERBOSE_STM_USART_DRIVER
    #endif /** ENABLE_VERBOSE_ALL */
#endif

/**
 * USART
 */
#define GET_USART_SEND_TIMEOUT_MS(bytesCount, baudRate)                      \
    (uint32_t)({                                                             \
        float bytesByMs    = (float)(baudRate) / (10.0f * 1000.0f);          \
        uint32_t timeoutMs = ceill(3.50f * (float)(bytesCount) / bytesByMs); \
        (baudRate) < 19200 ? timeoutMs : (timeoutMs < 2 ? 2 : timeoutMs);    \
    })

#if defined(__cplusplus)
}
#endif /** __cplusplus */

#endif /** APP_CONFIG_H */
