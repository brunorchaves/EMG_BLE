/******************************************************************************
 * @file         : AppData.h
 * @authors      : Thales Mol Vinhal
 * @copyright    : Copyright (c) 2023 CMOS Drake
 * @brief        : Arquivo contendo macros e definição de tipos para a
 * aplicação.
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef __APP_DATA_H__
#define __APP_DATA_H__

#if defined(__cplusplus)
extern "C" {
#endif /** __cplusplus */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "App/AppConfig.h"

/* Defines -------------------------------------------------------------------*/

#define AES_KEY_SIZE    32  // 256 bits
#define SECRET_KEY_SIZE 37

#define QR_CODE_SIZE 154

/**
 * @brief Cadeia de caracteres para identificação inicial do firmware.
 */
#define UPDATED_BY_FW "CMOS_FW"

/* Exported types ------------------------------------------------------------*/

typedef enum E_GeneralProfilesIds {
    GENERAL_PROFILE_APP,
    DEVICE_CONFIG_PROFILE_APP,
    THERAPY_PROFILE_APP,
    CALIBRATION_PROFILE_APP,
    WIRELESS_CONNECTIONS_PROFILE_APP,
    FILE_UPLOAD_PROFILE_APP,
    STM_FOTA_PROFILE_APP,
    ESP_FOTA_PROFILE_APP,
    TRACK_POSITION_PROFILE_APP,
    ESP_PROFILE_APP,
    DEBUG_UART_PROFILE_APP,
    PROFILE_NUM,
} E_GeneralProfilesIdsTypeDef;

typedef enum E_TherapyParameters {
    HUMIDIFIER_LEVEL = 0,
    RAMP_TIME,
    OUT_RELIEF_LEVEL,
    MANUAL_ENABLE,
    MANUAL_PRESSURE_VALUE,
    AUTO_ENABLE,
    APAP_MIN,
    APAP_MAX,
    AMBIENT_SOUND,
    FLOW_RESPONSE,
    LEAKAGE,
    RAMP_ENABLE,
    ALARMS
} E_TherapyParametersTypeDef;

typedef enum {
    DEV_INFO_HUMIDIFIER_ENABLE_CHAR,
    DEV_INFO_HUMIDIFIER_LEVEL_CHAR,
    DEV_INFO_RAMP_TIME_CHAR,
    DEV_INFO_OUT_RELIEF_ENABLE_CHAR,
    DEV_INFO_OUT_RELIEF_LEVEL_CHAR,
    DEV_INFO_FLOW_RESPONSE_ENABLE_CHAR,
    DEV_INFO_MASK_LEAK_ENABLE_CHAR,
    DEV_INFO_NIGHT_SHADE_ENABLE_CHAR,
    DEV_INFO_BRIGHTNESS_LEVEL_CHA,
    DEV_INFO_STANDBY_ENABLE_CHAR,
    DEV_INFO_AMBIENT_SET_AUDIO_VOLUME_CHAR,
    DEV_INFO_THERAPY_PRESSURE_CHAR,
    DEV_INFO_MANUAL_ENABLE_CHAR,
    DEV_INFO_APAP_MIN_CHAR,
    DEV_INFO_APAP_MAX_CHAR,
    DEV_INFO_ALERT_CHAR,
    DEV_INFO_MAINTENANCE_CHAR,
    DEV_INFO_FILTER_CHANGE_CHAR,
    DEV_INFO_BLOWER_HOUR_METER_CHAR,
    DEV_INFO_BLOWER_HOUR_METER_RST_CHAR,
    DEV_INFO_SOS_BUTTON_CHAR,
    DEV_INFO_REQ_LIST_CHAR,
    DEV_INFO_REQ_CLEAR_LIST_CHARR,
    DEV_INFO_LIST_EMPTY_CHAR,
    DEV_INFO_DATA_CHAR,
    DEV_INFO_CHAR_SIZE,
} E_DeviceInfoCharacteristicsTypeDef;

