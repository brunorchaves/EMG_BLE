/* Includes ------------------------------------------------------------------*/

#include "Tasks/EspFotaTask.h"

/* Private define ------------------------------------------------------------*/

/**
 * @brief Apelido utilizado logs desta tarefa.
 */
#define TAG __FILE__

/**
 * @brief Quantidade máxima de tentativas.
 */
#define MAX_ATTEMPTS 3UL

/**
 * @brief Tempo de espera antes de efetuar uma requisição.
 */
#define WAIT_CLIENT_REQUEST_TIMEOUT_MS 400UL

/**
 * @brief Tempo de atraso antes de solicitar o envio de uma requisição.
 */
#define WAIT_TIME_TO_REQUEST_USART_MS \
    (uint32_t)(3 * DRV_STM_USART_ACK_TIMEOUT_MS)  // 500U

/**
 * @brief Tempo de atraso necessário para aguardar uma resposta frente
 * a uma requisição enviada.
 */
#define WAIT_TIME_TO_RESPONSE_USART_MS 1000U

/**
 * @brief Tempo limite para envio do status de download da imagem.
 */
#define WAIT_TIME_TO_SEND_DOWNLOAD_IMAGE_STATUS 3000UL

/**
 * @brief Tempo limite para verificação do status de conexão com a internet.
 */
#define WAIT_TIME_TO_CHECK_CONNECTION_STATUS 5000UL

/**
 * @brief Tempo limite para aguardar o paciente confirmar o download da nova
 * versão de firmware do Esp.
 */
#define WAIT_TIME_TO_DOWNLOAD_CONFIRMATION 3000UL

/**
 * @brief Obtém o semáforo.
 */
#define __TAKE_SEMAPHORE(semaphore) (TAKE_SEMAPHORE(semaphore, portMAX_DELAY))

/* Private typedef -----------------------------------------------------------*/

typedef bool (*RequestFunctionTypeDef)(void);

/**
 * @brief Enumerador que identifica os estados da tarefa.
 */
typedef enum E_EspOtaTaskMachineState {
    E_ESP_FOTA_START = 0,
    E_ESP_FOTA_BEGIN,
    E_ESP_FOTA_GET_TOTAL_IMAGE_LENGTH,
    E_ESP_FOTA_CHECK_FIRMWARE_VERSION,
    E_ESP_FOTA_NOTIFY_NEW_VERSION,
    E_ESP_FOTA_AWAIT_DOWNLOAD_CONFIRMATION,
    E_ESP_FOTA_DOWNLOAD_IMAGES_BY_FRAMES,
    E_ESP_FOTA_CHECK_DOWNLOAD_IMAGE_COMPLETED,
    E_ESP_FOTA_FINISH,
    E_ESP_FOTA_ABORT,
    E_ESP_FOTA_TASK_SUSPEND,
    E_ESP_FOTA_TASK_DELETE,
} E_EspOtaTaskMachineStateTypeDef;

typedef struct S_ControlRegisters {
    bool shouldDeleteTask;

    uint8_t attempts;
    volatile E_EspOtaTaskMachineStateTypeDef stateMachine;

    uint32_t sendTimer;
    char versionAvailable[ESP_VERSION_LENGTH];

    struct {
        volatile bool answered;
        volatile bool requested;
    } volatile transaction;

    struct {
        volatile E_DownloadStatusTypeDef downloadStatus;
    } volatile response;

    struct {
        volatile bool notifyNewVersion;
        volatile bool confirmedDownload;
    } volatile flags;
} S_ControlRegistersTypeDef;

/* Private variables ---------------------------------------------------------*/

static const uint32_t TIMEOUT_MS = WAIT_CLIENT_REQUEST_TIMEOUT_MS;

static TaskHandle_t xHandle         = NULL;
static SemaphoreHandle_t xSemaphore = NULL;

static volatile S_ControlRegistersTypeDef *const volatile ps_taskRegisters =
    NULL;

/* Private function prototypes -----------------------------------------------*/

static uint32_t GetStackDepth(void);

static void Task_EspFota(void *p_parameters);
static bool UpdateMachineState(void);

static bool TransactionHandler(RequestFunctionTypeDef requestFunction);

static bool RequestToNotifyNewVersion(void);
static bool RequestToSendStatusDownloadingImage(
    const E_FwStatusDownloadTypeDef e_espStatusDownload);

