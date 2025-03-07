/* Includes ------------------------------------------------------------------*/

#include "Drivers/STMUsartDriver.h"

/* Private define ------------------------------------------------------------*/

/**
 * @brief Identificação do barramento da UART.
 */
#define UART_PORT_NUMBER UART_NUM_1

/**
 * @brief Taxa de transmissão de dados.
 */
#define UART_BAUD_RATE 115200UL

/**
 * @brief Pino para transmissão dos dados.
 */
#define UART_TX_GPIO_PIN GPIO_NUM_1

/**
 * @brief Pino para recepção dos dados.
 */
#define UART_RX_GPIO_PIN GPIO_NUM_0

/**
 * @brief Pino para controle de recepção dos dados.
 */
#define UART_RTS_GPIO_PIN UART_PIN_NO_CHANGE

/**
 * @brief Pino para controle de transmissão dos dados.
 */
#define UART_CTS_GPIO_PIN UART_PIN_NO_CHANGE

/**
 * @brief Tamanho da fila circular para armazenar dados a serem transmitidos.
 */
#define TX_RING_BUFFER_SIZE UTILS_WE_PROTOCOL_MAX_PACKET_SIZE

/**
 * @brief Tamanho da fila circular para armazenar dados a serem recebidos.
 */
#define RX_RING_BUFFER_SIZE UTILS_WE_PROTOCOL_MAX_PACKET_SIZE

/**
 * @brief Tamanho da fila de manipulação de eventos.
 */
#define EVENT_QUEUE_HANDLER_SIZE 10

#define USART_TIMEOUT_CHAR_MS ((float)UART_BAUD_RATE / (10.0f * 1000.0f))

/**
 * MACRO que calcula o tempo necessário para transmissão/recepção de dados em
 * linha ociosa.
 */
#define GET_USART_TIMEOUT_MS(len)                                            \
    (uint32_t)({                                                             \
        uint32_t idleTimeout = (uint32_t)ceilf(len / USART_TIMEOUT_CHAR_MS); \
        uint32_t timeout     = UART_BAUD_RATE < 19200UL                      \
                                   ? idleTimeout                             \
                                   : (idleTimeout < 2 ? 2 : idleTimeout);    \
        timeout;                                                             \
    })

/**
 * @brief Máxima tentativa de envios para ACK.
 */
#define MAX_ACK_RETRIES 1U

/**
 * @brief Tamanho do vetor para controle da estrutura de vetor cirular TX.
 */
#define TX_RING_BUFFER_LENGTH (10UL * UTILS_WE_PROTOCOL_MAX_PACKET_SIZE)

/**
 * @brief Tamanho do vetor para controle da estrutura de vetor cirular RX.
 */
#define RX_RING_BUFFER_LENGTH (2UL * UTILS_WE_PROTOCOL_MAX_PACKET_SIZE)

/**
 * @brief Obtém o semáforo.
 */
#define __TAKE_SEMAPHORE(semaphore) (TAKE_SEMAPHORE(semaphore, portMAX_DELAY))

/* Private typedef -----------------------------------------------------------*/

typedef enum E_HandleOutgoingStateMachine {
    SEARCH_PACKET = 0,
    BUILD_OUTPUT_PACKET,
    SEND_PACKET,
    WAIT_TRANSMITION_PACKET,
    NEXT_STATE,
    ACK_WAITING,
} E_HandleOutgoingStateMachineTypeDef;

/* Private variables ---------------------------------------------------------*/

static SemaphoreHandle_t xSemaphore = NULL;

static uint32_t ackTimer  = 0;
static uint8_t ackRetries = 0;
static bool waitingAck    = false;

static uint32_t sendTimer   = 0;
static uint32_t sendTimeout = 0;

static bool isInitializated = false;

static bool packetSent             = false;
static bool lastPackageWasReceived = true;

static rxCallbackTypeDef rxTableCallback[RX_SIZE] = {
    [0 ...(GET_SIZE_ARRAY(rxTableCallback) - 1)] = NULL};

