/* Includes ------------------------------------------------------------------*/

#include "Tasks/STMFotaTask.h"

/* Private define ------------------------------------------------------------*/

/**
 * @brief Apelido utilizado para identificar as saídas de logs do ESP.
 */
#define TAG __FILE__

/**
 * @brief Tamanho máximo para download da imagem do STM32, em bytes.
 */
#define BLOCK_DATA_LENGTH 2048U

/**
 * @brief Tamanho máximo da escrita de dados na flash do STM32, em bytes.
 */
#define FRAME_DATA_LENGTH (BLOCK_DATA_LENGTH / 16)

/**
 * @brief Macro para garantir aviso ao usuário para tratativa correta.
 */
// #if BLOCK_DATA_LENGTH < FRAME_DATA_LENGTH
//     #error \/
//         "Please, define the value of MACRO 'FRAME_DATA_LENGTH' lower than
//         value of MACRO 'BLOCK_DATA_LENGTH'.
// #endif

// #if FRAME_DATA_LENGTH == 0 || FRAME_DATA_LENGTH % 4 != 0
//     #error \/
//         "The MACRO 'FRAME_DATA_LENGTH' must be must be greater than 0 and
//         divisible by 4"
// #endif

/**
 * @brief Tempo de espera antes de inserir uma requisição na fila de
 * transmissão.
 */
#define WAIT_TIME_AFTER_SEND_REQUEST_MS \
    (uint32_t)(3 * DRV_STM_USART_ACK_TIMEOUT_MS)  // 500UL

/**
 * @brief Tempo de espera após de inserir uma requisição na fila de
 * transmissão.
 */
#define WAIT_TIME_BEFORE_SEND_REQUEST_MS 1000UL

/**
 * @brief Tempo de espera antes de efetuar uma requisição HTTP.
 */
#define WAIT_TIME_BEFORE_HTTP_REQUEST_MS 400U

/**
 * @brief Tempo limite para verificação do status de conexão com a internet.
 */
#define WAIT_TIME_TO_CHECK_CONNECTION_STATUS 5000UL

/**
 * @brief Tamanho da região de compartilhamento de dados.
 */
#define SHARED_DATA_LENGTH (2UL * 1024UL)

/**
 * @brief Tamanho da região da aplicação, seja principal ou secundária.
 */
#define APPLICATION_LENGTH (276UL * 1024UL)

/**
 * @brief Tamanho total da imagem a ser baixada
 */
#define APP_TOTAL_IMAGE_LENGTH (SHARED_DATA_LENGTH + APPLICATION_LENGTH)

/**
 * @brief Obtém o semáforo.
 */
#define __TAKE_SEMAPHORE(semaphore) (TAKE_SEMAPHORE(semaphore, portMAX_DELAY))

/* Private typedef -----------------------------------------------------------*/

typedef bool (*STMTaskRequestFunctionTypeDef)(void);

typedef enum E_STMFotaTaskMachineState {
    E_TASK_INIT = 0,
    E_TASK_GET_INFO_VERSION_AVAILABLE,
    E_TASK_GET_STM_VERSION_RUNNING,
    E_TASK_COMPARE_VERSIONS,
    E_TASK_GET_STM_PROGRESS_IMAGE_INFO,
    E_TASK_GET_TOTAL_IMAGE_LENGTH,
    E_TASK_RECOVER_CONFIRM_STATUS,
    E_TASK_DOWNLOAD_IMAGE_BY_FRAMES,
    E_TASK_SEND_BLOCK_IMAGE_BY_FRAMES,
    E_TASK_DEINIT,
    E_TASK_SUSPEND,
} E_STMFotaTaskMachineStateTypeDef;

typedef enum E_BitsGroupStatus {
    E_SEND_REQUEST = 0,
    E_WAIT_RESPONSE,
    E_GET_VERSION_RUNNING,
    E_RECOVERY_IMAGE_INFO,
    E_WRITE_FRAME_STATUS,
} E_BitsGroupStatusTypeDef;

typedef union U_STMImageBitFieldsStatus {
    uint8_t all;

    struct {
        uint8_t resyncInfo  : 1;  // 1
        uint8_t failToWrite : 1;  // 2
        uint8_t powerOff    : 1;  // 4
    } bitFields;
} U_STMImageBitFieldsStatusTypeDef;

typedef struct S_ControlRegisters {
    bool shouldDeleteTask;
    volatile E_STMFotaTaskMachineStateTypeDef machineState;

    struct {
        volatile uint32_t totalImageLen;
        uint8_t versionBuffer[SRV_STM_FOTA_VERSION_BYTES_LENGTH];
        char versionFormatted[SRV_STM_FW_VERSION_MAX_LENGTH];
    } volatile imageAvailable;

    volatile U_STMImageBitFieldsStatusTypeDef u_status;

    struct {
        E_DownloadStatusTypeDef confirmStatus;
        volatile uint8_t framePos;
        volatile uint16_t blockPos;

        volatile uint16_t totalReadBytes;
        volatile uint32_t nextCheckOffset;
        uint8_t frameDownload[BLOCK_DATA_LENGTH];
    } volatile imageDownload;

    struct {
        uint8_t versionBuffer[SRV_STM_FOTA_VERSION_BYTES_LENGTH];
        volatile char versionFormatted[SRV_STM_FW_VERSION_MAX_LENGTH];
    } volatile imageRunning;

    struct {
        uint16_t offsetWrite;
        uint16_t writeBytes;
    } volatile flash;

    struct {
        volatile bool answered;
        volatile bool requested;
    } volatile transaction;

    struct {
        volatile uint16_t blockPos;
        volatile uint8_t framePos;
    } volatile response;

    struct {
        volatile bool getVersionRunning;
        volatile bool getRecoveryInfoImage;
        volatile bool getFrameDataStatus;
        volatile bool getConfirmedStatus;
    } volatile flags;

    struct {
        const uint32_t timeout_ms;
    } timer;

} S_ControlRegistersTypeDef;