static void ResponseToNewVersionNotified(uint8_t *const p_payload, uint8_t len);
static void ResponseToConfirmedDownload(uint8_t *const p_payload, uint8_t len);

static void ResumeTasksAll(void);
static void SuspendTasksAll(void);

/* Public functions ----------------------------------------------------------*/

/******************************************************************************
 * @fn          Task_EspFota_Create()
 *
 * @brief       Cria a tarefa.
 * @param       priority Prioridade para a tarefa.
 * @param       p_parameters Parâmetro genérico (Opcional).
 ******************************************************************************/
void Task_EspFota_Create(const uint8_t priority, void *p_parameters) {
    IF_TRUE_RETURN(xHandle);

    xTaskCreate(Task_EspFota,     // Funcao
                __FILE__,         // Nome
                GetStackDepth(),  // Pilha
                p_parameters,     // Parametro
                priority,         // Prioridade
                &xHandle);

    configASSERT(xHandle);

    if (!xSemaphore) {
        xSemaphore = xSemaphoreCreateBinary();
        configASSERT(xSemaphore);
    }

    if (xSemaphore) {
        GIVE_SEMAPHORE(xSemaphore);
    }

    if (!ps_taskRegisters) {
        size_t *ps_aux = (size_t *)&ps_taskRegisters;
        *ps_aux = (size_t)pvPortMalloc(sizeof(S_ControlRegistersTypeDef));
        configASSERT(ps_taskRegisters);

        memset((void *)ps_taskRegisters, 0, sizeof(*ps_taskRegisters));
    }
}
/******************************************************************************
 * @fn          Task_EspFota_Destroy()
 *
 * @brief       Encerra a tarefa.
 ******************************************************************************/
void Task_EspFota_Destroy(void) {
    IF_TRUE_RETURN(!xHandle);

#if defined(ENABLE_VERBOSE_ESP_FOTA_TASK)
    ESP_LOGI(TAG, "Deletando Tarefa...");
#endif

    if (ps_taskRegisters) {
        vPortFree((void *)ps_taskRegisters);

        size_t *ps_aux = (size_t *)&ps_taskRegisters;
        *ps_aux        = 0;

        configASSERT(!ps_taskRegisters);
    }

    GIVE_SEMAPHORE(xSemaphore);

    if (xSemaphore) {
        vSemaphoreDelete(xSemaphore);
        xSemaphore = NULL;

        configASSERT(!xSemaphore);
    }

    if (xHandle) {
        xHandle = NULL;
        configASSERT(!xHandle);
        vTaskDelete(NULL);
    }
}
/******************************************************************************
 * @fn          Task_EspFota_Resume()
 *
 * @brief       Resume a tarefa.
 ******************************************************************************/
void Task_EspFota_Resume(void) {
    IF_TRUE_RETURN(!xHandle);

    eTaskState taskState = eTaskGetState(xHandle);

    if (taskState == eSuspended) {
        vTaskResume(xHandle);
    } else if (taskState == eBlocked) {
        xTaskAbortDelay(xHandle);
    }
}
/******************************************************************************
 * @fn          Task_EspFota_Supend()
 *
 * @brief       Suspende a tarefa.
 ******************************************************************************/
void Task_EspFota_Supend(void) {
    IF_TRUE_RETURN(!xHandle);

    eTaskState taskState = eTaskGetState(xHandle);

    if (taskState != eSuspended) {
        vTaskSuspend(xHandle);
    }
}
/******************************************************************************
 * @fn          MainUc_EspFotaProfile_ResponseCallback()
 *
 * @brief       Callback chamado ao receber alguma resposta do STM.
 * @param       charId - Identificador do subcomando.
 * @param       charSize - Tamanho do payload, em bytes.
 * @param       parameterValue - Ponteiro para payload.
 ******************************************************************************/
void MainUc_EspFotaProfile_ResponseCallback(uint8_t charId,
                                            uint8_t charSize,
                                            uint8_t *const parameterValue) {
    IF_TRUE_RETURN(!ps_taskRegisters);
    IF_TRUE_RETURN(!ps_taskRegisters->transaction.requested);

    switch (charId) {
        case ESP_FOTA_NOTIFY_NEW_VERSION_CHAR:
            {
                ResponseToNewVersionNotified(parameterValue, charSize);
                break;
            }

        case ESP_FOTA_CONFIRMED_DOWNLOAD_CHAR:
            {
                ResponseToConfirmedDownload(parameterValue, charSize);
            }
        default:
            break;
    }
}

