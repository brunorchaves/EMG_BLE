/* Includes ------------------------------------------------------------------*/

#include "Tasks/MainTask.h"

/* Private define ------------------------------------------------------------*/

/**
 * @brief Apelido utilizado para identificar as saídas de logs do ESP.
 */
#define TAG __FILE__

/**
 * @brief Conversão de tempo em segundos para microsegundos.
 */
#define SECONDS_TO_MICROS(value) (value * 1000000)

/**
 * @brief Intervalo de tempo para efetuar pesquisa por atualização em
 * parêmetros.
 */
#define FETCH_PARAMETERS_INTERVAL_S 60

/* Private typedef -----------------------------------------------------------*/

/**
 * @brief Enumerador para identificação dos estados da Task Main.
 */
typedef enum E_MainStateMachine {
    GET_EQUIPMENT_SERIAL_NUMBER = 0,
    SEND_ESP_RUNNING_VERSION,
    GET_THERAPY_DEVICE_PARAMETERS,
    GET_LOCAL_TIME,
    EXECUTE_CONFIGR_LOGIN,
    EXECUTE_CORE_LOGIN,
    RUN_FIRST_PARAMETER_EXTACTION,
    UPLOAD_LOG_FILE,
    UPDATE_THERAPY_PARAMETERS,
    UPDATE_TRACK_POSITION,
} E_MainStateMachineTypeDef;

/**
 * @brief Enumerador que modela os estados relacionados ao fluxo de atualização
 * de parâmetros.
 */
typedef enum E_ParameterStateMachine {
    PARAMETERS_VALUE_EXTRACTION,
    SEND_PACKAGES,
    WAIT_PACKAGES_BE_SENT,
    UPDATE_CLOUD_PARAMETER
} E_ParameterStateMachineTypeDef;

/* Private variables ---------------------------------------------------------*/

static TaskHandle_t xHandle = NULL;

static S_TherapyParameteresConfigurationTypeDef
    therapyConfiguration[PARAMETER_THERAPY_COUNT] = {};

// static volatile bool sendAlarmParameters = false;
// Define a flag to indicate whether it's time to fetch parameters
static volatile bool fetchParametersFlag = false;
// Define a flag to indicate whether the first fetch is done
static volatile bool isFirstParameterExtractionDone = false;

static volatile bool requestSerialNumber = false;

/* Private function prototypes -----------------------------------------------*/

static uint32_t GetStackDepth(void);
static void Task_Main(void *p_parameters);

static bool ParameterExtractionMachine(void);
static void parameters_timer_callback(void *arg);

static void SendPayloadOverRB(
    const S_TherapyParameteresConfigurationTypeDef *const ps_parameter);

/* Public functions ----------------------------------------------------------*/

/******************************************************************************
 * @fn          Task_FileUpload_Create()
 *
 * @brief       Cria a tarefa.
 * @param       priority Prioridade para a tarefa.
 * @param       p_parameters Parâmetro genérico (Opcional).
 ******************************************************************************/
void Task_Main_Create(const uint8_t priority, void *const p_parameters) {
    IF_TRUE_RETURN(xHandle);

    xTaskCreate(Task_Main,        // Funcao
                TAG,              // Nome
                GetStackDepth(),  // Pilha
                p_parameters,     // Parametro
                priority,         // Prioridade
                &xHandle);

    configASSERT(xHandle);
}
/******************************************************************************
 * @fn          Task_FileUpload_Destroy()
 *
 * @brief       Encerra a tarefa.
 ******************************************************************************/
void Task_Main_Destroy(void) {
    IF_TRUE_RETURN(!xHandle);

#if defined(ENABLE_VERBOSE_MAIN_TASK)
    ESP_LOGI(TAG, "Deletando Tarefa...");
#endif

    vTaskDelay(pdMS_TO_TICKS(5));

    if (xHandle) {
        xHandle = NULL;
        vTaskDelete(xHandle);
    }
}
/******************************************************************************
 * @fn          Task_Main_Resume()
 *
 * @brief       Resume a tarefa.
 ******************************************************************************/