static volatile S_WeProtocolDecodeTypeDef rxWeProtocolDecode = {0};
static volatile S_WeProtocolDecodeTypeDef txWeProtocolDecode = {0};

static S_RingBufferTypeDef *const volatile ps_rxRingBuffer = NULL;
static S_RingBufferTypeDef *const volatile ps_txRingBuffer = NULL;

static uint8_t volatile txStorageBuffer[TX_RING_BUFFER_LENGTH] = {
    [0 ...(GET_SIZE_ARRAY(txStorageBuffer) - 1)] = 0};

static uint8_t rxStorageBuffer[RX_RING_BUFFER_LENGTH] = {
    [0 ...(GET_SIZE_ARRAY(rxStorageBuffer) - 1)] = 0};

static volatile uint16_t txLen                             = 0;
static uint8_t txBuffer[UTILS_WE_PROTOCOL_MAX_PACKET_SIZE] = {
    [0 ...(GET_SIZE_ARRAY(txBuffer) - 1)] = 0};

/* Private function prototypes -----------------------------------------------*/

static bool IncomingHandler(void);
static bool OutgoingHandler(void);

static void ReadDataAvailableInRxQueue(void);

static bool HasAvailableDataInRxQueue(uint16_t *const p_countAvailable);
static uint16_t ReadDataFromRxQueue(uint8_t *const p_data, uint16_t len);

static uint16_t GetTxQueueAvailable(void);
static bool WaitTxQueueSendAll(void);

static bool SendData_IT(uint8_t *const p_data, uint16_t len);
static bool RetrySend(void);

static void ACKCallback(S_ProcessedPayloadTypeDef *const ps_processedPayload);

/* Public functions ----------------------------------------------------------*/

/******************************************************************************
 * @fn          DRV_STMUsart_Init()
 *
 * @brief       Inicializa o driver da UART responsável por comunicar com o
 *  STM32.
 * @return      Booleano - Status de retorno.
 *              @retval False - Não inicializado.
 *              @retval True - Driver inicializado com sucesso.
 ******************************************************************************/
bool DRV_STMUsart_Init(void) {
    IF_TRUE_RETURN(isInitializated, true);

    // gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT);
    // gpio_set_level(GPIO_NUM_5, 0);

    UTILS_WeProtocol_ResetDecodeState(
        (S_WeProtocolDecodeTypeDef *)&txWeProtocolDecode);
    UTILS_WeProtocol_ResetDecodeState(
        (S_WeProtocolDecodeTypeDef *)&rxWeProtocolDecode);

    if (!ps_rxRingBuffer) {
        IF_TRUE_RETURN(UTILS_RingBuffer_Create(
                           (S_RingBufferTypeDef * *const)&ps_rxRingBuffer,
                           (void *)rxStorageBuffer,
                           GET_SIZE_ARRAY(rxStorageBuffer),
                           sizeof(rxStorageBuffer[0]))
                           != APP_OK,
                       false);
    }

    if (!ps_txRingBuffer) {
        IF_TRUE_RETURN(UTILS_RingBuffer_Create(
                           (S_RingBufferTypeDef * *const)&ps_txRingBuffer,
                           (void *)txStorageBuffer,
                           GET_SIZE_ARRAY(txStorageBuffer),
                           sizeof(txStorageBuffer[0]))
                           != APP_OK,
                       false);
    }

    uart_config_t uart_config = {
        .baud_rate           = UART_BAUD_RATE,
        .data_bits           = UART_DATA_8_BITS,
        .parity              = UART_PARITY_DISABLE,
        .stop_bits           = UART_STOP_BITS_1,
        .flow_ctrl           = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = 122,
        .source_clk          = UART_SCLK_DEFAULT,
    };

    IF_TRUE_RETURN(uart_param_config(UART_PORT_NUMBER, &uart_config) != ESP_OK,
                   false);

    IF_TRUE_RETURN(uart_set_pin(UART_PORT_NUMBER,
                                UART_TX_GPIO_PIN,
                                UART_RX_GPIO_PIN,
                                UART_RTS_GPIO_PIN,
                                UART_CTS_GPIO_PIN)
                       != ESP_OK,
                   false);

    if (!uart_is_driver_installed(UART_PORT_NUMBER)) {
        IF_TRUE_RETURN(uart_driver_install(UART_PORT_NUMBER,
                                           RX_RING_BUFFER_SIZE,
                                           TX_RING_BUFFER_SIZE,
                                           EVENT_QUEUE_HANDLER_SIZE,
                                           NULL,
                                           0)
                           != ESP_OK,
                       false);
    }

    rxTableCallback[RX_ACK] = ACKCallback;

    if (!xSemaphore) {
        xSemaphore = xSemaphoreCreateBinary();
        configASSERT(xSemaphore);

        GIVE_SEMAPHORE(xSemaphore);
    }

    ackRetries      = 1;
    isInitializated = true;
    return true;
}
/******************************************************************************
 * @fn          DRV_STMUsart_DeInit()
 *
 * @brief       Deinicializa o driver da UART.
 * @return      Booleano - Status de retorno.
 *              @retval False - Não deinicializado.
 *              @retval True - Driver deinicializado com sucesso.
 ******************************************************************************/