typedef enum E_WirelessConnectionsProfileCharacteristic {
    WIRELESS_CONNECTIONS_WIFI_REQUEST_STATE_CHAR,
    WIRELESS_CONNECTIONS_WIFI_REQUEST_QRCODE_CHAR,
    WIRELESS_CONNECTIONS_BLUETOOTH_REQUEST_ENABLED_STATE_CHAR,
    WIRELESS_CONNECTIONS_BLUETOOTH_REQUEST_PAIRED_STATE_CHAR,
    WIRELESS_CONNECTIONS_BLUETOOTH_REQUEST_ENABLE_CHAR,
    WIRELESS_CONNECTIONS_BLUETOOTH_REQUEST_DISABLE_CHAR,
    WIRELESS_CONNECTIONS_WIFI_STATE_CHAR,
    WIRELESS_CONNECTIONS_WIFI_QRCODE_CHAR,
    WIRELESS_CONNECTIONS_BLUETOOTH_ENABLED_STATE_CHAR,
    WIRELESS_CONNECTIONS_BLUETOOTH_PAIRED_STATE_CHAR,
    WIRELESS_CONNECTIONS_WIFI_DISCONECT_CHAR,
    WIRELESS_CONNECTIONS_WIFI_NETWORK_NAME_CHAR,
    WIRELESS_CONNECTIONS_CHAR_SIZE,
} E_WirelessConnectionsProfileCharacteristicTypeDef;

typedef enum E_FileUploadCharacteristics {
    FILE_UPLOAD_HAS_DATA_RECOVERY_CHAR,
    FILE_UPLOAD_SET_FILE_RECOVERY_INFO_CHAR,
    FILE_UPLOAD_RECOVER_FILE_INFO_CHAR,
    FILE_UPLOAD_CONFIRM_BLOCK_POS_RECOVERED_CHAR,
    FILE_UPLOAD_GET_FILES_COUNT_CHAR,
    FILE_UPLOAD_READ_FRAME_DATA_CHAR,
    FILE_UPLOAD_SAVE_PROGRESS_CHAR,
    FILE_UPLOAD_MOVE_UPLOAD_FILE_CHAR,
} E_FileUploadCharacteristicsTypeDef;

typedef enum E_FileUploadProfileCharacteristics {
    E_FILE_UPLOAD_RESPONSE_COUNT_FILES_CHAR = 0,
    E_FILE_UPLOAD_RESPONSE_RECOVERY_INFO_CHAR,
    E_FILE_UPLOAD_RESPONSE_FRAME_DATA_CHAR,
    E_FILE_UPLOAD_CONFIRM_SAVE_DATA_CHAR,
} E_FileUploadProfileCharacteristicsTypeDef;

typedef enum E_STMFotaProfileCharacteristics {
    STM_FOTA_GET_VERSION_RUNNING_CHAR,
    STM_FOTA_RECOVER_IMAGE_INFO_CHAR,
    STM_FOTA_CONFIRMED_STATUS_CHAR,
    STM_FOTA_WRITE_FRAME_DATA_CHAR,
    STM_FOTA_FINISH_IMAGE_CHAR,
    STM_FOTA_ICON_STATUS_CHAR,
} E_STMFotaProfileCharacteristicsTypeDef;

typedef enum E_EspFotaProfilCharacteristics {
    ESP_FOTA_NOTIFY_NEW_VERSION_CHAR = 0,
    ESP_FOTA_CONFIRMED_DOWNLOAD_CHAR,
    ESP_FOTA_DOWNLOADING_IMAGE_CHAR,
} E_EspFotaProfilCharacteristicsTypeDef;

typedef enum E_EspProfileCharacteristics {
    ESP_VERSION_CHAR = 0,
    ESP_REESTART_CHAR,
} E_EspProfileCharacteristicsTypeDef;