/* Private functions ---------------------------------------------------------*/

/******************************************************************************
 * @fn          GetStackDepth()
 *
 * @brief       Informa o tamanho da pilha exigida para a tarefa.
 ******************************************************************************/
static uint32_t GetStackDepth(void) { return 4096UL; }
/******************************************************************************
 * @fn          Task_EspFota()
 *
 * @brief       Gerencia o processo de atualização do firmware do STM.
 * @param       p_parameters Parâmetro genérico (Opcional).
 ******************************************************************************/
static void Task_EspFota(void *p_parameters) {
    vTaskDelay(pdMS_TO_TICKS(300));

    memset((void *)ps_taskRegisters->versionAvailable,
           0,
           sizeof(ps_taskRegisters->versionAvailable));

    ps_taskRegisters->attempts                = MAX_ATTEMPTS;
    ps_taskRegisters->stateMachine            = E_ESP_FOTA_START;
    ps_taskRegisters->response.downloadStatus = E_DOWNLOAD_STATUS_NONE;

    while (true) {
        if (UpdateMachineState()) {
            break;
        }
    }

    Task_EspFota_Destroy();
}
/******************************************************************************
 * @fn          UpdateMachineState()
 *
 * @brief       Atualiza a máquina de estado da tarefa principal.
 * @return      Booleano - Status de retorno.
 *              @retval False - Não faz nada.
 *              @retval True - Deletar tarefa.
 ******************************************************************************/