bool DRV_STMUsart_DeInit(void) {
    IF_TRUE_RETURN(!isInitializated, true);

    IF_TRUE_RETURN(uart_driver_delete(UART_PORT_NUMBER) != ESP_OK, false);

    if (ps_rxRingBuffer) {
        IF_TRUE_RETURN(UTILS_RingBuffer_Destroy(
                           (S_RingBufferTypeDef * *const)&ps_rxRingBuffer)
                           != APP_OK,
                       false);
    }

    if (ps_txRingBuffer) {
        IF_TRUE_RETURN(UTILS_RingBuffer_Destroy(
                           (S_RingBufferTypeDef * *const)&ps_txRingBuffer)
                           != APP_OK,
                       false);
    }

    for (uint8_t i = 0; i < GET_SIZE_ARRAY(rxTableCallback); i++) {
        rxTableCallback[i] = NULL;
    }

    ackRetries = 1;

    UTILS_WeProtocol_ResetDecodeState(
        (S_WeProtocolDecodeTypeDef *)&txWeProtocolDecode);
    UTILS_WeProtocol_ResetDecodeState(
        (S_WeProtocolDecodeTypeDef *)&rxWeProtocolDecode);

    isInitializated = false;

    return true;
}
/******************************************************************************
 * @fn          DRV_STMUsart_IsInitializated()
 *
 * @brief       Informa se o driver está inicializado.
 * @return      Booleano - Status de retorno.
 *              @retval False - Não inicializado.
 *              @retval True - Driver já inicializado com sucesso.
 ******************************************************************************/
bool DRV_STMUsart_IsInitializated(void) { return isInitializated == true; }
/******************************************************************************
 * @fn          DRV_STMUsart_RegisterRxCallback()
 *
 * @brief       Registra funções de chamada de retorno.
 * @param       command Comando de recepção.
 * @param       rxCallback Função a ser registrada.
 * @return      Booleano - Status de retorno.
 *              @retval False - Erro durante tentativa de registro.
 *              @retval True - Função de callback registrada com sucesso.
 ******************************************************************************/