typedef enum E_DebugCharacteristic {
    DEBUG_PRESSURE_VALUE_CHAR,
    DEBUG_FILTERED_PRESSURE_CHAR,
    DEBUG_PRESSURE_BASELINE_CHAR,
    DEBUG_PRESSURE_DERIVATIVE_CHAR,
    DEBUG_PRESSURE_REFERENCE_CHAR,
    DEBUG_FLOW_VALUE_CHAR,
    DEBUG_FILTERED_FLOW_CHAR,
    DEBUG_FLOW_BASELINE_CHAR,
    DEBUG_FLOW_DERIVATIVE_CHAR,
    DEBUG_FLOW_REFERENCE_CHAR,
} E_DebugProfileCharacteristicTypeDef;

typedef enum E_BluetoothIcon {
    BLUETOOTH_DISABLED_ICON = 0,
    BLUETOOTH_ENABLING_ICON = 1,
    BLUETOOTH_ENABLED_ICON  = 2,
    BLUETOOTH_PAIRED_ICON   = 3,
} E_BluetoothIconTypeDef;

typedef enum E_WiFiStatusConnection {
    WIFI_STATUS_ENABLED = 0,
    WIFI_STATUS_PROVISIONING,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
} E_WiFiStatusConnectionTypeDef;

typedef enum E_DownloadStatus {
    E_DOWNLOAD_STATUS_NONE,
    E_UNTIL_NOT_CONFIRMED_DOWNLOAD_IMAGE_STATUS,
    E_CONFIRMED_DOWNLOAD_IMAGE_STATUS,
    E_DOWNLOAD_ABORT_STATUS,
} E_DownloadStatusTypeDef;

typedef enum E_FwImageStatusDownload {
    E_FW_IMG_STATUS_NONE = 0,
    E_FW_IMG_STATUS_DOWNLOADING,
    E_FW_IMG_STATUS_DOWNLOAD_COMPLETED,
    E_FW_IMG_STATUS_DOWNLOAD_PAUSE,
} E_FwStatusDownloadTypeDef;

typedef struct ApiCredentials {
    char configrUrl[128];
    char coreUrl[128];
    char coreApplicationKey[128];
    char cpapClientId[32];
    char cpapSecretKey[128];
    char cpapApplicationKey[128];
    char hashCryptoAlgorithm[128];
    char password[32];
    uint8_t hashCryptoKey[AES_KEY_SIZE];
} ApiCredentialsTypeDef;

typedef struct S_TherapyParameteresConfiguration {
    /**  Tipo de parâmetro de terapia. */
    E_TherapyParametersTypeDef e_type;
    /**  @brief Esse é o "externalParameterName" */
    const char externalParameterName[48];
    /**
     * @brief Nome obrigatório que relaciona com o tipo de parâmetro (Regra da
     * Plataforma).
     */
    const char name[24];
    /**
     * @brief Atualizado por: CMOS_FW ou outro.
     */
    char lastUpdatedBy[24];
    /**
     * @brief Valor configurado atualmente.
     */
    uint8_t configuredValue;
    /**
     * @brief Identificação do grupo do perfil.
     */
    uint8_t profileId;
    /**
     * @brief Identificação do perfil.
     */
    uint8_t charId;
    /**
     * @brief Primeira configuração advinda do STM, durante a inicialização
     * do Esp.
     */
    volatile bool firstSettings;
    volatile bool uploaded;
} S_TherapyParameteresConfigurationTypeDef;