static bool UpdateMachineState(void) {
    if (!SRV_WifiProvision_IsConnected()) {
        if (ps_taskRegisters->stateMachine
            == E_ESP_FOTA_DOWNLOAD_IMAGES_BY_FRAMES) {
            ESP_LOGI(TAG, "Está entrando aqui...");
            RequestToSendStatusDownloadingImage(E_FW_IMG_STATUS_DOWNLOAD_PAUSE);
        } else {
            ESP_LOGI(TAG, "Está entrando aqui (2)...");
            RequestToSendStatusDownloadingImage(E_FW_IMG_STATUS_NONE);
        }

        vTaskDelay(pdMS_TO_TICKS(WAIT_TIME_TO_CHECK_CONNECTION_STATUS));
        return false;
    }

    switch (ps_taskRegisters->stateMachine) {
        case E_ESP_FOTA_START:
            {
                /**
                 * Aqui estamos garantindo que se alguma tarefa diferente desta
                 * obteve o Mutex, aguardamos para obtê-lo e suspendemos todas
                 * as tarefas, a fim de não causar DeadLock; em seguida,
                 * liberamos o Mutex para a que esta tarefa faça uso.
                 */
                if (SRV_EspFota_GetMutex()) {
                    SuspendTasksAll();
                    SRV_EspFota_GiveMutex();

#if defined(ENABLE_VERBOSE_ESP_FOTA_TASK)
                    ESP_LOGI(TAG, "Inicializando tarefa...");
#endif

                    ps_taskRegisters->attempts     = MAX_ATTEMPTS;
                    ps_taskRegisters->stateMachine = E_ESP_FOTA_BEGIN;
                }
                break;
            }

        case E_ESP_FOTA_BEGIN:
            {
#if defined(ENABLE_VERBOSE_ESP_FOTA_TASK)
                ESP_LOGI(TAG, "Iniciando FOTA...");
#endif

                if (!SRV_EspFota_Begin()) {
                    if (ps_taskRegisters->attempts) {
                        ps_taskRegisters->attempts -= 1;
                        vTaskDelay(pdMS_TO_TICKS(TIMEOUT_MS));
                    } else {
                        ps_taskRegisters->stateMachine =
                            E_ESP_FOTA_TASK_SUSPEND;
                    }
                    break;
                }

                vTaskDelay(pdMS_TO_TICKS(TIMEOUT_MS));

                ps_taskRegisters->attempts = MAX_ATTEMPTS;
                ps_taskRegisters->stateMachine =
                    E_ESP_FOTA_GET_TOTAL_IMAGE_LENGTH;
                break;
            }

        case E_ESP_FOTA_GET_TOTAL_IMAGE_LENGTH:
            {
#if defined(ENABLE_VERBOSE_ESP_FOTA_TASK)
                ESP_LOGI(TAG, "Obtendo tamanho total da imagem...");
#endif

                uint32_t imageTotalLength = SRV_EspFota_GetTotalImageLength();

                if (imageTotalLength) {
#if defined(ENABLE_VERBOSE_ESP_FOTA_TASK)
                    ESP_LOGI(TAG,
                             "Tamanho total da imagem: %lu",
                             imageTotalLength);
#endif

                    vTaskDelay(pdMS_TO_TICKS(TIMEOUT_MS));

                    ps_taskRegisters->attempts = MAX_ATTEMPTS;
                    ps_taskRegisters->stateMachine =
                        E_ESP_FOTA_CHECK_FIRMWARE_VERSION;
                } else if (ps_taskRegisters->attempts > 0) {
                    vTaskDelay(pdMS_TO_TICKS(TIMEOUT_MS));
                    ps_taskRegisters->attempts -= 1;
                } else {
                    ps_taskRegisters->stateMachine = E_ESP_FOTA_ABORT;
                }
                break;
            }

        case E_ESP_FOTA_CHECK_FIRMWARE_VERSION:
            {
                bool isNewVersion = false;

#if defined(ENABLE_VERBOSE_ESP_FOTA_TASK)
                ESP_LOGI(TAG, "Verificando versão do firmware...");
#endif

                if (SRV_EspFota_CheckVersion(
                        &isNewVersion,
                        (char *)ps_taskRegisters->versionAvailable)
                    != APP_OK) {
                    if (ps_taskRegisters->attempts > 0) {
                        vTaskDelay(pdMS_TO_TICKS(TIMEOUT_MS));
                        ps_taskRegisters->attempts -= 1;
                    } else {
                        ps_taskRegisters->stateMachine = E_ESP_FOTA_ABORT;
                    }
                    break;
                } else if (!isNewVersion) {
#if defined(ENABLE_VERBOSE_ESP_FOTA_TASK)
                    ESP_LOGW(TAG,
                             "A versão disponível é igual ou inferior à "
                             "versão em "
                             "execução: V%s.",
                             (char *)ps_taskRegisters->versionAvailable);
#endif

                    RequestToSendStatusDownloadingImage(E_FW_IMG_STATUS_NONE);

                    ps_taskRegisters->shouldDeleteTask = true;
                    ps_taskRegisters->stateMachine     = E_ESP_FOTA_ABORT;
                } else {
#if defined(ENABLE_VERBOSE_ESP_FOTA_TASK)
                    ESP_LOGI(TAG,
                             "Nova versão disponível: V%s",
                             (char *)ps_taskRegisters->versionAvailable);
#endif

                    vTaskDelay(pdMS_TO_TICKS(TIMEOUT_MS));

                    ps_taskRegisters->attempts = MAX_ATTEMPTS;
                    ps_taskRegisters->stateMachine =
                        E_ESP_FOTA_NOTIFY_NEW_VERSION;
                }

                break;
            }

        case E_ESP_FOTA_NOTIFY_NEW_VERSION:
            {
#if defined(ENABLE_VERBOSE_ESP_FOTA_TASK)
                ESP_LOGI(TAG, "Notificando nova versão...");
#endif
                if (!TransactionHandler(RequestToNotifyNewVersion)) {
                    break;
                }

                if (ps_taskRegisters->response.downloadStatus
                    == E_UNTIL_NOT_CONFIRMED_DOWNLOAD_IMAGE_STATUS) {
                    ps_taskRegisters->stateMachine =
                        E_ESP_FOTA_AWAIT_DOWNLOAD_CONFIRMATION;
                } else if (ps_taskRegisters->response.downloadStatus
                           == E_CONFIRMED_DOWNLOAD_IMAGE_STATUS) {
                    ps_taskRegisters->stateMachine =
                        E_ESP_FOTA_DOWNLOAD_IMAGES_BY_FRAMES;
                } else if (ps_taskRegisters->response.downloadStatus
                           == E_DOWNLOAD_ABORT_STATUS) {
                    ps_taskRegisters->stateMachine = E_ESP_FOTA_ABORT;
                }
                break;
            }

        case E_ESP_FOTA_AWAIT_DOWNLOAD_CONFIRMATION:
            {
                vTaskDelay(pdMS_TO_TICKS(WAIT_TIME_TO_DOWNLOAD_CONFIRMATION));

                if (ps_taskRegisters->response.downloadStatus
                    == E_CONFIRMED_DOWNLOAD_IMAGE_STATUS) {
                    RequestToSendStatusDownloadingImage(
                        E_FW_IMG_STATUS_DOWNLOADING);
                    ps_taskRegisters->sendTimer = UTILS_Timer_Now();
                    ps_taskRegisters->stateMachine =
                        E_ESP_FOTA_DOWNLOAD_IMAGES_BY_FRAMES;
                } else if (ps_taskRegisters->response.downloadStatus
                           == E_DOWNLOAD_ABORT_STATUS) {
                    ps_taskRegisters->stateMachine = E_ESP_FOTA_ABORT;
                } else {
#if defined(ENABLE_VERBOSE_ESP_FOTA_TASK)
                    ESP_LOGI(TAG, "Requisitando confirmação...");
#endif
                    ps_taskRegisters->stateMachine =
                        E_ESP_FOTA_NOTIFY_NEW_VERSION;
                }
                break;
            }

        case E_ESP_FOTA_DOWNLOAD_IMAGES_BY_FRAMES:
            {
                int statusCode = 0;
                E_AppStatusTypeDef appStatusCode =
                    SRV_EspFota_DownloadPartialImage(&statusCode);

                if (statusCode >= HttpStatus_Forbidden) {
                    ps_taskRegisters->stateMachine = E_ESP_FOTA_ABORT;
                } else if (appStatusCode == APP_OK) {
                    ps_taskRegisters->attempts = MAX_ATTEMPTS;
                    ps_taskRegisters->stateMachine =
                        E_ESP_FOTA_CHECK_DOWNLOAD_IMAGE_COMPLETED;
                } else if (appStatusCode == APP_IS_BUSY) {
                    ps_taskRegisters->attempts = MAX_ATTEMPTS;

                    if (UTILS_Timer_WaitTimeMSec(
                            ps_taskRegisters->sendTimer,
                            WAIT_TIME_TO_SEND_DOWNLOAD_IMAGE_STATUS)) {
                        ps_taskRegisters->sendTimer = UTILS_Timer_Now();
                        RequestToSendStatusDownloadingImage(
                            E_FW_IMG_STATUS_DOWNLOADING);
                    }

#if defined(ENABLE_VERBOSE_ESP_FOTA_TASK)
                    ESP_LOGI(TAG, "Baixando imagem...");
#endif
                } else if (ps_taskRegisters->attempts > 0) {
                    ps_taskRegisters->attempts = ps_taskRegisters->attempts - 1;
                    RequestToSendStatusDownloadingImage(
                        E_FW_IMG_STATUS_DOWNLOAD_PAUSE);
                    vTaskDelay(
                        pdMS_TO_TICKS(WAIT_TIME_TO_SEND_DOWNLOAD_IMAGE_STATUS));

                } else {
                    ps_taskRegisters->stateMachine = E_ESP_FOTA_ABORT;
                }

                break;
            }

        case E_ESP_FOTA_CHECK_DOWNLOAD_IMAGE_COMPLETED:
            {
#if defined(ENABLE_VERBOSE_ESP_FOTA_TASK)
                ESP_LOGI(
                    TAG,
                    "Verificando se a imagem foi completamente baixada...");
#endif

                if (!SRV_EspFota_IsDownloadImageCompleted()) {
                    vTaskDelay(pdMS_TO_TICKS(TIMEOUT_MS));
                    ps_taskRegisters->attempts -= 1;

                    if (!ps_taskRegisters->attempts) {
                        ps_taskRegisters->stateMachine = E_ESP_FOTA_ABORT;
                    }
                    break;
                }

                vTaskDelay(pdMS_TO_TICKS(TIMEOUT_MS));
                ps_taskRegisters->attempts     = MAX_ATTEMPTS;
                ps_taskRegisters->stateMachine = E_ESP_FOTA_FINISH;
                break;
            }

        case E_ESP_FOTA_FINISH:
            {
#if defined(ENABLE_VERBOSE_ESP_FOTA_TASK)
                ESP_LOGI(TAG, "Finalizando...");
#endif

                if (!SRV_EspFota_Finish()) {
                    ps_taskRegisters->attempts -= 1;
                    vTaskDelay(pdMS_TO_TICKS(TIMEOUT_MS));

                    if (!ps_taskRegisters->attempts) {
                        ps_taskRegisters->stateMachine = E_ESP_FOTA_ABORT;
                    }
                    break;
                } else {
                    RequestToSendStatusDownloadingImage(
                        E_FW_IMG_STATUS_DOWNLOAD_COMPLETED);

#if defined(ENABLE_VERBOSE_ESP_FOTA_TASK)
                    ESP_LOGI(TAG, "Reinicie o ESP...");
#endif

                    ps_taskRegisters->stateMachine = E_ESP_FOTA_TASK_DELETE;
                }
                break;
            }

        case E_ESP_FOTA_ABORT:
            {
                vTaskDelay(pdMS_TO_TICKS(TIMEOUT_MS));

                if (SRV_EspFota_Abort()) {
                    if (ps_taskRegisters->shouldDeleteTask) {
                        ps_taskRegisters->stateMachine = E_ESP_FOTA_TASK_DELETE;
                    } else {
                        ps_taskRegisters->stateMachine =
                            E_ESP_FOTA_TASK_SUSPEND;
                    }
                }
                break;
            }

        case E_ESP_FOTA_TASK_SUSPEND:
            {
                if (!RequestToSendStatusDownloadingImage(
                        E_FW_IMG_STATUS_NONE)) {
                    break;
                }

#if defined(ENABLE_VERBOSE_ESP_FOTA_TASK)
                ESP_LOGI(TAG, "Suspendando tarefa...");
#endif

                ResumeTasksAll();
                vTaskDelay(pdMS_TO_TICKS(ESP_CHECK_FIRMWARE_PERIOD_MS));
                ps_taskRegisters->stateMachine = E_ESP_FOTA_START;
                break;
            }

        case E_ESP_FOTA_TASK_DELETE:
            {
                ResumeTasksAll();
                return true;
            }

        default:
            break;
    }

    return false;
}
/******************************************************************************
 * @fn          TransactionHandler()
 *
 * @brief       Gerencia transações entre Esp e STM.
 * @param       requestFunction Ponteiro para função a ser utilizada para
 * envio de requisição para o STM.
 * @return      Booleano - Status de retorno.
 *              @retval False - Requisição não enviada ou resposta não
 *recebida.
 *              @retval True - Transação executada com sucesso.
 ******************************************************************************/