bool DRV_STMUsart_RegisterRxCallback(E_RxCommandTypeDef command,
                                     rxCallbackTypeDef rxCallback) {
    IF_TRUE_RETURN(!isInitializated, false);

    IF_TRUE_RETURN(command == RX_ACK || command >= RX_SIZE, false);

    rxTableCallback[command] = rxCallback;
    return true;
}
/******************************************************************************
 * @fn          DRV_STMUsart_PutPayloadInTxQueue()
 *
 * @brief       Cria um pacote incorporando os dados do payload e, por fim,
 *insere na fila de transmisão de dados.
 @param         p_payload Vetor com os dados do payload.
 @param         len Tamanho do payload, em bytes.
 * @return      Booleano - Status de retorno.
 *              @retval False - Não há espaço disponível.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
bool DRV_STMUsart_PutPayloadInTxQueue(uint8_t *const p_payload, uint16_t len) {
    IF_TRUE_RETURN(!isInitializated, false);

    if (__TAKE_SEMAPHORE(xSemaphore)) {
        vTaskDelay(pdMS_TO_TICKS(1));

        uint16_t packetLen = UTILS_WeProtocol_GetPacketSpaceRequired(len);

        uint16_t countPosAvailable = 0;
        UTILS_RingBuffer_GetSpaceAvailable(ps_txRingBuffer, &countPosAvailable);

        if (countPosAvailable < packetLen) {
            if (!WaitTxQueueSendAll()) {
                GIVE_SEMAPHORE(xSemaphore);
                return false;
            }
        }

        uint8_t response[UTILS_WE_PROTOCOL_MAX_PACKET_SIZE] = {
            [0 ...(GET_SIZE_ARRAY(response) - 1)] = 0};

        packetLen = UTILS_WeProtocol_CreatePacket(p_payload,
                                                  len,
                                                  response,
                                                  sizeof(response));

        if (!packetLen) {
            GIVE_SEMAPHORE(xSemaphore);
            return false;
        }

#if defined(ENABLE_VERBOSE_STM_USART_DRIVER)
// ESP_LOG_BUFFER_HEX("PACKET BUILD:", response, packetLen);
#endif
        vTaskDelay(pdMS_TO_TICKS(1));

        bool status = true;

        for (uint16_t i = 0; i < packetLen; i++) {
            if (UTILS_RingBuffer_Push(ps_txRingBuffer, (void *)&response[i])
                != APP_OK) {
                status = false;
                break;
            }
        }

        GIVE_SEMAPHORE(xSemaphore);
        return status;
    }

    return true;
}
/******************************************************************************
 * @fn          DRV_STMUsart_TransactionsHandler()
 *
 * @brief       Gerenciar os manipuladores de entrada e saída de dados.
 * @return      Booleano - Status de retorno.
 ******************************************************************************/
void DRV_STMUsart_TransactionsHandler(void) {
    IF_TRUE_RETURN(!isInitializated);

    IncomingHandler();
    OutgoingHandler();
}
/******************************************************************************
 * @fn          DRV_STMUsart_SetLastPackageStatus()
 *
 * @brief       Define o status de envio do último pacote.
 * @param       status Status a ser definido.
 ******************************************************************************/
void DRV_STMUsart_SetLastPackageStatus(bool status) {
    IF_TRUE_RETURN(!isInitializated);
    lastPackageWasReceived = status;
}
/******************************************************************************
 * @fn          DRV_STMUsart_GetLastPackageStatus()
 *
 * @brief       Recupera o status de envio do último pacote.
 ******************************************************************************/
bool DRV_STMUsart_GetLastPackageStatus(void) { return lastPackageWasReceived; }
/******************************************************************************
 * @fn          DRV_STMUsart_GetPacketSendStatus()
 *
 * @brief       Informa o status de envio do pacote.
 ******************************************************************************/
bool DRV_STMUsart_GetPacketSendStatus(void) { return packetSent == true; }

/* Private functions ---------------------------------------------------------*/
// ESP_LOG_BUFFER_HEX("ENTRADA =>:",
//                    decodedPayload.p_payload,
//                    decodedPayload.payloadLen);
/******************************************************************************
 * @fn          IncomingHandler()
 *
 * @brief       Gerenciar o processo de entrada de dados.
 * @return      Booleano - Status de retorno.
 ******************************************************************************/