#define LIST_THERAPY_CONFIGURATIONS                                 \
    ENTRY_CONFIGURATION(HUMIDIFIER_LEVEL,                           \
                        STRINGFY(HUMIDITY),                         \
                        STRINGFY(Umidade),                          \
                        DEV_INFO_HUMIDIFIER_LEVEL_CHAR,             \
                        DEVICE_CONFIG_PROFILE_APP)                  \
                                                                    \
    ENTRY_CONFIGURATION(RAMP_TIME,                                  \
                        STRINGFY(RAMP_TIME),                        \
                        STRINGFY(Tempo de rampa),                   \
                        DEV_INFO_RAMP_TIME_CHAR,                    \
                        DEVICE_CONFIG_PROFILE_APP)                  \
                                                                    \
    ENTRY_CONFIGURATION(OUT_RELIEF_LEVEL,                           \
                        STRINGFY(OUT_RELIEF),                       \
                        STRINGFY(Alívio Respiratório),              \
                        DEV_INFO_OUT_RELIEF_LEVEL_CHAR,             \
                        DEVICE_CONFIG_PROFILE_APP)                  \
                                                                    \
    ENTRY_CONFIGURATION(MANUAL_ENABLE,                              \
                        STRINGFY(MANUAL_PRESSURE),                  \
                        STRINGFY(Pressão Manual),                   \
                        DEV_INFO_MANUAL_ENABLE_CHAR,                \
                        DEVICE_CONFIG_PROFILE_APP)                  \
                                                                    \
    ENTRY_CONFIGURATION(MANUAL_PRESSURE_VALUE,                      \
                        STRINGFY(MANUAL_PRESSURE_CONFIGURED_VALUE), \
                        STRINGFY(Valor Configurado),                \
                        DEV_INFO_THERAPY_PRESSURE_CHAR,             \
                        DEVICE_CONFIG_PROFILE_APP)                  \
                                                                    \
    ENTRY_CONFIGURATION(APAP_MIN,                                   \
                        STRINGFY(AUTOMATIC_PRESSURE_MIN_VALUE),     \
                        STRINGFY(Valor Mínimo),                     \
                        DEV_INFO_APAP_MIN_CHAR,                     \
                        DEVICE_CONFIG_PROFILE_APP)                  \
                                                                    \
    ENTRY_CONFIGURATION(APAP_MAX,                                   \
                        STRINGFY(AUTOMATIC_PRESSURE_MAX_VALUE),     \
                        STRINGFY(Valor Máximo),                     \
                        DEV_INFO_APAP_MAX_CHAR,                     \
                        DEVICE_CONFIG_PROFILE_APP)                  \
                                                                    \
    ENTRY_CONFIGURATION(AMBIENT_SOUND,                              \
                        STRINGFY(AMBIENT_SOUNDS),                   \
                        STRINGFY(Sons Ambiente),                    \
                        DEV_INFO_AMBIENT_SET_AUDIO_VOLUME_CHAR,     \
                        DEVICE_CONFIG_PROFILE_APP)                  \
                                                                    \
    ENTRY_CONFIGURATION(FLOW_RESPONSE,                              \
                        STRINGFY(FLOW_RESPONSE),                    \
                        STRINGFY(Resposta de fluxo),                \
                        DEV_INFO_FLOW_RESPONSE_ENABLE_CHAR,         \
                        DEVICE_CONFIG_PROFILE_APP)                  \
                                                                    \
    ENTRY_CONFIGURATION(LEAKAGE,                                    \
                        STRINGFY(MASK_LEAK),                        \
                        STRINGFY(Vazamento da Máscara),             \
                        DEV_INFO_MASK_LEAK_ENABLE_CHAR,             \
                        DEVICE_CONFIG_PROFILE_APP)

/**
 * @brief Quantidade de parâmetros da terapia.
 */
#define PARAMETER_THERAPY_COUNT 10UL

ApiCredentialsTypeDef *App_AppData_GetApiCredentials(void);

void App_AppData_SetSerialNumber(const uint64_t serialNumber);
const uint64_t App_AppData_GetSerialNumber(void);

void App_AppData_NotifySetSerialNumberCallback(void);

const uint32_t App_AppData_GetVersionFirmware(char *const p_version);

#if defined(__cplusplus)
}
#endif /** __cplusplus */

#endif /* __APP_DATA_H__ */