static bool TransactionHandler(RequestFunctionTypeDef requestFunction) {
    IF_TRUE_RETURN(!requestFunction, false);

    if (!ps_taskRegisters->transaction.requested) {
        vTaskDelay(pdMS_TO_TICKS(WAIT_TIME_TO_REQUEST_USART_MS));

        if (!requestFunction()) {
            vTaskDelay(pdMS_TO_TICKS(WAIT_TIME_TO_REQUEST_USART_MS));
            return false;
        }

        if (__TAKE_SEMAPHORE(xSemaphore)) {
            ps_taskRegisters->transaction.answered  = false;
            ps_taskRegisters->transaction.requested = true;
            GIVE_SEMAPHORE(xSemaphore);
        }
    }

    taskYIELD();

    if (!ps_taskRegisters->transaction.answered) {
        vTaskDelay(pdMS_TO_TICKS(WAIT_TIME_TO_RESPONSE_USART_MS));
    }

    if (!ps_taskRegisters->transaction.answered) {
        if (__TAKE_SEMAPHORE(xSemaphore)) {
            ps_taskRegisters->transaction.requested = false;
            GIVE_SEMAPHORE(xSemaphore);
        }
        return false;
    }

    if (__TAKE_SEMAPHORE(xSemaphore)) {
        ps_taskRegisters->transaction.answered  = false;
        ps_taskRegisters->transaction.requested = false;
        GIVE_SEMAPHORE(xSemaphore);
    }

    return true;
}
/******************************************************************************
 * @fn          RequestToNotifyNewVersion()
 *
 * @brief       Notifica o STM a respeito da nova versão do Esp.
 * @return      Booleano - Status de retorno.
 *              @retval False - Requisição não enviada.
 *              @retval True - Requisição enviada com sucesso.
 ******************************************************************************/