static bool IncomingHandler(void) {
    ReadDataAvailableInRxQueue();

    E_AppStatusTypeDef appStatusCode = APP_BUFFER_EMPTY;

    do {
        E_AppStatusTypeDef appStatusCode =
            UTILS_WeProtocol_DecodePacket(&rxWeProtocolDecode,
                                          ps_rxRingBuffer,
                                          E_DECODE_TYPE_UNTIL_EMPTY,
                                          false);

        if (appStatusCode != APP_OK) {
            continue;
        }

        uint8_t cmd = rxWeProtocolDecode.payload[0];

        S_ProcessedPayloadTypeDef decodedPayload = {
            .cmd       = cmd,  // tx_cmd or another cmd
            .p_payload = (uint8_t *)&rxWeProtocolDecode
                             .payload[1],  // profile id and next data
            .payloadLen = rxWeProtocolDecode.payloadLen - 1U,
        };

        if (cmd == UTILS_WE_PROTOCOL_ACK_COMMAND) {
            rxTableCallback[RX_ACK](&decodedPayload);
            continue;
        } else if (cmd >= RX_SIZE) {
            continue;
        }

        uint8_t ackResponse[] = {
            UTILS_WE_PROTOCOL_ACK_COMMAND,  // ACK
            cmd,                            // CMD
            decodedPayload.p_payload[0],    // PROFILE ID
            decodedPayload.p_payload[1],    // CHAR ID
        };

        DRV_STMUsart_PutPayloadInTxQueue(ackResponse, sizeof(ackResponse));

        if (rxTableCallback[decodedPayload.cmd]) {
            rxTableCallback[decodedPayload.cmd](&decodedPayload);
        }
    } while (appStatusCode != APP_BUFFER_EMPTY);
    return true;
}
/******************************************************************************
 * @fn          OutgoingHandler()
 *
 * @brief       Gerenciar o processo de saída de dados.
 * @return      Booleano - Status de retorno.
 ******************************************************************************/
static bool OutgoingHandler(void) {
    static E_HandleOutgoingStateMachineTypeDef txStateMachine = SEARCH_PACKET;

    while (true) {
        switch (txStateMachine) {
            default:
            case SEARCH_PACKET:
                {
                    vTaskDelay(pdMS_TO_TICKS(1));

                    E_AppStatusTypeDef appStatusCode = APP_BUFFER_EMPTY;

                    if (__TAKE_SEMAPHORE(xSemaphore)) {
                        appStatusCode = UTILS_WeProtocol_DecodePacket(
                            (S_WeProtocolDecodeTypeDef *)&txWeProtocolDecode,
                            ps_txRingBuffer,
                            E_DECODE_TYPE_UNTIL_EMPTY,
                            true);

                        GIVE_SEMAPHORE(xSemaphore);
                    }

                    if (appStatusCode == APP_BUFFER_EMPTY) {
                        return false;
                    } else {
                        txStateMachine = BUILD_OUTPUT_PACKET;
                    }
                    break;
                }

            case BUILD_OUTPUT_PACKET:
                {
                    txLen = UTILS_WeProtocol_CreatePacket(
                        (uint8_t *)txWeProtocolDecode.payload,
                        txWeProtocolDecode.payloadLen,
                        txBuffer,
                        sizeof(txBuffer));

                    waitingAck = txWeProtocolDecode.payload[0]
                                 != UTILS_WE_PROTOCOL_ACK_COMMAND;

#if defined(ENABLE_VERBOSE_STM_USART_DRIVER)
// ESP_LOG_BUFFER_HEX("DECODED =>:", txBuffer, txLen);
#endif

                    sendTimeout    = 5UL + GET_USART_TIMEOUT_MS(txLen);
                    txStateMachine = SEND_PACKET;
                    break;
                }

            case SEND_PACKET:
                {
                    if (UTILS_Timer_WaitTimeMSec(sendTimer, sendTimeout)) {
                        vTaskDelay(pdMS_TO_TICKS(1));
                        if (SendData_IT(txBuffer, txLen)) {
                            sendTimer      = UTILS_Timer_Now();
                            txStateMachine = NEXT_STATE;
                        }
                    }
                    break;
                }

            case WAIT_TRANSMITION_PACKET:
                {
                    if (WaitTxQueueSendAll()) {
                        txStateMachine = NEXT_STATE;
                    }
                    break;
                }

            case NEXT_STATE:
                {
                    if (!waitingAck) {
                        txLen          = 0;
                        txStateMachine = SEARCH_PACKET;
                    } else {
                        ackRetries = 1;
                        ackTimer   = UTILS_Timer_GetTicks();

                        txStateMachine = ACK_WAITING;
                    }

                    return true;
                }

            case ACK_WAITING:
                {
                    if (!waitingAck) {
                        txStateMachine = SEARCH_PACKET;
                    } else if (ackRetries >= MAX_ACK_RETRIES) {
                        ackRetries             = 1;
                        txLen                  = 0;
                        waitingAck             = false;
                        lastPackageWasReceived = false;

                        txStateMachine = SEARCH_PACKET;
                    } else {
                        RetrySend();
                    }

                    return true;
                    break;
                }
        }
    }

    return true;
}
/******************************************************************************
 * @fn          ReadDataAvailableInRxQueue()
 *
 * @brief       Verifica se há dados disponíveis na fila de recepção para
 * lê-los e armazená-los na estrutura de controle de vetor circular.
 * @return      Booleano - Status de retorno.
 ******************************************************************************/