/* Private variables ---------------------------------------------------------*/

static TaskHandle_t xHandle         = NULL;
static SemaphoreHandle_t xSemaphore = NULL;

static volatile S_ControlRegistersTypeDef *const volatile ps_taskRegisters =
    NULL;

/* Private function prototypes -----------------------------------------------*/

static uint32_t GetStackDepth(void);
static void Task_STMFota(void *p_parameters);

static bool UpdateMachineState(void);

static void TaskStateInit(void);
static void TaskStateGetInfoVersionAvailable(void);
static void TaskStateGetSTMRunningVersion(void);
static void TaskStateCompareSTMVersions(void);
static void TaskStateGetTotalImageLength(void);
static void TaskStateRecoverConfirmStatus(void);
static void TaskStateGetDownloadImageProgress(void);
static void TaskStateDownloadImageByFrames(void);
static void TaskStateSendBlockImageByFrames(void);
static bool TaskStateDeInit(void);
static void TaskStateSuspend(void);

static bool TransactionHandler(STMTaskRequestFunctionTypeDef requestFunction);

static void SetDefaultRegisters(void);
static bool RequestToGetVersionRunning(void);
static bool RequestConfirmedStatus(void);
static bool RequestToGetImageInfo(void);
static bool RequestToWriteFrameDataImage(void);
static bool RequestDownloadImageCompleted(void);
static bool RequestDownloadIconStatus(
    E_FwStatusDownloadTypeDef fwStatusDownload);

static uint16_t GetStatusAll(uint8_t *const p_payload);

static void GetResponseVersionRunning(const uint8_t *const p_payload,
                                      const uint8_t len);
static void GetResponseConfirmedStatus(const uint8_t *const p_payload,
                                       const uint8_t len);
static void GetResponseWriteFrameStatus(const uint8_t *const p_data,
                                        const uint8_t len);
static void GetResponseRecoveryImageInfo(const uint8_t *const p_payload,
                                         const uint16_t len);

static uint32_t GetOffsetImage(void);
static float GetProgressImagePercentage(void);

static void ComputeNextCheckProgressImage(void);
static const bool IsTimeToCheckIfHasVersionChanged(void);

/* Public functions ----------------------------------------------------------*/

/******************************************************************************
 * @fn          Task_STMFota_Create()
 *
 * @brief       Cria a tarefa.
 * @param       priority Prioridade para a tarefa.
 * @param       p_parameters Parâmetro genérico (Opcional).
 ******************************************************************************/
void Task_STMFota_Create(const uint8_t priority, void *p_parameters) {
    IF_TRUE_RETURN(xHandle);

    xTaskCreate(Task_STMFota,     // Funcao
                __FILE__,         // Nome
                GetStackDepth(),  // Pilha
                p_parameters,     // Parametro
                priority,         // Prioridade
                &xHandle);

    configASSERT(xHandle);

    if (!ps_taskRegisters) {
        size_t *ps_aux = (size_t *)&ps_taskRegisters;
        *ps_aux = (size_t)pvPortMalloc(sizeof(S_ControlRegistersTypeDef));
        configASSERT(ps_taskRegisters);

        memset((void *)ps_taskRegisters, 0, sizeof(*ps_taskRegisters));
    }

    if (!xSemaphore) {
        xSemaphore = xSemaphoreCreateBinary();
        configASSERT(xSemaphore);
    }

    if (xSemaphore) {
        GIVE_SEMAPHORE(xSemaphore);
    }
}
/******************************************************************************
 * @fn          Task_STMFota_Destroy()
 *
 * @brief       Encerra a tarefa.
 ******************************************************************************/
void Task_STMFota_Destroy(void) {
    IF_TRUE_RETURN(!xHandle);

#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
    ESP_LOGI(TAG, "Deletando Tarefa...");
#endif

    while (!SRV_STMFota_DeInit()) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }

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
 * @fn          Task_STMFota_Resume()
 *
 * @brief       Resume a tarefa.
 ******************************************************************************/
void Task_STMFota_Resume(void) {
    IF_TRUE_RETURN(!xHandle);

    eTaskState taskState = eTaskGetState(xHandle);

    if (taskState == eSuspended) {
        vTaskResume(xHandle);
    } else if (taskState == eBlocked) {
        xTaskAbortDelay(xHandle);
    }
}
/******************************************************************************
 * @fn          Task_STMFota_Suspend()
 *
 * @brief       Suspende a tarefa.
 ******************************************************************************/
void Task_STMFota_Suspend(void) {
    IF_TRUE_RETURN(!xHandle);

    eTaskState taskState = eTaskGetState(xHandle);

    if (taskState != eSuspended) {
        vTaskSuspend(xHandle);
    }
}
/******************************************************************************
 * @fn          MainUc_STMFotaProfile_ResponseCallback()
 *
 * @brief       Callback chamado ao receber alguma resposta do STM.
 * @param       charId - Identificador do subcomando.
 * @param       charSize - Tamanho do payload, em bytes.
 * @param       parameterValue - Ponteiro para payload.
 ******************************************************************************/