static bool RequestToNotifyNewVersion(void) {
    if (!__TAKE_SEMAPHORE(xSemaphore)) {
        return false;
    }

    uint32_t newFwVersion = App_AppData_GetVersionFirmware(
        (char *)ps_taskRegisters->versionAvailable);

    if (!newFwVersion) {
        GIVE_SEMAPHORE(xSemaphore);
        return false;
    }

    ps_taskRegisters->response.downloadStatus = E_DOWNLOAD_STATUS_NONE;
    ps_taskRegisters->flags.notifyNewVersion  = false;

    if (MainUc_SRV_AppWriteEvtCB(ESP_FOTA_PROFILE_APP,
                                 ESP_FOTA_NOTIFY_NEW_VERSION_CHAR,
                                 sizeof(newFwVersion),
                                 &newFwVersion)
        != APP_OK) {
        GIVE_SEMAPHORE(xSemaphore);
        return false;
    }

    ps_taskRegisters->flags.notifyNewVersion = true;
    GIVE_SEMAPHORE(xSemaphore);

    return true;
}
/******************************************************************************
 * @fn          RequestToSendStatusDownloadingImage()
 *
 * @brief       Notifica o STM quanto ao status de download a imagem do Esp.
 * @param       e_espStatusDownload Status de download da imagem.
 * @return      Booleano - Status de retorno.
 *              @retval False - Requisição não enviada.
 *              @retval True - Requisição enviada com sucesso.
 ******************************************************************************/
