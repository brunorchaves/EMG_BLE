/* Includes ------------------------------------------------------------------*/

#include "Tasks/FileUploadTask.h"

/* Private define ------------------------------------------------------------*/

/**
 * @brief Apelido utilizado para identificar as saídas de logs do ESP.
 */
#define TAG __FILE__

/**
 * @brief Tamanho total do vetor de quadro, em bytes.
 */
#define FRAME_DATA_LENGTH 128U

/**
 * @brief Tamanho total do vetor de bloco, em bytes.
 */
#define BLOCK_DATA_LENGTH (3 * 4096U)

#if BLOCK_DATA_LENGTH % FRAME_DATA_LENGTH != 0
    #error '(FileUploadTask.c) - Please, set the MACRO"s value "FRAME_DATA_LENGTH" to be \
multiple of MACRO"s value "BLOCK_DATA_LENGTH"'.
#endif

/**
 * @brief Quantidade de frames em um bloco.
 */
#define FRAMES_BY_BLOCK_COUNT (BLOCK_DATA_LENGTH / FRAME_DATA_LENGTH)

/**
 * @brief Tamanho máximo do nome do arquivo.
 */
#define FILE_NAME_MAX_LENGTH 18U

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
 * @brief Tempo limite de espera após falhar no envio do upload.
 */
#define WAIT_TIME_AFTER_FAIL_UPLOAD_MS 1000U

/**
 * @brief Tempo limite para verificar se a quantidade de arquivos
 * armazenados no cartão de memória.
 */
#define RECHECK_COUNT_FILES_TIMEOUT_MS 30000U

/**
 * @brief Tempo limite para verificar se há mais dados disponíveis
 * em relação ao arquivo do dia, quando já tiver chegado ao final do arquivo
 * e estiver também esperando novos dados.
 */
#define REQUEST_FRAME_DATA_TODAY_FILE_TIMEOUT_MS 30000U

/**
 * @brief Tempo limite de atraso, caso o cartão de memória esteja inativo.
 */
#define SD_CARD_INACTIVE_TIMEOUT_MS 60000U

/**
 * @brief Tempo limite acordado com plataforma para enviar o próximo arquivo.
 */
#define WAIT_TIME_AFTER_UPLOAD_BLOCK_FILE_MS (2UL * 60 * 1000)

/**
 * @brief Obtém o semáforo.
 */
#define __TAKE_SEMAPHORE(semaphore) (TAKE_SEMAPHORE(semaphore, portMAX_DELAY))

/* Private typedef -----------------------------------------------------------*/

typedef bool (*RequestFunctionTypeDef)(void);

typedef enum E_TaskFileUploadMachineState {
    E_HAS_DATA_RECOVERY = 0,
    E_SET_FILE_RECOVERY_INFO,
    E_RECOVER_FILE_INFO,
    E_CONFIRM_BLOCK_POS_RECEIVED,
    E_GET_FILES_COUNT,
    E_READ_FRAME_DATA,
    E_UPLOAD_BLOCK_DATA,
    E_SAVE_PROGRESS,
    E_MOVE_UPLOAD_FILE,
} E_TaskFileUploadMachineStateTypeDef;

typedef struct S_ControlRegisters {
    volatile E_TaskFileUploadMachineStateTypeDef machineState;

    struct S_Upload {
        uint32_t uploadTimer;
        volatile uint16_t filesCount;
        volatile uint16_t blockPos;
        volatile uint8_t framePos;
        volatile uint16_t frameLen;
        uint8_t frameData[FRAME_DATA_LENGTH];
        volatile uint16_t blockLen;
        uint8_t blockData[BLOCK_DATA_LENGTH];
        bool buildFileName;
        char uploadFileName[UINT8_MAX / 4];
        volatile char fileName[FILE_NAME_MAX_LENGTH];

        union U_FileControl {
            volatile uint8_t all;
            struct S_BitControl {
                volatile uint8_t isTodayFile  : 1;
                volatile uint8_t endOfFile    : 1;
                volatile uint8_t sdCardStatus : 1;
            } bitFields;
        } volatile fileControl;
    } volatile upload;

    struct {
        volatile bool answered;
        volatile bool requested;
    } volatile transaction;

    struct {
        volatile bool hasDataToRecovery;
        volatile bool moveFileStatus;
        volatile bool blockPosStatus;
        volatile bool blockPosNotEqual;
    } volatile response;

    struct {
        volatile bool hasDataRecovery;
        volatile bool setFileRecoveryInfo;
        volatile bool getRecoverFileInfo;
        volatile bool getConfirmStatusBlockPos;
        volatile bool getFilesCount;
        volatile bool getFrameData;
        volatile bool getConfirmSaveProgress;
        volatile bool moveUploadFile;
    } volatile flags;

} S_ControlRegistersTypeDef;

/* Private variables ---------------------------------------------------------*/

static TaskHandle_t xHandle         = NULL;
static SemaphoreHandle_t xSemaphore = NULL;

static volatile S_ControlRegistersTypeDef *const volatile ps_taskRegisters =
    NULL;

/* Private function prototypes -----------------------------------------------*/

static void Task_FileUpload(void *p_parameters);
static uint32_t GetStackDepth(void);

static bool UpdateMachineState(void);
static bool TransactionHandler(RequestFunctionTypeDef requestFunction);

static bool RequestToKnowHasDataRecovery(void);
static bool RequestToSetFileRecoveryInfo(void);
static bool RequestToGetFilesCount(void);
static bool RequestToRecoveryFileInfo(void);
static bool RequestToConfirmStatusBlockPosRecovered(void);
static bool RequestReadFrameData(void);
static bool UploadBlockFile(void);
static bool RequestToSaveProgress(void);
static bool RequestToMoveFile(void);

static void CopyFrameDataToBlockData(void);

static uint16_t GetStatusAll(uint8_t *const p_payload);