static void ReadDataAvailableInRxQueue(void) {
    uint16_t readLen = 0;

    IF_TRUE_RETURN(!HasAvailableDataInRxQueue(&readLen));

    uint16_t rxSpaceAvailable = 0;
    UTILS_RingBuffer_GetSpaceAvailable(ps_rxRingBuffer, &rxSpaceAvailable);

    if (rxSpaceAvailable < readLen) {
        return;
    }

    uint8_t readData[readLen];
    memset((void *)readData, 0, sizeof(readData));

    uint16_t countBytesRead = ReadDataFromRxQueue(readData, readLen);

    for (uint16_t i = 0; i < countBytesRead; i++) {
        if (UTILS_RingBuffer_Push(ps_rxRingBuffer, (void *)&readData[i])
            != APP_OK) {
            break;
        }
    }
}
/******************************************************************************
 * @fn          HasAvailableDataInRxQueue()
 *
 * @brief       Informa se há dados disponíveis na USART para leitura.
 * @param       p_countAvailable Ponteiro para variável que seja preenchida
 * com o valor de bytes disponíveis para leitura, se houver.
 * @return      Booleano - Status de retorno.
 *              @retval False - Não há dados disponíveis.
 *              @retval True - Há dados disponíveis para leitura.
 ******************************************************************************/
static bool HasAvailableDataInRxQueue(uint16_t *const p_countAvailable) {
    size_t countAvailable = 0;
    IF_TRUE_RETURN(
        uart_get_buffered_data_len(UART_PORT_NUMBER, &countAvailable) != ESP_OK,
        false);

    *p_countAvailable = (uint16_t)countAvailable;
    return countAvailable > 0;
}
/******************************************************************************
 * @fn          ReadDataFromRxQueue()
 *
 * @brief       Solicita a leitura de dados recebidos na USART e retorna
 * a quantidade de bytes lidos, de fato.
 * @param       p_data Ponteiro para vetor que será preenchido com os dados.
 * @param       len Quantidade de dados para serem lidos.
 * @return      uint16_t - Valor de retorno.
 ******************************************************************************/
static uint16_t ReadDataFromRxQueue(uint8_t *const p_data, uint16_t len) {
    uint32_t timeout =
        (uint32_t)(10.0f + 3.5f * (float)GET_USART_TIMEOUT_MS(len));
    uint32_t rxTimeoutTicks = UTILS_Timer_ConvertToTicks(timeout);

    int countReadBytes =
        uart_read_bytes(UART_PORT_NUMBER, (void *)p_data, len, rxTimeoutTicks);

    IF_TRUE_RETURN(countReadBytes <= 0, 0);

    return (uint16_t)countReadBytes;
}
/******************************************************************************
 * @fn          GetTxQueueAvailable()
 *
 * @brief       Informa o espaço disponível para inserir dados na fila de
 * transmissão.
 * @return      uint16_t - Valor de retorno.
 ******************************************************************************/