static bool RequestToSendStatusDownloadingImage(
    const E_FwStatusDownloadTypeDef e_espStatusDownload) {
    if (__TAKE_SEMAPHORE(xSemaphore)) {
        uint8_t espStatus = (uint8_t)e_espStatusDownload;

        if (MainUc_SRV_AppWriteEvtCB(ESP_FOTA_PROFILE_APP,
                                     ESP_FOTA_DOWNLOADING_IMAGE_CHAR,
                                     sizeof(espStatus),
                                     &espStatus)
            != APP_OK) {
            GIVE_SEMAPHORE(xSemaphore);
            return false;
        }

        GIVE_SEMAPHORE(xSemaphore);
    }

    return true;
}
/******************************************************************************
 * @fn          ResponseToNewVersionNotified()
 *
 * @brief       Processa a resposta e informa se o STM respondeu à
 *requisição de notificação de nova versão recebida do Esp.
 * @param       p_payload Vetor com os dados.
 * @param       len Quantidade de dados no vetor, em bytes.
 ******************************************************************************/
static void ResponseToNewVersionNotified(uint8_t *const p_payload,
                                         uint8_t len) {
    IF_TRUE_RETURN(!ps_taskRegisters->flags.notifyNewVersion);
    IF_TRUE_RETURN(!len || !p_payload);

    if (!__TAKE_SEMAPHORE(xSemaphore)) {
        return;
    }

    ps_taskRegisters->flags.notifyNewVersion = false;

    memcpy((void *)&ps_taskRegisters->response.downloadStatus,
           (void *)p_payload,
           sizeof(uint8_t));

    ps_taskRegisters->transaction.answered = true;

    GIVE_SEMAPHORE(xSemaphore);
    Task_EspFota_Resume();
}
/******************************************************************************
 * @fn          ResponseToConfirmedDownload()
 *
 * @brief       Processa a resposta e informa se o STM respondeu à
 * confirmação de download por ação do paciente.
 * @param       p_payload Vetor com os dados.
 * @param       len Quantidade de dados no vetor, em bytes.
 ******************************************************************************/
static void ResponseToConfirmedDownload(uint8_t *const p_payload, uint8_t len) {
    IF_TRUE_RETURN(!ps_taskRegisters->flags.confirmedDownload);
    IF_TRUE_RETURN(!len || !p_payload);

    if (__TAKE_SEMAPHORE(xSemaphore)) {
        ps_taskRegisters->flags.confirmedDownload = false;

        memcpy((void *)&ps_taskRegisters->response.downloadStatus,
               (void *)p_payload,
               sizeof(*p_payload));

        ps_taskRegisters->transaction.answered = true;

        GIVE_SEMAPHORE(xSemaphore);
        Task_EspFota_Resume();
    }
}

static void SuspendTasksAll(void) {
    Task_Main_Suspend();
    Task_STMFota_Suspend();
    Task_FileUpload_Suspend();
}

static void ResumeTasksAll(void) {
    SRV_EspFota_GiveMutex();

    Task_Main_Resume();
    Task_STMFota_Resume();
    Task_FileUpload_Resume();
}
/*****************************END OF FILE**************************************/