void MainUc_STMFotaProfile_ResponseCallback(uint8_t charId,
                                            uint8_t charSize,
                                            uint8_t *const parameterValue) {
    IF_TRUE_RETURN(!ps_taskRegisters);
    IF_TRUE_RETURN(!ps_taskRegisters->transaction.requested);

    switch (charId) {
        case STM_FOTA_GET_VERSION_RUNNING_CHAR:
            {
                GetResponseVersionRunning(parameterValue, charSize);
                break;
            }

        case STM_FOTA_RECOVER_IMAGE_INFO_CHAR:
            {
                GetResponseRecoveryImageInfo(parameterValue, charSize);
                break;
            }

        case STM_FOTA_WRITE_FRAME_DATA_CHAR:
            {
                GetResponseWriteFrameStatus(parameterValue, charSize);
                break;
            }

        case STM_FOTA_CONFIRMED_STATUS_CHAR:
            {
                GetResponseConfirmedStatus(parameterValue, charSize);
                break;
            }

        case STM_FOTA_FINISH_IMAGE_CHAR:
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
 * @fn          Task_STMFota()
 *
 * @brief       Gerencia o processo de atualização do firmware do STM.
 * @param       p_parameters Parâmetro genérico (Opcional).
 ******************************************************************************/
static void Task_STMFota(void *p_parameters) {
    vTaskDelay(pdMS_TO_TICKS(300));

    SetDefaultRegisters();
    ps_taskRegisters->machineState = E_TASK_INIT;

    while (true) {
        if (UpdateMachineState()) {
            break;
        }
    }

    Task_STMFota_Destroy();
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
        if (ps_taskRegisters->machineState >= E_TASK_DOWNLOAD_IMAGE_BY_FRAMES
            && ps_taskRegisters->machineState
                   <= E_TASK_SEND_BLOCK_IMAGE_BY_FRAMES) {
            RequestDownloadIconStatus(E_FW_IMG_STATUS_DOWNLOAD_PAUSE);
        }

        vTaskDelay(pdMS_TO_TICKS(WAIT_TIME_TO_CHECK_CONNECTION_STATUS));
        return false;
    }

    switch (ps_taskRegisters->machineState) {
        case E_TASK_INIT:
            {
                TaskStateInit();
                break;
            }

        case E_TASK_GET_INFO_VERSION_AVAILABLE:
            {
                TaskStateGetInfoVersionAvailable();
                break;
            }

        case E_TASK_GET_STM_VERSION_RUNNING:
            {
                TaskStateGetSTMRunningVersion();
                break;
            }

        case E_TASK_COMPARE_VERSIONS:
            {
                TaskStateCompareSTMVersions();
                break;
            }

        case E_TASK_GET_TOTAL_IMAGE_LENGTH:
            {
                TaskStateGetTotalImageLength();
                break;
            }

        case E_TASK_RECOVER_CONFIRM_STATUS:
            {
                TaskStateRecoverConfirmStatus();
                break;
            }

        case E_TASK_GET_STM_PROGRESS_IMAGE_INFO:
            {
                TaskStateGetDownloadImageProgress();
                break;
            }

        case E_TASK_DOWNLOAD_IMAGE_BY_FRAMES:
            {
                TaskStateDownloadImageByFrames();
                break;
            }

        case E_TASK_SEND_BLOCK_IMAGE_BY_FRAMES:
            {
                TaskStateSendBlockImageByFrames();
                break;
            }

        case E_TASK_DEINIT:
            {
                bool shoulDeleteTask = TaskStateDeInit();

                if (shoulDeleteTask) {
                    return true;
                }

                break;
            }

        case E_TASK_SUSPEND:
            {
                TaskStateSuspend();
                break;
            }

        default:
            break;
    }

    return false;
}
/******************************************************************************
 * @fn          TaskStateInit()
 *
 * @brief       Gerencia o estado "E_TASK_INIT".
 ******************************************************************************/
static void TaskStateInit(void) {
#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
    ESP_LOGI(TAG, "Iniciando...");
#endif

    IF_TRUE_RETURN(!SRV_STMFota_Init());
    RequestDownloadIconStatus(E_FW_IMG_STATUS_NONE);

    ps_taskRegisters->machineState = E_TASK_GET_INFO_VERSION_AVAILABLE;
}
/******************************************************************************
 * @fn          TaskStateGetInfoVersionAvailable()
 *
 * @brief       Gerencia o estado "E_TASK_GET_INFO_VERSION_AVAILABLE".
 ******************************************************************************/
static void TaskStateGetInfoVersionAvailable(void) {
#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
    ESP_LOGI(TAG, "Obtendo versão disponível em nuvem...");
#endif

    vTaskDelay(pdMS_TO_TICKS(ps_taskRegisters->timer.timeout_ms));

    bool magicCodeValid = false;

    if (!SRV_STMFota_GetVersionAvailable(
            ps_taskRegisters->imageAvailable.versionBuffer,
            GET_SIZE_ARRAY(ps_taskRegisters->imageAvailable.versionBuffer),
            &magicCodeValid)) {
        return;
    }

    if (!magicCodeValid) {
#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
        ESP_LOGW(TAG,
                 "Binário disponível não corresponde à um "
                 "equipamento OXYGENIS...");
#endif

        ps_taskRegisters->shouldDeleteTask = true;
        ps_taskRegisters->machineState     = E_TASK_DEINIT;
        return;
    }

    SRV_STMFota_GetFirmwareVersionFormatted(
        ps_taskRegisters->imageAvailable.versionBuffer,
        ps_taskRegisters->imageAvailable.versionFormatted);

#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
    ESP_LOGI(TAG,
             "Versão disponível para o STM32: V%s",
             ps_taskRegisters->imageAvailable.versionFormatted);
#endif

    ps_taskRegisters->machineState = E_TASK_GET_STM_VERSION_RUNNING;
}
/******************************************************************************
 * @fn          TaskStateGetSTMRunningVersion()
 *
 * @brief       Gerencia o estado "E_TASK_GET_STM_VERSION_RUNNING".
 ******************************************************************************/
static void TaskStateGetSTMRunningVersion(void) {
#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
    ESP_LOGI(TAG, "Obtendo versão em execução...");
#endif

    IF_TRUE_RETURN(!TransactionHandler(&RequestToGetVersionRunning));

    SRV_STMFota_GetFirmwareVersionFormatted(
        ps_taskRegisters->imageRunning.versionBuffer,
        ps_taskRegisters->imageRunning.versionFormatted);

#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
    ESP_LOGI(TAG,
             "Versão em execução no STM32: V%s",
             ps_taskRegisters->imageRunning.versionFormatted);
#endif

    ps_taskRegisters->machineState = E_TASK_COMPARE_VERSIONS;
}
/******************************************************************************
 * @fn          TaskStateCompareSTMVersions()
 *
 * @brief       Gerencia o estado "E_TASK_COMPARE_VERSIONS".
 ******************************************************************************/
static void TaskStateCompareSTMVersions(void) {
    bool isNewVersion = SRV_STMFota_CheckFirmwareVersion(
        (char *)ps_taskRegisters->imageAvailable.versionFormatted,
        (char *)ps_taskRegisters->imageRunning.versionFormatted);

    if (isNewVersion) {
#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
        ESP_LOGI(TAG,
                 "A nova versão é maior que a versão atual em "
                 "execução!");
#endif

        ps_taskRegisters->machineState = E_TASK_GET_TOTAL_IMAGE_LENGTH;
    } else {
#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
        ESP_LOGW(TAG,
                 "A versão disponível é igual ou inferior à "
                 "versão em execução.");
#endif

        ps_taskRegisters->shouldDeleteTask = true;
        ps_taskRegisters->machineState     = E_TASK_DEINIT;
    }
}
/******************************************************************************
 * @fn          TaskStateGetTotalImageLength()
 *
 * @brief       Gerencia o estado "E_TASK_GET_TOTAL_IMAGE_LENGTH".
 ******************************************************************************/
static void TaskStateGetTotalImageLength(void) {
#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
    ESP_LOGI(TAG, "Obtendo tamanho total da imagem diponível em nuvem...");
#endif

    vTaskDelay(pdMS_TO_TICKS(ps_taskRegisters->timer.timeout_ms));

    uint32_t totalImageLen = 0;
    IF_TRUE_RETURN(!SRV_STMFota_GetTotalImageLength(&totalImageLen));

    if (totalImageLen >= APP_TOTAL_IMAGE_LENGTH) {
#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
        ESP_LOGI(TAG, "Tamanho total da imagem: %lu.", APP_TOTAL_IMAGE_LENGTH);
#endif

        ps_taskRegisters->machineState = E_TASK_RECOVER_CONFIRM_STATUS;
    } else {
#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
        ESP_LOGE(TAG,
                 "O tamanho da imagem do disponível é menor que o "
                 "esperado!");
#endif

        ps_taskRegisters->machineState = E_TASK_DEINIT;
    }
}
/******************************************************************************
 * @fn          TaskStateDownloadImageByFrames()
 *
 * @brief       Gerencia o estado "E_TASK_RECOVER_CONFIRM_STATUS".
 ******************************************************************************/
static void TaskStateRecoverConfirmStatus(void) {
#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
    ESP_LOGI(TAG, "Recuperando status de confirmação...");
#endif

    IF_TRUE_RETURN(!TransactionHandler(&RequestConfirmedStatus));

    if (ps_taskRegisters->imageDownload.confirmStatus
        == E_UNTIL_NOT_CONFIRMED_DOWNLOAD_IMAGE_STATUS) {
        vTaskDelay(pdMS_TO_TICKS(3000));
    } else if (ps_taskRegisters->imageDownload.confirmStatus
               == E_CONFIRMED_DOWNLOAD_IMAGE_STATUS) {
        ps_taskRegisters->machineState = E_TASK_GET_STM_PROGRESS_IMAGE_INFO;
    }
}
/******************************************************************************
 * @fn          TaskStateGetDownloadImageProgress()
 *
 * @brief       Gerencia o estado "E_TASK_GET_STM_PROGRESS_IMAGE_INFO".
 ******************************************************************************/
static void TaskStateGetDownloadImageProgress(void) {
#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
    ESP_LOGI(TAG, "Recuperando informações do progresso da imagem...");
#endif

    IF_TRUE_RETURN(!TransactionHandler(&RequestToGetImageInfo));

#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
    ESP_LOGI(TAG,
             "Progresso da imagem: %lu de %lu",
             GetOffsetImage(),
             APP_TOTAL_IMAGE_LENGTH);
#endif

    ComputeNextCheckProgressImage();
    RequestDownloadIconStatus(E_FW_IMG_STATUS_DOWNLOADING);

    ps_taskRegisters->machineState = E_TASK_DOWNLOAD_IMAGE_BY_FRAMES;
}
/******************************************************************************
 * @fn          TaskStateDownloadImageByFrames()
 *
 * @brief       Gerencia o estado "E_TASK_DOWNLOAD_IMAGE_BY_FRAMES".
 ******************************************************************************/
static void TaskStateDownloadImageByFrames(void) {
    if (IsTimeToCheckIfHasVersionChanged()) {
#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
        ESP_LOGI(TAG,
                 "Iniciando verificação de versão disponivel em "
                 "nuvem...");
#endif

        ps_taskRegisters->machineState = E_TASK_GET_INFO_VERSION_AVAILABLE;
        return;
    }

    uint32_t offsetImage = GetOffsetImage();

    if (offsetImage >= APP_TOTAL_IMAGE_LENGTH) {
#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
        float progress = GetProgressImagePercentage();
        ESP_LOGI(TAG,
                 "Baixando imagem: %lu bytes (%02.2f%%)...",
                 offsetImage,
                 progress);
#endif

        RequestDownloadIconStatus(E_FW_IMG_STATUS_DOWNLOAD_COMPLETED);

        ps_taskRegisters->shouldDeleteTask = true;
        ps_taskRegisters->machineState     = E_TASK_DEINIT;
        return;
    }

    ps_taskRegisters->imageDownload.totalReadBytes = 0UL;

    uint32_t restImageBytes = APP_TOTAL_IMAGE_LENGTH - offsetImage;

    uint16_t bytesToRead =
        MIN(restImageBytes,
            sizeof(ps_taskRegisters->imageDownload.frameDownload));

    vTaskDelay(pdMS_TO_TICKS(ps_taskRegisters->timer.timeout_ms));

    bool downloadFrameStatus = SRV_STMFota_StreamDownload(
        (uint8_t *)ps_taskRegisters->imageDownload.frameDownload,
        offsetImage,
        bytesToRead);

    if (!downloadFrameStatus) {
        RequestDownloadIconStatus(E_FW_IMG_STATUS_DOWNLOAD_PAUSE);
        return;
    }

    RequestDownloadIconStatus(E_FW_IMG_STATUS_DOWNLOADING);

    ps_taskRegisters->flash.writeBytes             = 0;
    ps_taskRegisters->flash.offsetWrite            = 0;
    ps_taskRegisters->imageDownload.totalReadBytes = bytesToRead;

    ps_taskRegisters->machineState = E_TASK_SEND_BLOCK_IMAGE_BY_FRAMES;

#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
    float progress = GetProgressImagePercentage();

    ESP_LOGI(TAG,
             "Download STM32 image %lu bytes (%02.2f%%)... ",
             offsetImage,
             progress);
#endif
}
/******************************************************************************
 * @fn          TaskStateSendBlockImageByFrames()
 *
 * @brief       Gerencia o estado "E_TASK_SEND_BLOCK_IMAGE_BY_FRAMES".
 ******************************************************************************/
static void TaskStateSendBlockImageByFrames(void) {
    ps_taskRegisters->flash.writeBytes =
        MIN((uint16_t)(ps_taskRegisters->imageDownload.totalReadBytes
                       - ps_taskRegisters->flash.offsetWrite),
            FRAME_DATA_LENGTH);

#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
    ESP_LOGI(TAG,
             "Offset write: %d, Bytes: %d, BlockPos: %d, FramePos: %d",
             ps_taskRegisters->flash.offsetWrite,
             ps_taskRegisters->flash.writeBytes,
             ps_taskRegisters->imageDownload.blockPos,
             ps_taskRegisters->imageDownload.framePos);
#endif

    if (!TransactionHandler(&RequestToWriteFrameDataImage)
        || ps_taskRegisters->u_status.bitFields.failToWrite) {
        return;
    }

    if (ps_taskRegisters->response.blockPos
        != ps_taskRegisters->imageDownload.blockPos) {
#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
        ESP_LOGI(TAG, "Resincronizando total...");
#endif
        ps_taskRegisters->machineState = E_TASK_GET_STM_PROGRESS_IMAGE_INFO;
        return;
    } else if (ps_taskRegisters->response.framePos
               != ps_taskRegisters->imageDownload.framePos) {
        ESP_LOGI(TAG, "Resincronizando apenas o framePos");

        ps_taskRegisters->imageDownload.framePos =
            ps_taskRegisters->response.framePos;

        ps_taskRegisters->flash.offsetWrite =
            ps_taskRegisters->imageDownload.framePos * FRAME_DATA_LENGTH;

        vTaskDelay(pdMS_TO_TICKS(WAIT_TIME_AFTER_SEND_REQUEST_MS));
        return;
    }

    ps_taskRegisters->flash.offsetWrite += ps_taskRegisters->flash.writeBytes;

    if (ps_taskRegisters->flash.offsetWrite
        >= ps_taskRegisters->imageDownload.totalReadBytes) {
        ps_taskRegisters->imageDownload.framePos = 0;
        ps_taskRegisters->imageDownload.blockPos += 1;
        ps_taskRegisters->machineState = E_TASK_DOWNLOAD_IMAGE_BY_FRAMES;
    } else {
        ps_taskRegisters->imageDownload.framePos += 1;
    }
}
/******************************************************************************
 * @fn          TaskStateDeInit()
 *
 * @brief       Gerencia o estado "E_TASK_DEINIT".
 * @return      Booleano - Status de retorno.
 *              @retval False - Não deve encerrar a tarefa.
 *              @retval True - Deve encerrar a tarefa imediatamente.
 ******************************************************************************/
static bool TaskStateDeInit(void) {
    if (!SRV_STMFota_DeInit()) {
        vTaskDelay(pdMS_TO_TICKS(ps_taskRegisters->timer.timeout_ms));
        return false;
    }

    if (ps_taskRegisters->shouldDeleteTask) {
        if (RequestDownloadImageCompleted()) {
            return true;
        }
    } else {
        ps_taskRegisters->machineState = E_TASK_SUSPEND;
    }

    return false;
}
/******************************************************************************
 * @fn          TaskStateDeInit()
 *
 * @brief       Gerencia o estado "E_TASK_SUSPEND".
 ******************************************************************************/
static void TaskStateSuspend(void) {
#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
    ESP_LOGI(TAG, "Suspendendo...");
#endif

    vTaskDelay(pdMS_TO_TICKS(STM_CHECK_FIRMWARE_PERIOD_MS));
    ps_taskRegisters->machineState = E_TASK_GET_INFO_VERSION_AVAILABLE;
}
/******************************************************************************
 * @fn          TransactionHandler()
 *
 * @brief       Gerencia transações entre Esp e STM.
 * @param       requestFunction Ponteiro para função a ser utilizada para
 * envio de requisição para o STM.
 * @return      Booleano - Status de retorno.
 *              @retval False - Requisição não enviada ou resposta não recebida.
 *              @retval True - Transação executada com sucesso.
 ******************************************************************************/
static bool TransactionHandler(STMTaskRequestFunctionTypeDef requestFunction) {
    IF_TRUE_RETURN(requestFunction == NULL, false);

    if (!ps_taskRegisters->transaction.requested) {
        vTaskDelay(pdMS_TO_TICKS(WAIT_TIME_AFTER_SEND_REQUEST_MS));

        if (!requestFunction()) {
            vTaskDelay(pdMS_TO_TICKS(WAIT_TIME_AFTER_SEND_REQUEST_MS));
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
        vTaskDelay(pdMS_TO_TICKS(WAIT_TIME_BEFORE_SEND_REQUEST_MS));
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

    if (ps_taskRegisters->u_status.bitFields.powerOff) {
#if defined(ENABLE_VERBOSE_STM_FOTA_TASK)
        ESP_LOGW(TAG, "Dispositivo inativo...");
#endif
        vTaskDelay(pdMS_TO_TICKS(30000));
        return false;
    }

    return true;
}
/******************************************************************************
 * @fn          SetDefaultRegisters()
 *
 * @brief       Define os valores padrões para a estrutura utilizada pela
 *tarefa.
 ******************************************************************************/
static void SetDefaultRegisters(void) {
    memset((void *)ps_taskRegisters, 0, sizeof(S_ControlRegistersTypeDef));
    uint32_t standardTimeout = WAIT_TIME_AFTER_SEND_REQUEST_MS;

    memcpy((void *)&ps_taskRegisters->timer.timeout_ms,
           &standardTimeout,
           sizeof(ps_taskRegisters->timer.timeout_ms));
}
/******************************************************************************
 * @fn          RequestToGetVersionRunning()
 *
 * @brief       Requisição responsável por solicitar a versão atualmente em
 * execução no STM.
 * @return      Booleano - Status de retorno.
 *              @retval False - Requisição não enviada.
 *              @retval True - Requisição enviada com sucesso.
 ******************************************************************************/
static bool RequestToGetVersionRunning(void) {
    if (__TAKE_SEMAPHORE(xSemaphore)) {
        memset((void *)ps_taskRegisters->imageRunning.versionBuffer,
               0,
               sizeof(ps_taskRegisters->imageRunning.versionBuffer));

        ps_taskRegisters->flags.getVersionRunning = false;

        if (MainUc_SRV_AppWriteEvtCB(STM_FOTA_PROFILE_APP,
                                     STM_FOTA_GET_VERSION_RUNNING_CHAR,
                                     0,
                                     NULL)
            != APP_OK) {
            GIVE_SEMAPHORE(xSemaphore);
            return false;
        }

        ps_taskRegisters->flags.getVersionRunning = true;

        GIVE_SEMAPHORE(xSemaphore);
    }
    return true;
}
/******************************************************************************
 * @fn          RequestConfirmedStatus()
 *
 * @brief       Requisição o status de confirmação do download.
 * @return      Booleano - Status de retorno.
 *              @retval False - Requisição não enviada.
 *              @retval True - Requisição enviada com sucesso.
 ******************************************************************************/
static bool RequestConfirmedStatus(void) {
    if (!__TAKE_SEMAPHORE(xSemaphore)) {
        return false;
    }

    ps_taskRegisters->flags.getConfirmedStatus = false;

    if (MainUc_SRV_AppWriteEvtCB(
            STM_FOTA_PROFILE_APP,
            STM_FOTA_CONFIRMED_STATUS_CHAR,
            sizeof(ps_taskRegisters->imageAvailable.versionBuffer),
            (uint8_t *)ps_taskRegisters->imageAvailable.versionBuffer)
        != APP_OK) {
        GIVE_SEMAPHORE(xSemaphore);
        return false;
    }

    ps_taskRegisters->flags.getConfirmedStatus = true;
    GIVE_SEMAPHORE(xSemaphore);

    return true;
}
/******************************************************************************
 * @fn          RequestToGetImageInfo()
 *
 * @brief       Requisição responsável por solicitar a recuperação/obtenção do
 * progresso da imagem.
 * @return      Booleano - Status de retorno.
 *              @retval False - Requisição não enviada.
 *              @retval True - Requisição enviada com sucesso.
 ******************************************************************************/
static bool RequestToGetImageInfo(void) {
    if (!__TAKE_SEMAPHORE(xSemaphore)) {
        return false;
    }

    ps_taskRegisters->imageDownload.framePos = 0;
    ps_taskRegisters->imageDownload.blockPos = 0;

    ps_taskRegisters->flags.getRecoveryInfoImage = false;

    if (MainUc_SRV_AppWriteEvtCB(
            STM_FOTA_PROFILE_APP,
            STM_FOTA_RECOVER_IMAGE_INFO_CHAR,
            sizeof(ps_taskRegisters->imageAvailable.versionBuffer),
            (uint8_t *)ps_taskRegisters->imageAvailable.versionBuffer)
        != APP_OK) {
        GIVE_SEMAPHORE(xSemaphore);
        return false;
    }

    ps_taskRegisters->flags.getRecoveryInfoImage = true;
    GIVE_SEMAPHORE(xSemaphore);

    return true;
}
/******************************************************************************
 * @fn          RequestToWriteFrameDataImage()
 *
 * @brief       Requisição responsável por enviar uma parte da imagem para
 * ser escrita/armazenada pelo STM.
 * @return      Booleano - Status de retorno.
 *              @retval False - Requisição não enviada.
 *              @retval True - Requisição enviada com sucesso.
 ******************************************************************************/
static bool RequestToWriteFrameDataImage(void) {
    if (!__TAKE_SEMAPHORE(xSemaphore)) {
        return false;
    }

    uint8_t *const p_frameData =
        (uint8_t *)&ps_taskRegisters->imageDownload
            .frameDownload[ps_taskRegisters->flash.offsetWrite];

    uint8_t frameLen = ps_taskRegisters->flash.writeBytes;

    uint16_t pos = 0;

    uint8_t payload[UTILS_WE_PROTOCOL_MAX_PAYLOAD_SIZE] = {
        [0 ...(GET_SIZE_ARRAY(payload) - 1)] = 0};

    memcpy((void *)&payload[pos],
           (void *)&ps_taskRegisters->imageDownload.blockPos,
           sizeof(ps_taskRegisters->imageDownload.blockPos));
    pos += sizeof(ps_taskRegisters->imageDownload.blockPos);

    memcpy((void *)&payload[pos],
           (void *)&ps_taskRegisters->imageDownload.framePos,
           sizeof(ps_taskRegisters->imageDownload.framePos));
    pos += sizeof(ps_taskRegisters->imageDownload.framePos);

    memcpy((void *)&payload[pos], (void *)p_frameData, frameLen);
    pos += frameLen;

    ps_taskRegisters->flags.getFrameDataStatus = false;

    if (MainUc_SRV_AppWriteEvtCB(STM_FOTA_PROFILE_APP,
                                 STM_FOTA_WRITE_FRAME_DATA_CHAR,
                                 (uint8_t)pos,
                                 payload)
        != APP_OK) {
        GIVE_SEMAPHORE(xSemaphore);
        return false;
    }

    ps_taskRegisters->flags.getFrameDataStatus = true;
    GIVE_SEMAPHORE(xSemaphore);

    return true;
}
/******************************************************************************
 * @fn          RequestDownloadImageCompleted()
 *
 * @brief       Requisição responsável por notificar o STM que a imagem foi
 * baixada por completo.
 * @return      Booleano - Status de retorno.
 *              @retval False - Requisição não enviada.
 *              @retval True - Requisição enviada com sucesso.
 ******************************************************************************/
static bool RequestDownloadImageCompleted(void) {
    IF_TRUE_RETURN(MainUc_SRV_AppWriteEvtCB(STM_FOTA_PROFILE_APP,
                                            STM_FOTA_FINISH_IMAGE_CHAR,
                                            0,
                                            NULL)
                       != APP_OK,
                   false);

    return true;
}
/******************************************************************************
 * @fn          RequestDownloadIconStatus()
 *
 * @brief       Requisição responsável por  enviar o status do download,
 * definindo o tipo de ícone a ser exibido.
 * @return      Booleano - Status de retorno.
 *              @retval False - Requisição não enviada.
 *              @retval True - Requisição enviada com sucesso.
 ******************************************************************************/
static bool RequestDownloadIconStatus(
    E_FwStatusDownloadTypeDef fwStatusDownload) {
    uint8_t iconStatus = (uint8_t)fwStatusDownload;
    IF_TRUE_RETURN(MainUc_SRV_AppWriteEvtCB(STM_FOTA_PROFILE_APP,
                                            STM_FOTA_ICON_STATUS_CHAR,
                                            sizeof(iconStatus),
                                            &iconStatus)
                       != APP_OK,
                   false);

    return true;
}
/******************************************************************************
 * @fn          GetStatusAll()
 *
 * @brief       Obtém todos os status relacionado à estrutura de controle
 * de arquivo, com seus respectivos bits de campo.
 * @param       p_payload Vetor com os dados.
 ******************************************************************************/
static uint16_t GetStatusAll(uint8_t *const p_payload) {
    uint16_t i = 0;

    ps_taskRegisters->u_status.all = p_payload[i++];
    return i;
}
/******************************************************************************
 * @fn          GetResponseVersionRunning()
 *
 * @brief       Processa a resposta acerca da versão em execução no STM.
 * @param       p_payload Vetor contendo os dados.
 * @param       len Tamanho do vetor, em bytes.
 ******************************************************************************/
static void GetResponseVersionRunning(const uint8_t *const p_payload,
                                      const uint8_t len) {
    IF_TRUE_RETURN(!ps_taskRegisters->flags.getVersionRunning || !len);
    IF_TRUE_RETURN(!__TAKE_SEMAPHORE(xSemaphore));

    ps_taskRegisters->flags.getVersionRunning = false;

    uint16_t pos = GetStatusAll(p_payload);

    if (len > pos) {
        memcpy((void *)ps_taskRegisters->imageRunning.versionBuffer,
               (void *)&p_payload[pos],
               sizeof(ps_taskRegisters->imageRunning.versionBuffer));
    }

    ps_taskRegisters->transaction.answered = true;

    GIVE_SEMAPHORE(xSemaphore);
    Task_STMFota_Resume();
}
/******************************************************************************
 * @fn          GetResponseConfirmedStatus()
 *
 * @brief       Processa a resposta acerca do status de confirmação do download
 * da nova versão disponível.
 * @param       p_payload Vetor contendo os dados.
 * @param       len Tamanho do vetor, em bytes.
 ******************************************************************************/
static void GetResponseConfirmedStatus(const uint8_t *const p_payload,
                                       const uint8_t len) {
    IF_TRUE_RETURN(!ps_taskRegisters->flags.getConfirmedStatus || !len);
    IF_TRUE_RETURN(!__TAKE_SEMAPHORE(xSemaphore));

    ps_taskRegisters->flags.getConfirmedStatus = false;

    if (len) {
        ps_taskRegisters->imageDownload.confirmStatus = p_payload[0];
    }

    ps_taskRegisters->transaction.answered = true;

    GIVE_SEMAPHORE(xSemaphore);
    Task_STMFota_Resume();
}
/******************************************************************************
 * @fn          GetResponseRecoveryImageInfo()
 *
 * @brief       Processa a resposta acerca do progresso da imagem baixada.
 * @param       p_payload Vetor contendo os dados.
 * @param       len Tamanho do vetor, em bytes.
 ******************************************************************************/
static void GetResponseRecoveryImageInfo(const uint8_t *const p_payload,
                                         const uint16_t len) {
    IF_TRUE_RETURN(!ps_taskRegisters->flags.getRecoveryInfoImage);

    if (!__TAKE_SEMAPHORE(xSemaphore)) {
        return;
    }

    ps_taskRegisters->flags.getRecoveryInfoImage = false;

    uint16_t pos = GetStatusAll(p_payload);

    if (len > pos) {
        memcpy((void *)&ps_taskRegisters->imageDownload.blockPos,
               (void *)&p_payload[pos],
               sizeof(ps_taskRegisters->imageDownload.blockPos));
        pos += sizeof(ps_taskRegisters->imageDownload.blockPos);
    }

    ps_taskRegisters->transaction.answered = true;

    GIVE_SEMAPHORE(xSemaphore);
    Task_STMFota_Resume();
}
/******************************************************************************
 * @fn          GetResponseWriteFrameStatus()
 *
 * @brief       Processa a resposta acerca da parte da imagem escrita/armazenada
 * no STM.
 * @param       p_payload Vetor contendo os dados.
 * @param       len Tamanho do vetor, em bytes.
 ******************************************************************************/
static void GetResponseWriteFrameStatus(const uint8_t *const p_payload,
                                        const uint8_t len) {
    IF_TRUE_RETURN(!ps_taskRegisters->flags.getFrameDataStatus || !len);

    if (__TAKE_SEMAPHORE(xSemaphore)) {
        ps_taskRegisters->flags.getFrameDataStatus = false;

        uint16_t pos = GetStatusAll(p_payload);

        if (len > pos) {
            ps_taskRegisters->response.blockPos = 0;
            ps_taskRegisters->response.framePos = 0;

            memcpy((void *)&ps_taskRegisters->response.blockPos,
                   (void *)&p_payload[pos],
                   sizeof(ps_taskRegisters->response.blockPos));
            pos += sizeof(ps_taskRegisters->response.blockPos);

            memcpy((void *)&ps_taskRegisters->response.framePos,
                   (void *)&p_payload[pos],
                   sizeof(ps_taskRegisters->response.framePos));
            pos += sizeof(ps_taskRegisters->response.framePos);
        }

        ps_taskRegisters->transaction.answered = true;

        GIVE_SEMAPHORE(xSemaphore);
        Task_STMFota_Resume();
    }
}
/******************************************************************************
 * @fn          GetOffsetImage()
 *
 * @brief       Retorna o deslocamento da imagem, segundo o número do bloco,
 * apenas.
 * @return      uint32_t - Valor de retorno.
 ******************************************************************************/
static uint32_t GetOffsetImage(void) {
    return ps_taskRegisters->imageDownload.blockPos * BLOCK_DATA_LENGTH;
}
/******************************************************************************
 * @fn          GetProgressImagePercentage()
 *
 * @brief       Retorna o progresso da imagem, em porcentagem.
 * @return      float - Valor de retorno.
 ******************************************************************************/
static float GetProgressImagePercentage(void) {
    float progressImage = (float)GetOffsetImage() / APP_TOTAL_IMAGE_LENGTH;
    progressImage *= 100.0f;
    return progressImage;
}
/******************************************************************************
 * @fn          ComputeNextCheckProgressImage()
 *
 * @brief       Calcula a próxima vez em que a versão da imagem em nuvem
 * deverá ser consultada.
 ******************************************************************************/
static void ComputeNextCheckProgressImage(void) {
    float progress = GetProgressImagePercentage();

    uint8_t nextProgress = ((uint8_t)progress / 10 + 1) * 10;

    ps_taskRegisters->imageDownload.nextCheckOffset =
        (uint32_t)((float)nextProgress * (float)APP_TOTAL_IMAGE_LENGTH
                   / 100.0f);
}
/******************************************************************************
 * @fn          IsTimeToCheckIfHasVersionChanged()
 *
 * @brief       Verifica se é o momento de iniciar a verificação da versão
 * da imagem disponível em nuvem.
 * @return      Booleano - Status de retorno.
 *              @retval False - Não é o momento de verificar.
 *              @retval True - É o momento de verificar.
 ******************************************************************************/
static const bool IsTimeToCheckIfHasVersionChanged(void) {
    return GetOffsetImage() >= ps_taskRegisters->imageDownload.nextCheckOffset;
}
/*****************************END OF FILE**************************************/