void Task_Main_Resume(void) {
    IF_TRUE_RETURN(!xHandle);

    eTaskState taskState = eTaskGetState(xHandle);

    if (taskState == eSuspended) {
        vTaskResume(xHandle);
    } else if (taskState == eBlocked) {
        xTaskAbortDelay(xHandle);
    }
}
/******************************************************************************
 * @fn          Task_Main_Suspend()
 *
 * @brief       Suspende a tarefa.
 ******************************************************************************/
void Task_Main_Suspend(void) {
    IF_TRUE_RETURN(!xHandle);

    eTaskState taskState = eTaskGetState(xHandle);

    if (taskState != eSuspended) {
        vTaskSuspend(xHandle);
    }
}
/******************************************************************************
 * @fn          App_AppData_NotifySetSerialNumberCallback()
 *
 * @brief       Notifica o Esp quando receber o número serial advindo do STM.
 ******************************************************************************/
void App_AppData_NotifySetSerialNumberCallback(void) {
    if (requestSerialNumber) {
        requestSerialNumber = false;
        Task_Main_Resume();
    }
}
/* Private functions ---------------------------------------------------------*/

/******************************************************************************
 * @fn          Task_Main()
 *
 * @brief       Gerencia o fluxo de trabalho principal da aplicação.
 * @param       p_parameters Parâmetro genérico (Opcional).
 ******************************************************************************/