static uint16_t GetTxQueueAvailable(void) {
    size_t txSpaceAvailable = 0;
    uart_get_tx_buffer_free_size(UART_PORT_NUMBER, &txSpaceAvailable);

    return (uint16_t)txSpaceAvailable;
}
/******************************************************************************
 * @fn          WaitTxQueueSendAll()
 *
 * @brief       Aguarda o envio de todos os dados armazenados na fila de
 * transmissão de dados.
 * @return      Booleano - Status de retorno.
 *              @retval False - Ocorreu um erro durante a execução da operação.
 *              @retval True - Operação executada com sucesso.
 ******************************************************************************/
static bool WaitTxQueueSendAll(void) {
    uint16_t bytesToSend = TX_RING_BUFFER_SIZE - GetTxQueueAvailable();

    IF_TRUE_RETURN(!bytesToSend, true);

    uint32_t txTimeoutTicks =
        UTILS_Timer_ConvertToTicks(10UL + GET_USART_TIMEOUT_MS(bytesToSend));

    esp_err_t espStatusCode =
        uart_wait_tx_done(UART_PORT_NUMBER, txTimeoutTicks);

    IF_TRUE_RETURN(espStatusCode == ESP_ERR_TIMEOUT, false);
    IF_TRUE_RETURN(espStatusCode != ESP_OK, false);

    return true;
}
/******************************************************************************
 * @fn          SendData_IT()
 *
 * @brief       Envia os dados, em modo assíncrono.
 * @param       p_data Ponteiro com os dados.
 * @param       len Tamanho total dos dados, em bytes.
 * @return      Booleano - Status de retorno.
 *              @retval False - Ocorreu um erro durante a execução da operação.
 *              @retval True - Operação executada com sucesso (os dados foram
 * inseridos na fila de transmissão para posterior envio).
 ******************************************************************************/
static bool SendData_IT(uint8_t *const p_data, uint16_t len) {
    IF_TRUE_RETURN(!p_data || !len, false);

    packetSent = false;

    int bytesPushTxQueue =
        uart_write_bytes(UART_PORT_NUMBER, (void *)p_data, len);

    if (bytesPushTxQueue <= 0 || bytesPushTxQueue != len) {
        return false;
    }

    packetSent = true;
    return true;
}
/******************************************************************************
 * @fn          RetrySend()
 *
 * @brief       Efetua tentativas de envio de uma resposta.
 * @return      Booleano - Status de retorno.
 ******************************************************************************/
static bool RetrySend(void) {
    if (UTILS_Timer_WaitTimeMSec(ackTimer, DRV_STM_USART_ACK_TIMEOUT_MS)) {
#if defined(ENABLE_VERBOSE_STM_USART_DRIVER)
        ESP_LOGW(__FUNCTION__, "RETRY SENT");
#endif

        SendData_IT(txBuffer, txLen);

        ++ackRetries;
        ackTimer = UTILS_Timer_Now();
    }

    return true;
}
/******************************************************************************
 * @fn          ACKCallback()
 *
 * @brief       Callback quando recebe um ACK.
 * @param       ps_processedPayload Ponteiro para estrutura contendo o payload
 * processado.
 ******************************************************************************/
static void ACKCallback(S_ProcessedPayloadTypeDef *const ps_processedPayload) {
    IF_TRUE_RETURN(!waitingAck);

    if (ps_processedPayload->cmd == UTILS_WE_PROTOCOL_ACK_COMMAND
        && ps_processedPayload->p_payload[0]
               == txBuffer[UTILS_WE_PROTOCOL_START_PAYLOAD_POS]  // Cmd
        && ps_processedPayload->p_payload[1]
               == txBuffer[UTILS_WE_PROTOCOL_START_PAYLOAD_POS + 1]  // Profile
        && ps_processedPayload->p_payload[2]
               == txBuffer[UTILS_WE_PROTOCOL_START_PAYLOAD_POS + 2])  // Char ID
    {
        txLen      = 0;
        waitingAck = false;
    }
}

/*****************************END OF FILE**************************************/