static void ResponseToGetHasDataRecovery(uint8_t *const p_payload, uint8_t len);
static void ResponseToSettedFileRecoveryInfo(uint8_t *const p_payload,
                                             uint8_t len);

static void ResponseToGetCountFiles(uint8_t *const p_payload, uint8_t len);
static void ResponseToRecoveryInfo(uint8_t *const p_payload, uint8_t len);
static void ResponseToConfirmStatusBlockPosRecovered(uint8_t *const p_payload,
                                                     uint8_t len);
static void ResponseGetFrameData(uint8_t *const p_payload, uint8_t len);
static void ResponseToSaveProgress(uint8_t *const p_payload, uint8_t len);
static void ResponseToMovedFileStatus(uint8_t *const p_payload, uint8_t len);

static void ShowTotalSizeFile(void);
static bool CheckSdCardStatus(void);

/* Public functions ----------------------------------------------------------*/

/******************************************************************************
 * @fn          Task_FileUpload_Create()
 *
 * @brief       Cria a tarefa.
 * @param       priority Prioridade para a tarefa.
 * @param       p_parameters Parâmetro genérico (Opcional).
 ******************************************************************************/
void Task_FileUpload_Create(const uint8_t priority, void *p_parameters) {
    IF_TRUE_RETURN(xHandle);

    xTaskCreate(Task_FileUpload,  // Funcao
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
 * @fn          Task_FileUpload_Destroy()
 *
 * @brief       Encerra a tarefa.
 ******************************************************************************/
void Task_FileUpload_Destroy(void) {
    IF_TRUE_RETURN(!xHandle);

#if defined(ENABLE_VERBOSE_FILE_UPLOAD_TASK)
    ESP_LOGI(__FILE__, "Deletando Tarefa...");
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
 * @fn          Task_FileUpload_Resume()
 *
 * @brief       Resume a tarefa.
 ******************************************************************************/
void Task_FileUpload_Resume(void) {
    IF_TRUE_RETURN(!xHandle);

    eTaskState taskState = eTaskGetState(xHandle);

    if (taskState == eSuspended) {
        vTaskResume(xHandle);
    } else if (taskState == eBlocked) {
        xTaskAbortDelay(xHandle);
    }
}
/******************************************************************************
 * @fn          Task_FileUpload_Suspend()
 *
 * @brief       Suspende a tarefa.
 ******************************************************************************/
void Task_FileUpload_Suspend(void) {
    IF_TRUE_RETURN(!xHandle);

    eTaskState taskState = eTaskGetState(xHandle);

    if (taskState != eSuspended) {
        vTaskSuspend(xHandle);
    }
}
/******************************************************************************
 * @fn          MainUc_FileUploadProfile_ResponseCallback()
 *
 * @brief       Callback chamado ao receber alguma resposta do STM.
 * @param       charId - Identificador do subcomando.
 * @param       charSize - Tamanho do payload, em bytes.
 * @param       parameterValue - Ponteiro para payload.
 ******************************************************************************/
void MainUc_FileUploadProfile_ResponseCallback(uint8_t charId,
                                               uint8_t charSize,
                                               uint8_t *const parameterValue) {
    IF_TRUE_RETURN(!ps_taskRegisters);
    IF_TRUE_RETURN(!ps_taskRegisters->transaction.requested);

    switch (charId) {
        case FILE_UPLOAD_HAS_DATA_RECOVERY_CHAR:
            {
                ResponseToGetHasDataRecovery(parameterValue, charSize);
                break;
            }

        case FILE_UPLOAD_SET_FILE_RECOVERY_INFO_CHAR:
            {
                ResponseToSettedFileRecoveryInfo(parameterValue, charSize);
                break;
            }

        case FILE_UPLOAD_RECOVER_FILE_INFO_CHAR:
            {
                ResponseToRecoveryInfo(parameterValue, charSize);
                break;
            }

        case FILE_UPLOAD_CONFIRM_BLOCK_POS_RECOVERED_CHAR:
            {
                ResponseToConfirmStatusBlockPosRecovered(parameterValue,
                                                         charSize);
                break;
            }

        case FILE_UPLOAD_GET_FILES_COUNT_CHAR:
            {
                ResponseToGetCountFiles(parameterValue, charSize);
                break;
            }

        case FILE_UPLOAD_READ_FRAME_DATA_CHAR:
            {
                ResponseGetFrameData(parameterValue, charSize);
                break;
            }

        case FILE_UPLOAD_SAVE_PROGRESS_CHAR:
            {
                ResponseToSaveProgress(parameterValue, charSize);
                break;
            }

        case FILE_UPLOAD_MOVE_UPLOAD_FILE_CHAR:
            {
                ResponseToMovedFileStatus(parameterValue, charSize);
                break;
            }

        default:
            break;
    }
}

/* Private functions ---------------------------------------------------------*/

/******************************************************************************
 * @fn          Task_FileUpload()
 *
 * @brief       Gerencia o processo de dados e de envio dos logs.
 * @param       p_parameters Parâmetro genérico (Opcional).
 ******************************************************************************/
static void Task_FileUpload(void *p_parameters) {
    vTaskDelay(pdMS_TO_TICKS(WAIT_TIME_TO_REQUEST_USART_MS));

#if defined(ENABLE_VERBOSE_FILE_UPLOAD_TASK)
    ESP_LOGI(TAG, "Iniciando...");
#endif

    memset((void *)ps_taskRegisters, 0, sizeof(*ps_taskRegisters));

    ps_taskRegisters->machineState = E_HAS_DATA_RECOVERY;

    while (true) {
        /**
         * Se o upload tiver perdendo o valor de blockPos após o bendito timeout
         * de terapia, então é só aumentar o tamanho da pilha da tarefa do
         * upload
         */
        if (UpdateMachineState()) {
            break;
        }
    }

    Task_FileUpload_Destroy();
}
/******************************************************************************
 * @fn          GetStackDepth()
 *
 * @brief       Informa o tamanho da pilha exigida para a tarefa.
 ******************************************************************************/
static uint32_t GetStackDepth(void) { return 4096UL; }
/******************************************************************************
 * @fn          UpdateMachineState()
 *
 * @brief       Atualiza a máquina de estado da tarefa principal.
 * @return      Booleano - Status de retorno.
 *              @retval False - Não faz nada.
 *              @retval True - Deletar tarefa.
 ******************************************************************************/
static bool UpdateMachineState(void) {
    switch (ps_taskRegisters->machineState) {
        case E_HAS_DATA_RECOVERY:
            {
#if defined(ENABLE_VERBOSE_FILE_UPLOAD_TASK)
                ESP_LOGI(__FILE__,
                         "Verificando se há dados para recuperação...");
#endif

                if (!TransactionHandler(RequestToKnowHasDataRecovery)) {
                    break;
                }

                if (!ps_taskRegisters->response.hasDataToRecovery) {
#if defined(ENABLE_VERBOSE_FILE_UPLOAD_TASK)
                    ESP_LOGI(__FILE__, "Não há dados!");
#endif

                    ps_taskRegisters->machineState = E_GET_FILES_COUNT;
                } else {
                    ps_taskRegisters->machineState = E_RECOVER_FILE_INFO;
                }
                break;
            }

        case E_GET_FILES_COUNT:
            {
#if defined(ENABLE_VERBOSE_FILE_UPLOAD_TASK)
                ESP_LOGI(__FILE__, "Obtendo quantidade de arquivos...");
#endif

                if (!TransactionHandler(RequestToGetFilesCount)) {
                    break;
                }

#if defined(ENABLE_VERBOSE_FILE_UPLOAD_TASK)
                ESP_LOGI(TAG,
                         "Quantidade de arquivos: %d",
                         ps_taskRegisters->upload.filesCount);
#endif

                if (!ps_taskRegisters->upload.filesCount) {
                    vTaskDelay(pdMS_TO_TICKS(RECHECK_COUNT_FILES_TIMEOUT_MS));
                    break;
                }
                ps_taskRegisters->machineState = E_SET_FILE_RECOVERY_INFO;
                break;
            }

        case E_SET_FILE_RECOVERY_INFO:
            {
#if defined(ENABLE_VERBOSE_FILE_UPLOAD_TASK)
                ESP_LOGI(__FILE__,
                         "Solicitando novas definições de arquivo...");
#endif

                if (!TransactionHandler(RequestToSetFileRecoveryInfo)) {
                    break;
                }

#if defined(ENABLE_VERBOSE_FILE_UPLOAD_TASK)
                ESP_LOGI(TAG,
                         "Novo arquivo definido: %s.",
                         ps_taskRegisters->upload.fileName);
#endif
                ps_taskRegisters->machineState = E_READ_FRAME_DATA;
                break;
            }

        case E_RECOVER_FILE_INFO:
            {
#if defined(ENABLE_VERBOSE_FILE_UPLOAD_TASK)
                ESP_LOGI(__FILE__, "Recuperando definições de upload...");
#endif

                if (!TransactionHandler(RequestToRecoveryFileInfo)) {
                    break;
                }

#if defined(ENABLE_VERBOSE_FILE_UPLOAD_TASK)
                ESP_LOGI(TAG,
                         "Nome do arquivo: %s, BlockPos: %d, FramePos: %d.",
                         ps_taskRegisters->upload.fileName,
                         ps_taskRegisters->upload.blockPos,
                         ps_taskRegisters->upload.framePos);
#endif

                ps_taskRegisters->machineState = E_CONFIRM_BLOCK_POS_RECEIVED;
                break;
            }

        case E_CONFIRM_BLOCK_POS_RECEIVED:
            {
#if defined(ENABLE_VERBOSE_FILE_UPLOAD_TASK)
                ESP_LOGI(__FILE__, "Confirmando número do bloco recuperado...");
#endif

                if (!TransactionHandler(
                        RequestToConfirmStatusBlockPosRecovered)) {
                    break;
                }

                if (!ps_taskRegisters->response.blockPosStatus) {
                    ps_taskRegisters->machineState = E_RECOVER_FILE_INFO;
                    break;
                }

                if (ps_taskRegisters->upload.fileControl.bitFields.endOfFile) {
                    ps_taskRegisters->machineState = E_MOVE_UPLOAD_FILE;
                } else {
                    ps_taskRegisters->machineState = E_READ_FRAME_DATA;
                }
                break;
            }

        case E_READ_FRAME_DATA:
            {
                if (!TransactionHandler(RequestReadFrameData)) {
                    break;
                }

#if defined(ENABLE_VERBOSE_FILE_UPLOAD_TASK)
                ESP_LOGI(TAG,
                         "Dados recebidos: %d bytes. BlockPos: %d, "
                         "FramePos: %d, "
                         "Day file: "
                         "%d, EOF: %d",
                         (int)ps_taskRegisters->upload.frameLen,
                         (int)ps_taskRegisters->upload.blockPos,
                         (int)ps_taskRegisters->upload.framePos,
                         (int)ps_taskRegisters->upload.fileControl.bitFields
                             .isTodayFile,
                         (int)ps_taskRegisters->upload.fileControl.bitFields
                             .endOfFile);
#endif

                if (ps_taskRegisters->upload.fileControl.bitFields
                        .isTodayFile) {
                    if (ps_taskRegisters->upload.frameLen
                        != FRAME_DATA_LENGTH) {
#if defined(ENABLE_VERBOSE_FILE_UPLOAD_TASK)
                        ESP_LOGI(TAG,
                                 "Day file - Aguardando completar os %d bytes "
                                 "(Pendentes: %d "
                                 "bytes)",
                                 FRAME_DATA_LENGTH,
                                 FRAME_DATA_LENGTH
                                     - ps_taskRegisters->upload.frameLen);
#endif

                        vTaskDelay(pdMS_TO_TICKS(
                            REQUEST_FRAME_DATA_TODAY_FILE_TIMEOUT_MS));
                        break;
                    }
                }

                if (ps_taskRegisters->upload.frameLen == FRAME_DATA_LENGTH
                    || ps_taskRegisters->upload.fileControl.bitFields
                           .endOfFile) {
                    CopyFrameDataToBlockData();
                    ps_taskRegisters->upload.framePos += 1;
                }

                if (ps_taskRegisters->upload.framePos
                    == FRAMES_BY_BLOCK_COUNT) {
                    if (ps_taskRegisters->upload.fileControl.bitFields
                            .endOfFile) {
                        ShowTotalSizeFile();
                    }

                    ps_taskRegisters->machineState = E_UPLOAD_BLOCK_DATA;
                } else if (!ps_taskRegisters->upload.fileControl.bitFields
                                .isTodayFile
                           && ps_taskRegisters->upload.fileControl.bitFields
                                  .endOfFile) {
                    if (ps_taskRegisters->upload.fileControl.bitFields
                            .endOfFile) {
                        ShowTotalSizeFile();
                    }
                    ps_taskRegisters->machineState = E_UPLOAD_BLOCK_DATA;
                }

                break;
            }

        case E_UPLOAD_BLOCK_DATA:
            {
                if (ps_taskRegisters->upload.uploadTimer
                    && !UTILS_Timer_WaitTimeMSec(
                        ps_taskRegisters->upload.uploadTimer,
                        WAIT_TIME_AFTER_UPLOAD_BLOCK_FILE_MS)) {
                    uint32_t elapsedTime_MS =
                        (UTILS_Timer_Now()
                         - ps_taskRegisters->upload.uploadTimer);
                    uint32_t remainTime_MS =
                        WAIT_TIME_AFTER_UPLOAD_BLOCK_FILE_MS - elapsedTime_MS;
#if defined(ENABLE_VERBOSE_FILE_UPLOAD_TASK)
                    ESP_LOGW(__FILE__,
                             "Tempo restante para upload: %lu ms...",
                             remainTime_MS);
#endif
                    vTaskDelay(pdMS_TO_TICKS(remainTime_MS));
                    break;
                }

                if (UploadBlockFile()) {
                    ps_taskRegisters->upload.uploadTimer = UTILS_Timer_Now();

                    if (__TAKE_SEMAPHORE(xSemaphore)) {
                        ps_taskRegisters->upload.blockPos += 1;
                        ps_taskRegisters->upload.framePos = 0;

                        memset((void *)ps_taskRegisters->upload.blockData,
                               0,
                               sizeof(ps_taskRegisters->upload.blockData));
                        ps_taskRegisters->upload.blockLen = 0;

                        GIVE_SEMAPHORE(xSemaphore);
                    }

                    ps_taskRegisters->machineState = E_SAVE_PROGRESS;
                }
                break;
            }

        case E_SAVE_PROGRESS:
            {
#if defined(ENABLE_VERBOSE_FILE_UPLOAD_TASK)
                ESP_LOGI(__FILE__, "Salvando progresso...");
#endif

                if (!TransactionHandler(RequestToSaveProgress)) {
                    break;
                }

                if (ps_taskRegisters->response.blockPosNotEqual) {
                    ps_taskRegisters->response.blockPosNotEqual = false;
                    ps_taskRegisters->machineState = E_RECOVER_FILE_INFO;
                }

                if (ps_taskRegisters->upload.fileControl.bitFields
                        .isTodayFile) {
                    ps_taskRegisters->machineState = E_READ_FRAME_DATA;
                } else if (!ps_taskRegisters->upload.fileControl.bitFields
                                .endOfFile) {
                    ps_taskRegisters->machineState = E_READ_FRAME_DATA;
                } else {
                    ps_taskRegisters->machineState = E_MOVE_UPLOAD_FILE;
                }
                break;
            }

        case E_MOVE_UPLOAD_FILE:
            {
#if defined(ENABLE_VERBOSE_FILE_UPLOAD_TASK)
                ESP_LOGI(__FILE__, "Movendo arquivo...");
#endif

                if (!TransactionHandler(RequestToMoveFile)) {
                    break;
                }

                if (ps_taskRegisters->response.moveFileStatus) {
                    ps_taskRegisters->machineState = E_GET_FILES_COUNT;
                }

                break;
            }
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
 *              @retval False - Requisição não enviada ou resposta não recebida.
 *              @retval True - Transação executada com sucesso.
 ******************************************************************************/
static bool TransactionHandler(RequestFunctionTypeDef requestFunction) {
    IF_TRUE_RETURN(!requestFunction, false);
    if (!ps_taskRegisters->transaction.requested) {
        vTaskDelay(pdMS_TO_TICKS(WAIT_TIME_TO_REQUEST_USART_MS));

        if (!requestFunction()) {
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

    if (!CheckSdCardStatus()) {
#if defined(ENABLE_VERBOSE_FILE_UPLOAD_TASK)
        ESP_LOGW(TAG, "Cartão de memória inativo...");
#endif

        vTaskDelay(pdMS_TO_TICKS(SD_CARD_INACTIVE_TIMEOUT_MS));
        return false;
    }

    return true;
}
/******************************************************************************
 * @fn          RequestToKnowHasDataRecovery()
 *
 * @brief       Requisita ao STM se o mesmo possui dados para recuperação.
 * @return      Booleano - Status de retorno.
 *              @retval False - Requisição não enviada.
 *              @retval True - Requisição enviada com sucesso.
 ******************************************************************************/
static bool RequestToKnowHasDataRecovery(void) {
    if (__TAKE_SEMAPHORE(xSemaphore)) {
        ps_taskRegisters->response.hasDataToRecovery = false;
        ps_taskRegisters->flags.hasDataRecovery      = false;

        if (MainUc_SRV_AppWriteEvtCB(FILE_UPLOAD_PROFILE_APP,
                                     FILE_UPLOAD_HAS_DATA_RECOVERY_CHAR,
                                     0,
                                     NULL)
            != APP_OK) {
            GIVE_SEMAPHORE(xSemaphore);
            return false;
        }

        ps_taskRegisters->flags.hasDataRecovery = true;
        GIVE_SEMAPHORE(xSemaphore);
    }

    return true;
}
/******************************************************************************
 * @fn          RequestToSetFileRecoveryInfo()
 *
 * @brief       Requisita ao STM para definir o novo arquivo e suas informações
 * de definição para o fluxo de trabalho de upload desse arquivo.
 * @return      Booleano - Status de retorno.
 *              @retval False - Requisição não enviada.
 *              @retval True - Requisição enviada com sucesso.
 ******************************************************************************/
static bool RequestToSetFileRecoveryInfo(void) {
    if (__TAKE_SEMAPHORE(xSemaphore)) {
        memset((void *)&ps_taskRegisters->upload,
               0,
               sizeof(ps_taskRegisters->upload));
        ps_taskRegisters->flags.setFileRecoveryInfo = false;

        if (MainUc_SRV_AppWriteEvtCB(FILE_UPLOAD_PROFILE_APP,
                                     FILE_UPLOAD_SET_FILE_RECOVERY_INFO_CHAR,
                                     0,
                                     NULL)
            != APP_OK) {
            GIVE_SEMAPHORE(xSemaphore);
            return false;
        }

        ps_taskRegisters->flags.setFileRecoveryInfo = true;
        GIVE_SEMAPHORE(xSemaphore);
    }

    return true;
}
/******************************************************************************
 * @fn          RequestToGetFilesCount()
 *
 * @brief       Requisita a quantidade de arquivos existentes no cartão de
 * memória.
 * @return      Booleano - Status de retorno.
 *              @retval False - Requisição não enviada.
 *              @retval True - Requisição enviada com sucesso.
 ******************************************************************************/
static bool RequestToGetFilesCount(void) {
    if (__TAKE_SEMAPHORE(xSemaphore)) {
        ps_taskRegisters->upload.filesCount   = 0;
        ps_taskRegisters->flags.getFilesCount = false;

        if (MainUc_SRV_AppWriteEvtCB(FILE_UPLOAD_PROFILE_APP,
                                     FILE_UPLOAD_GET_FILES_COUNT_CHAR,
                                     0,
                                     NULL)
            != APP_OK) {
            GIVE_SEMAPHORE(xSemaphore);
            return false;
        }

        ps_taskRegisters->flags.getFilesCount = true;
        GIVE_SEMAPHORE(xSemaphore);
    }

    return true;
}
/******************************************************************************
 * @fn          RequestToRecoveryFileInfo()
 *
 * @brief       Requisita a recuperação das informações do arquivo que está
 * sendo trabalhado pelo upload.
 * @return      Booleano - Status de retorno.
 *              @retval False - Requisição não enviada.
 *              @retval True - Requisição enviada com sucesso.
 ******************************************************************************/
static bool RequestToRecoveryFileInfo(void) {
    if (__TAKE_SEMAPHORE(xSemaphore)) {
        memset((void *)&ps_taskRegisters->upload,
               0,
               sizeof(ps_taskRegisters->upload));
        ps_taskRegisters->flags.getRecoverFileInfo = false;

        if (MainUc_SRV_AppWriteEvtCB(FILE_UPLOAD_PROFILE_APP,
                                     FILE_UPLOAD_RECOVER_FILE_INFO_CHAR,
                                     0,
                                     NULL)
            != APP_OK) {
            GIVE_SEMAPHORE(xSemaphore);
            return false;
        }

        ps_taskRegisters->flags.getRecoverFileInfo = true;
        GIVE_SEMAPHORE(xSemaphore);
    }

    return true;
}
/******************************************************************************
 * @fn          RequestToConfirmStatusBlockPosRecovered()
 *
 * @brief       Requisita a confirmação do número do bloco recuperado.
 * @return      Booleano - Status de retorno.
 *              @retval False - Requisição não enviada.
 *              @retval True - Requisição enviada com sucesso.
 ******************************************************************************/
static bool RequestToConfirmStatusBlockPosRecovered(void) {
    if (__TAKE_SEMAPHORE(xSemaphore)) {
        ps_taskRegisters->response.blockPosStatus        = false;
        ps_taskRegisters->flags.getConfirmStatusBlockPos = false;

        if (MainUc_SRV_AppWriteEvtCB(
                FILE_UPLOAD_PROFILE_APP,
                FILE_UPLOAD_CONFIRM_BLOCK_POS_RECOVERED_CHAR,
                sizeof(ps_taskRegisters->upload.blockPos),
                (uint8_t *)&ps_taskRegisters->upload.blockPos)
            != APP_OK) {
            GIVE_SEMAPHORE(xSemaphore);
            return false;
        }

        ps_taskRegisters->flags.getConfirmStatusBlockPos = true;
        GIVE_SEMAPHORE(xSemaphore);
    }

    return true;
}
/******************************************************************************
 * @fn          RequestReadFrameData()
 *
 * @brief       Requisita a leitura de parte do arquivo.
 * @return      Booleano - Status de retorno.
 *              @retval False - Requisição não enviada.
 *              @retval True - Requisição enviada com sucesso.
 ******************************************************************************/
static bool RequestReadFrameData(void) {
    if (__TAKE_SEMAPHORE(xSemaphore)) {
        ps_taskRegisters->upload.frameLen    = 0;
        ps_taskRegisters->flags.getFrameData = false;

        memset((void *)ps_taskRegisters->upload.frameData,
               0,
               sizeof(ps_taskRegisters->upload.frameData));

        uint16_t len                   = 0;
        uint8_t payload[UINT8_MAX / 8] = {[0 ...(GET_SIZE_ARRAY(payload) - 1)] =
                                              0};

        memcpy((void *)&payload[len],
               (void *)&ps_taskRegisters->upload.blockPos,
               sizeof(ps_taskRegisters->upload.blockPos));
        len += sizeof(ps_taskRegisters->upload.blockPos);

        memcpy((void *)&payload[len],
               (void *)&ps_taskRegisters->upload.framePos,
               sizeof(ps_taskRegisters->upload.framePos));
        len += sizeof(ps_taskRegisters->upload.framePos);

        if (MainUc_SRV_AppWriteEvtCB(FILE_UPLOAD_PROFILE_APP,
                                     FILE_UPLOAD_READ_FRAME_DATA_CHAR,
                                     len,
                                     payload)
            != APP_OK) {
            GIVE_SEMAPHORE(xSemaphore);
            return false;
        }

        ps_taskRegisters->flags.getFrameData = true;
        GIVE_SEMAPHORE(xSemaphore);
    }

    return true;
}
/******************************************************************************
 * @fn          CopyFrameDataToBlockData()
 *
 * @brief       Copia os dados do frame para o bloco de dados.
 ******************************************************************************/
static void CopyFrameDataToBlockData(void) {
    if (__TAKE_SEMAPHORE(xSemaphore)) {
        uint16_t bytesToWrite = ps_taskRegisters->upload.frameLen;

        memcpy((void *)(ps_taskRegisters->upload.blockData
                        + ps_taskRegisters->upload.blockLen),
               (void *)ps_taskRegisters->upload.frameData,
               bytesToWrite);
        ps_taskRegisters->upload.blockLen += bytesToWrite;

        GIVE_SEMAPHORE(xSemaphore);
    }
}
/******************************************************************************
 * @fn          UploadBlockFile()
 *
 * @brief       Copia os dados do frame para o bloco de dados.
 * @param       blockPos Número da posição do bloco.
 * @return      Booleano - Status de retorno.
 *              @retval False - Bloco não enviado.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
static bool UploadBlockFile(void) {
    SRV_WifiProvision_WaitingConnect();

    if (!ps_taskRegisters->upload.buildFileName) {
        memset(ps_taskRegisters->upload.uploadFileName,
               0,
               sizeof(ps_taskRegisters->upload.uploadFileName));

        uint64_t unixTimestamp = SRV_NTP_GetUnixTimestamp();

        char *const fileExtension =
            strstr((char *)ps_taskRegisters->upload.fileName, ".");

        sprintf(ps_taskRegisters->upload.uploadFileName,
                "CPAP_%llu_%llu%s",
                App_AppData_GetSerialNumber(),
                unixTimestamp,
                fileExtension);

        ps_taskRegisters->upload.buildFileName = true;
    }

    uint16_t blockLen = ps_taskRegisters->upload.blockLen;

#if defined(ENABLE_VERBOSE_FILE_UPLOAD_TASK)
    ESP_LOGI(TAG,
             "Enviando arquivo: %s (%d bytes)...",
             ps_taskRegisters->upload.uploadFileName,
             blockLen);
#endif

    bool httpStatus =
        SRV_HttpRequest_SendLogFile(ps_taskRegisters->upload.uploadFileName,
                                    ps_taskRegisters->upload.blockData,
                                    blockLen);

    if (httpStatus) {
        ps_taskRegisters->upload.buildFileName = false;
    } else {
        vTaskDelay(pdMS_TO_TICKS(WAIT_TIME_AFTER_FAIL_UPLOAD_MS));
    }

    return httpStatus;
}
/******************************************************************************
 * @fn          RequestToSaveProgress()
 *
 * @brief       Requisita ao STM que salve o progresso atual.
 * @return      Booleano - Status de retorno.
 *              @retval False - Requisição não enviada.
 *              @retval True - Requisição enviada com sucesso.
 ******************************************************************************/
static bool RequestToSaveProgress(void) {
    if (__TAKE_SEMAPHORE(xSemaphore)) {
        ps_taskRegisters->response.blockPosNotEqual    = false;
        ps_taskRegisters->flags.getConfirmSaveProgress = false;

        uint16_t len                   = 0;
        uint8_t payload[UINT8_MAX / 8] = {[0 ...(GET_SIZE_ARRAY(payload) - 1)] =
                                              0};

        uint16_t framePos = ps_taskRegisters->upload.framePos;
        uint16_t blockPos = ps_taskRegisters->upload.blockPos;

        memcpy((void *)&payload[len], (void *)&blockPos, sizeof(blockPos));
        len += sizeof(blockPos);

        memcpy((void *)&payload[len], (void *)&framePos, sizeof(framePos));
        len += sizeof(framePos);

        if (MainUc_SRV_AppWriteEvtCB(FILE_UPLOAD_PROFILE_APP,
                                     FILE_UPLOAD_SAVE_PROGRESS_CHAR,
                                     len,
                                     payload)
            != APP_OK) {
            GIVE_SEMAPHORE(xSemaphore);
            return false;
        }

        ps_taskRegisters->flags.getConfirmSaveProgress = true;
        GIVE_SEMAPHORE(xSemaphore);
    }

    return true;
}
/******************************************************************************
 * @fn          RequestToMoveFile()
 *
 * @brief       Requisita ao STM que move o arquivo finalizado para a pasta
 * designada.
 * @return      Booleano - Status de retorno.
 *              @retval False - Requisição não enviada.
 *              @retval True - Requisição enviada com sucesso.
 ******************************************************************************/
static bool RequestToMoveFile(void) {
    if (__TAKE_SEMAPHORE(xSemaphore)) {
        ps_taskRegisters->flags.moveUploadFile = false;

        if (MainUc_SRV_AppWriteEvtCB(FILE_UPLOAD_PROFILE_APP,
                                     FILE_UPLOAD_MOVE_UPLOAD_FILE_CHAR,
                                     0,
                                     NULL)
            != APP_OK) {
            GIVE_SEMAPHORE(xSemaphore);
            return false;
        }

        ps_taskRegisters->flags.moveUploadFile = true;
        GIVE_SEMAPHORE(xSemaphore);
    }

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

    ps_taskRegisters->upload.fileControl.all = p_payload[i++];
    return i;
}
/******************************************************************************
 * @fn          ResponseToGetHasDataRecovery()
 *
 * @brief       Processa a resposta e tenta obter o status para identificar
 * se há ou não dados para recuperação.
 * @param       p_payload Vetor com os dados.
 * @param       len Quantidade de dados no vetor, em bytes.
 ******************************************************************************/
static void ResponseToGetHasDataRecovery(uint8_t *const p_payload,
                                         uint8_t len) {
    IF_TRUE_RETURN(!ps_taskRegisters->flags.hasDataRecovery || !len);

    if (__TAKE_SEMAPHORE(xSemaphore)) {
        ps_taskRegisters->flags.hasDataRecovery = false;

        uint16_t pos = GetStatusAll(p_payload);

        if (!CheckSdCardStatus()) {
            ps_taskRegisters->transaction.answered = true;
        } else if (len > pos) {
            ps_taskRegisters->response.hasDataToRecovery = p_payload[pos];
            ps_taskRegisters->transaction.answered       = true;
        }

        GIVE_SEMAPHORE(xSemaphore);
        Task_FileUpload_Resume();
    }
}
/******************************************************************************
 * @fn          ResponseToSettedFileRecoveryInfo()
 *
 * @brief       Processa a resposta e tenta obter o nome do novo arquivo
 * definido para ser trabalhado pelo processo de upload de arquivo.
 * @param       p_payload Vetor com os dados.
 * @param       len Quantidade de dados no vetor, em bytes.
 ******************************************************************************/
static void ResponseToSettedFileRecoveryInfo(uint8_t *const p_payload,
                                             uint8_t len) {
    IF_TRUE_RETURN(!ps_taskRegisters->flags.setFileRecoveryInfo || !len);

    if (__TAKE_SEMAPHORE(xSemaphore)) {
        ps_taskRegisters->flags.setFileRecoveryInfo = false;

        uint16_t pos = GetStatusAll(p_payload);

        if (!CheckSdCardStatus()) {
            ps_taskRegisters->transaction.answered = true;
        } else if (len > pos) {
            strcpy((char *)ps_taskRegisters->upload.fileName,
                   (char *)&p_payload[pos]);
            ps_taskRegisters->transaction.answered = true;
        }

        GIVE_SEMAPHORE(xSemaphore);
        Task_FileUpload_Resume();
    }
}
/******************************************************************************
 * @fn          ResponseToRecoveryInfo()
 *
 * @brief       Processa a resposta e tenta obter as informações de recuperação
 * do arquivo atualmente sendo trabalhado pelo upload.
 * @param       p_payload Vetor com os dados.
 * @param       len Quantidade de dados no vetor, em bytes.
 ******************************************************************************/
static void ResponseToRecoveryInfo(uint8_t *const p_payload, uint8_t len) {
    IF_TRUE_RETURN(!ps_taskRegisters->flags.getRecoverFileInfo || !len);

    if (__TAKE_SEMAPHORE(xSemaphore)) {
        ps_taskRegisters->flags.getRecoverFileInfo = false;

        uint16_t pos = GetStatusAll(p_payload);

        if (!CheckSdCardStatus()) {
            ps_taskRegisters->transaction.answered = true;
        } else if (len > pos) {
            memcpy((void *)&ps_taskRegisters->upload.blockPos,
                   (void *)&p_payload[pos],
                   sizeof(ps_taskRegisters->upload.blockPos));
            pos += sizeof(ps_taskRegisters->upload.blockPos);

            memcpy((void *)&ps_taskRegisters->upload.framePos,
                   (void *)&p_payload[pos],
                   sizeof(ps_taskRegisters->upload.framePos));
            pos += sizeof(ps_taskRegisters->upload.framePos);

            if (ps_taskRegisters->upload.framePos == 0) {
                strcpy((char *)ps_taskRegisters->upload.fileName,
                       (char *)&p_payload[pos]);

                ps_taskRegisters->transaction.answered = true;
            }
        }

        GIVE_SEMAPHORE(xSemaphore);
        Task_FileUpload_Resume();
    }
}
/******************************************************************************
 * @fn          ResponseToConfirmStatusBlockPosRecovered()
 *
 * @brief       Processa a resposta e tenta obter o status de confirmação
 * do número do bloco recuperado.
 * @param       p_payload Vetor com os dados.
 * @param       len Quantidade de dados no vetor, em bytes.
 ******************************************************************************/
static void ResponseToConfirmStatusBlockPosRecovered(uint8_t *const p_payload,
                                                     uint8_t len) {
    IF_TRUE_RETURN(!ps_taskRegisters->flags.getConfirmStatusBlockPos || !len);

    if (__TAKE_SEMAPHORE(xSemaphore)) {
        ps_taskRegisters->flags.getConfirmStatusBlockPos = false;

        uint16_t pos = GetStatusAll(p_payload);

        if (!CheckSdCardStatus()) {
            ps_taskRegisters->transaction.answered = true;
        } else if (len > pos) {
            ps_taskRegisters->response.blockPosStatus = p_payload[pos] > 0;
            ps_taskRegisters->transaction.answered    = true;
        }

        GIVE_SEMAPHORE(xSemaphore);
        Task_FileUpload_Resume();
    }
}
/******************************************************************************
 * @fn          ResponseToGetCountFiles()
 *
 * @brief       Processa a resposta e tenta obter quantidade de arquivos.
 * @param       p_payload Vetor com os dados.
 * @param       len Quantidade de dados no vetor, em bytes.
 ******************************************************************************/
static void ResponseToGetCountFiles(uint8_t *const p_payload, uint8_t len) {
    IF_TRUE_RETURN(!ps_taskRegisters->flags.getFilesCount || !len);

    if (__TAKE_SEMAPHORE(xSemaphore)) {
        ps_taskRegisters->flags.getFilesCount = false;

        uint16_t pos = GetStatusAll(p_payload);

        if (!CheckSdCardStatus()) {
            ps_taskRegisters->transaction.answered = true;
        } else if (len > pos) {
            memcpy((void *)&ps_taskRegisters->upload.filesCount,
                   (void *)&p_payload[pos],
                   sizeof(ps_taskRegisters->upload.filesCount));

            ps_taskRegisters->transaction.answered = true;
        }

        GIVE_SEMAPHORE(xSemaphore);
        Task_FileUpload_Resume();
    }
}
/******************************************************************************
 * @fn          ResponseGetFrameData()
 *
 * @brief       Processa a resposta e tenta obter informações do sobre
 * e demais status importantes.
 * @param       p_payload Vetor com os dados.
 * @param       len Quantidade de dados no vetor, em bytes.
 ******************************************************************************/
static void ResponseGetFrameData(uint8_t *const p_payload, uint8_t len) {
    IF_TRUE_RETURN(!ps_taskRegisters->flags.getFrameData || !len);

    if (__TAKE_SEMAPHORE(xSemaphore)) {
        ps_taskRegisters->flags.getFrameData = false;

        uint16_t pos = GetStatusAll(p_payload);

        if (!CheckSdCardStatus()) {
            ps_taskRegisters->transaction.answered = true;

        } else if (len > pos) {
            uint16_t blockPos = 0;
            uint8_t framePos  = 0;

            memcpy((void *)&blockPos,
                   (void *)&p_payload[pos],
                   sizeof(blockPos));
            pos += sizeof(blockPos);

            memcpy((void *)&framePos,
                   (void *)&p_payload[pos],
                   sizeof(framePos));
            pos += sizeof(framePos);

            if (blockPos == ps_taskRegisters->upload.blockPos
                && framePos == ps_taskRegisters->upload.framePos) {
                ps_taskRegisters->upload.frameLen = len - pos;
                memcpy((void *)ps_taskRegisters->upload.frameData,
                       (void *)&p_payload[pos],
                       ps_taskRegisters->upload.frameLen);
                ps_taskRegisters->transaction.answered = true;
            }
        }

        GIVE_SEMAPHORE(xSemaphore);
        Task_FileUpload_Resume();
    }
}
/******************************************************************************
 * @fn          ResponseToSaveProgress()
 *
 * @brief       Processa a resposta e tenta obter os dados de confirmação
 * salvos e demais status.
 * @param       p_payload Vetor com os dados.
 * @param       len Quantidade de dados no vetor, em bytes.
 ******************************************************************************/
static void ResponseToSaveProgress(uint8_t *const p_payload, uint8_t len) {
    IF_TRUE_RETURN(!ps_taskRegisters->flags.getConfirmSaveProgress || !len);

    if (__TAKE_SEMAPHORE(xSemaphore)) {
        ps_taskRegisters->flags.getConfirmSaveProgress = false;

        uint16_t pos = GetStatusAll(p_payload);

        if (!CheckSdCardStatus()) {
            ps_taskRegisters->transaction.answered = true;

        } else if (len > pos) {
            uint16_t blockPos = 0;
            uint8_t framePos  = 0;

            memcpy((void *)&blockPos,
                   (void *)&p_payload[pos],
                   sizeof(blockPos));
            pos += sizeof(blockPos);

            memcpy((void *)&framePos,
                   (void *)&p_payload[pos],
                   sizeof(framePos));
            pos += sizeof(framePos);

            if (blockPos == ps_taskRegisters->upload.blockPos
                && framePos == ps_taskRegisters->upload.framePos) {
            } else {
                ps_taskRegisters->response.blockPosNotEqual = true;
            }
            ps_taskRegisters->transaction.answered = true;
        }

        GIVE_SEMAPHORE(xSemaphore);
        Task_FileUpload_Resume();
    }
}

/******************************************************************************
 * @fn          ResponseToMovedFileStatus()
 *
 * @brief       Processa a resposta e tenta obter o status para saber se
 * o arquivo foi movido para a pasta designada.
 * @param       p_payload Vetor com os dados.
 * @param       len Quantidade de dados no vetor, em bytes.
 ******************************************************************************/
static void ResponseToMovedFileStatus(uint8_t *const p_payload, uint8_t len) {
    IF_TRUE_RETURN(!ps_taskRegisters->flags.moveUploadFile || !len);

    if (__TAKE_SEMAPHORE(xSemaphore)) {
        ps_taskRegisters->flags.moveUploadFile = false;

        uint16_t pos = GetStatusAll(p_payload);

        if (!CheckSdCardStatus()) {
            ps_taskRegisters->transaction.answered = true;
        } else if (len > pos) {
            ps_taskRegisters->response.moveFileStatus = p_payload[pos] > 0;
            ps_taskRegisters->transaction.answered    = true;
        }

        GIVE_SEMAPHORE(xSemaphore);
        Task_FileUpload_Resume();
    }
}
/******************************************************************************
 * @fn          ShowTotalSizeFile()
 *
 * @brief       Exibe o tamanho total do arquivo.
 ******************************************************************************/
static void ShowTotalSizeFile(void) {
    uint32_t fileSize =
        ps_taskRegisters->upload.blockPos * BLOCK_DATA_LENGTH
        + (ps_taskRegisters->upload.framePos - 1) * FRAME_DATA_LENGTH
        + ps_taskRegisters->upload.frameLen;

#if defined(ENABLE_VERBOSE_FILE_UPLOAD_TASK)
    ESP_LOGI(TAG, "Tamaho total do arquivo: %lu", fileSize);
#endif
}
/******************************************************************************
 * @fn          CheckSdCardStatus()
 *
 * @brief       Informa se o status do cartão de memória está ok.
 * @return      Booleano - Status de retorno.
 *              @retval False - Seviço do cartão de memória deinicializado.
 *              @retval True - Seviço do cartão de memória operando
 *normalmente.
 ******************************************************************************/
static bool CheckSdCardStatus(void) {
    if (!ps_taskRegisters->upload.fileControl.bitFields.sdCardStatus) {
        return false;
    }

    return true;
}
/*****************************END OF FILE**************************************/