static void Task_Main(void *p_parameters) {
    vTaskDelay(100 / portTICK_PERIOD_MS);

    memset(therapyConfiguration, 0, sizeof(therapyConfiguration));

    char *secretKey = (char *)pvPortMalloc(128 * sizeof(char));
    char *id        = (char *)pvPortMalloc(32 * sizeof(char));

    esp_timer_handle_t parameter_timer;

    const esp_timer_create_args_t parameter_timer_args = {
        .name     = "parameter_timer",
        .callback = &parameters_timer_callback,
    };

    esp_timer_create(&parameter_timer_args, &parameter_timer);
    esp_timer_start_periodic(parameter_timer,
                             SECONDS_TO_MICROS(FETCH_PARAMETERS_INTERVAL_S));

    E_MainStateMachineTypeDef mainMachineState = GET_EQUIPMENT_SERIAL_NUMBER;

    vTaskDelay(100 / portTICK_PERIOD_MS);

    while (true) {
        if (mainMachineState > GET_THERAPY_DEVICE_PARAMETERS) {
            SRV_WifiProvision_WaitingConnect();
        }

        switch (mainMachineState) {
            case GET_EQUIPMENT_SERIAL_NUMBER:
                {
                    if (MainUc_SRV_AppWriteEvtCB((uint8_t)GENERAL_PROFILE_APP,
                                                 GEN_SERIAL_NUMBER_CHAR,
                                                 0,
                                                 NULL)
                        == APP_OK) {
                        requestSerialNumber = true;
                        vTaskDelay(2000 / portTICK_PERIOD_MS);

                        const uint64_t SERIAL_NUMBER =
                            App_AppData_GetSerialNumber();

                        if (SERIAL_NUMBER != 0) {
                            MainUc_SRV_SendEspRunningVersion();

                            ESP_LOGI(TAG,
                                     "Número serial do equipamento: %llu",
                                     SERIAL_NUMBER);
                            mainMachineState = SEND_ESP_RUNNING_VERSION;
                        }
                    }

                    break;
                }

            case SEND_ESP_RUNNING_VERSION:
                {
                    vTaskDelay(100 / portTICK_PERIOD_MS);

                    if (MainUc_SRV_SendEspRunningVersion()) {
                        mainMachineState = GET_THERAPY_DEVICE_PARAMETERS;
                    }
                    break;
                }

            case GET_THERAPY_DEVICE_PARAMETERS:
                {
                    if (MainUc_SRV_FirstSettingsParamDefinedAll()) {
                        ESP_LOGI(TAG, "Todos os parâmetros obtidos.");
                        mainMachineState = GET_LOCAL_TIME;
                    } else if (MainUc_SRV_RequestTherapyDeviceParametersAll()) {
                        ESP_LOGI(TAG, "Requisição dos parâmetros...");
                        taskYIELD();
                        vTaskDelay(500 / portTICK_PERIOD_MS);
                    }

                    break;
                }

            case GET_LOCAL_TIME:
                {
                    uint8_t wifiStatus = WIFI_STATUS_CONNECTING;

                    MainUc_SRV_AppWriteEvtCB(
                        (uint8_t)WIRELESS_CONNECTIONS_PROFILE_APP,
                        WIRELESS_CONNECTIONS_WIFI_STATE_CHAR,
                        sizeof(wifiStatus),
                        &wifiStatus);

                    SRV_NTP_ObtainTime();
                    SRV_NTP_PrintLocalTime();

                    mainMachineState = EXECUTE_CONFIGR_LOGIN;

                    break;
                }

            case EXECUTE_CONFIGR_LOGIN:
                {
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    SRV_Crypto_GenerateSecretKeyId(secretKey, id);

                    if (!SRV_HttpRequest_ExecuteConfigRLogin(secretKey, id)) {
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                        break;
                    }

                    mainMachineState = EXECUTE_CORE_LOGIN;
                    break;
                }

            case EXECUTE_CORE_LOGIN:
                {
                    vTaskDelay(100 / portTICK_PERIOD_MS);

                    if (!SRV_HttpRequest_ExecuteCoreLogin()) {
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                        break;
                    }
                    /**
                     * Informa ao STM o estado "conectado" da rede WiFi
                     */
                    uint8_t wifiStatus = WIFI_STATUS_CONNECTED;

                    MainUc_SRV_AppWriteEvtCB(
                        (uint8_t)WIRELESS_CONNECTIONS_PROFILE_APP,
                        WIRELESS_CONNECTIONS_WIFI_STATE_CHAR,
                        sizeof(wifiStatus),
                        &wifiStatus);

                    mainMachineState = RUN_FIRST_PARAMETER_EXTACTION;
                    break;
                }

            case RUN_FIRST_PARAMETER_EXTACTION:
                {
                    vTaskDelay(100 / portTICK_PERIOD_MS);

                    if (ParameterExtractionMachine()) {
                        ESP_LOGI(TAG, "Extração finalizada!");
#if defined(ENABLE_ESP_FOTA)
                        Task_EspFota_Resume();
                        taskYIELD();

    #if defined(ENABLE_STM_FOTA)
                        Task_STMFota_Resume();
    #endif /** ENABLE_STM_FOTA */

                        Task_FileUpload_Resume();
#else
    #if defined(ENABLE_STM_FOTA)
                        Task_STMFota_Resume();
    #endif /** ENABLE_STM_FOTA */

                        Task_FileUpload_Resume();
#endif

                        isFirstParameterExtractionDone = true;
                        mainMachineState = UPDATE_THERAPY_PARAMETERS;
                    }
                    break;
                }

            case UPDATE_THERAPY_PARAMETERS:
                {
                    MainUc_SRV_UpdateParametersTherapy();
                    mainMachineState = UPDATE_TRACK_POSITION;
                    break;
                }

            case UPDATE_TRACK_POSITION:
                {
                    if (TrackPositionProfile_SRV_GetPendingUpload()) {
                        S_TrackPositionTypeDef trackPosition;
                        TrackPositionProfile_SRV_GetTrackPosition(
                            &trackPosition);
                        if (SRV_HttpRequest_UpdateDeviceTrackPosition(
                                App_AppData_GetSerialNumber(),
                                &trackPosition)) {
                            TrackPositionProfile_SRV_SetPendingUpload(false);
                        }
                    }
                    mainMachineState = UPDATE_THERAPY_PARAMETERS;
                    break;
                }

            default:
                break;
        }

        if (fetchParametersFlag) {
            if (SRV_WifiProvision_IsConnected()) {
                ParameterExtractionMachine();
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    // O uso de realloc é imediato enquanto que o uso de free não.
    vPortFree((void *)id);
    vPortFree((void *)secretKey);

    id        = NULL;
    secretKey = NULL;

    Task_Main_Destroy();
}
/******************************************************************************
 * @fn          GetStackDepth()
 *
 * @brief       Informa o tamanho da pilha exigida para a tarefa.
 ******************************************************************************/
static uint32_t GetStackDepth(void) { return 8192UL; }
/******************************************************************************
 * @brief Função responsável pela logica de extração, salvamento e envio de
 *parametros da plataforma para o dispositivo
 *
 * @returns bool
 * @return true - Se as etapas foram cumpridas
 * @return faes - Caso contrário
 ******************************************************************************/
static bool ParameterExtractionMachine() {
    static uint8_t parameterMachineStep = PARAMETERS_VALUE_EXTRACTION;
    static uint8_t packagePos           = 0;
    static uint8_t packagesSent         = 0;
    static uint8_t packagesErrors       = 0;
    const uint8_t packagesQty           = PARAMETER_THERAPY_COUNT;
    static bool lastPackageStatus       = false;

    switch (parameterMachineStep) {
        case PARAMETERS_VALUE_EXTRACTION:
            {
                packagePos = 0;
                MainUc_SRV_GetTherapyConfiguration(therapyConfiguration);

                const bool requestStatus =
                    SRV_HttpRequest_GetEquipmentParameters(
                        App_AppData_GetSerialNumber(),
                        therapyConfiguration,
                        GET_SIZE_ARRAY(therapyConfiguration));

                if (!requestStatus) {
                    parameterMachineStep = PARAMETERS_VALUE_EXTRACTION;
                } else {
                    MainUc_SRV_SetTherapyConfiguration(therapyConfiguration);
                    parameterMachineStep = SEND_PACKAGES;
                }
                break;
            }

        case SEND_PACKAGES:
            {
                if (packagePos >= packagesQty) {
                    packagePos           = 0;
                    parameterMachineStep = PARAMETERS_VALUE_EXTRACTION;

#if defined(ENABLE_VERBOSE_MAIN_TASK)
                    printf("\r\n Todos parâmetros enviados! \n");
                    printf("\r\n Erro em %d de %d pacotes \n",
                           packagesErrors,
                           packagesSent);
#endif

                    packagesSent        = 0;
                    packagesErrors      = 0;
                    fetchParametersFlag = false;

                    return true;
                }

                lastPackageStatus = DRV_STMUsart_GetLastPackageStatus();

                if (!lastPackageStatus) {
                    packagesErrors++;
                    DRV_STMUsart_SetLastPackageStatus(!lastPackageStatus);
                }

                if (strcmp(therapyConfiguration[packagePos].lastUpdatedBy,
                           UPDATED_BY_FW)
                    != 0) {
                    SendPayloadOverRB(&therapyConfiguration[packagePos]);
                    packagesSent++;
                    parameterMachineStep = WAIT_PACKAGES_BE_SENT;
                } else {
                    packagePos++;
                    parameterMachineStep = SEND_PACKAGES;
                }

                break;
            }

        case WAIT_PACKAGES_BE_SENT:
            {
                if (DRV_STMUsart_GetLastPackageStatus()) {
                    parameterMachineStep = UPDATE_CLOUD_PARAMETER;
                }
                break;
            }

        case UPDATE_CLOUD_PARAMETER:
            {
                packagePos++;
                parameterMachineStep = SEND_PACKAGES;

                vTaskDelay(1000 / portTICK_PERIOD_MS);
                break;
            }

        default:
            break;
    }

    return false;
}

static void SendPayloadOverRB(
    const S_TherapyParameteresConfigurationTypeDef *const ps_parameter) {
    const uint8_t ProfileID = 1;

#if defined(ENABLE_VERBOSE_MAIN_TASK)
    printf("\r\nConfigurando \"%s\" = %d\n",
           ps_parameter->externalParameterName,
           ps_parameter->configuredValue);
#endif

    MainUc_SRV_AppWriteEvtCB(ps_parameter->profileId,
                             ps_parameter->charId,
                             sizeof(ps_parameter->configuredValue),
                             &ps_parameter->configuredValue);
}

// Callback for the timer (every 5 minutes)
static void parameters_timer_callback(void *arg) {
    if (isFirstParameterExtractionDone) {
        fetchParametersFlag = true;
        // sendAlarmParameters = false;

#if defined(ENABLE_VERBOSE_MAIN_TASK)
        ESP_LOGI(__FUNCTION__, "Timeout para buscar parâmetros de terapia");
#endif
    }
}
