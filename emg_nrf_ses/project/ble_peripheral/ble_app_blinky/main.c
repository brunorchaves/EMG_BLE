/*
 * Junção do exemplo ble_app_blinky com o código funcional do EMG.
 * Mantém o BLE ativo com advertising, sem usar funções de LED ou botões BLE.
 * Parte do main e inicialização BLE permanece, o loop original é preservado.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "nrfx_uart.h"
#include "nrfx_twi.h"
#include "nrf_gpio.h"
#include "nrfx_timer.h"
#include "ADS112C04.h"

#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "ble_advdata.h"
#include "ble_gap.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_pwr_mgmt.h"

#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"
#include "ble.h"
#include "ble_err.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_conn_params.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_pwr_mgmt.h"

#include "ble_emg_service.h" // Adicionando serviço EMG

#define DEVICE_NAME                     "EMG_BLE"
#define APP_BLE_OBSERVER_PRIO           3
#define APP_BLE_CONN_CFG_TAG            1
#define APP_ADV_INTERVAL                64
#define APP_ADV_DURATION                BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(100, UNIT_1_25_MS)
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(200, UNIT_1_25_MS)
#define SLAVE_LATENCY                   0
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)

#define FIRST_CONN_PARAMS_UPDATE_DELAY  (20000 / 0.32768) // Substituindo APP_TIMER_TICKS
#define NEXT_CONN_PARAMS_UPDATE_DELAY   (5000 / 0.32768)
#define MAX_CONN_PARAMS_UPDATE_COUNT    3

#define DEAD_BEEF                       0xDEADBEEF

NRF_BLE_GATT_DEF(m_gatt);
NRF_BLE_QWR_DEF(m_qwr);

ble_emg_service_t m_emg_service; // Instância do serviço EMG manualmente declarada

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;

static uint8_t m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
static uint8_t m_enc_advdata[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
static uint8_t m_enc_scan_response_data[BLE_GAP_ADV_SET_DATA_SIZE_MAX];

static ble_gap_adv_data_t m_adv_data =
{
    .adv_data =
    {
        .p_data = m_enc_advdata,
        .len    = BLE_GAP_ADV_SET_DATA_SIZE_MAX
    },
    .scan_rsp_data =
    {
        .p_data = m_enc_scan_response_data,
        .len    = BLE_GAP_ADV_SET_DATA_SIZE_MAX
    }
};

void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

static void timers_init(void)
{
    // Initialize timer module, making it use the scheduler
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
}
static void gap_params_init(void)
{
    ret_code_t              err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}

static void gatt_init(void)
{
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    APP_ERROR_CHECK(err_code);
}

static void advertising_init(void)
{
    ret_code_t    err_code;
    ble_advdata_t advdata;
    ble_advdata_t srdata;

    ble_uuid_t adv_uuids[] = {{EMG_SERVICE_UUID, m_emg_service.uuid_type}};

    memset(&advdata, 0, sizeof(advdata));
    advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance = true;
    advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    memset(&srdata, 0, sizeof(srdata));
    srdata.uuids_complete.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
    srdata.uuids_complete.p_uuids  = adv_uuids;

    err_code = ble_advdata_encode(&advdata, m_adv_data.adv_data.p_data, &m_adv_data.adv_data.len);
    APP_ERROR_CHECK(err_code);

    err_code = ble_advdata_encode(&srdata, m_adv_data.scan_rsp_data.p_data, &m_adv_data.scan_rsp_data.len);
    APP_ERROR_CHECK(err_code);

    ble_gap_adv_params_t adv_params;
    memset(&adv_params, 0, sizeof(adv_params));

    adv_params.primary_phy     = BLE_GAP_PHY_1MBPS;
    adv_params.duration        = APP_ADV_DURATION;
    adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
    adv_params.p_peer_addr     = NULL;
    adv_params.filter_policy   = BLE_GAP_ADV_FP_ANY;
    adv_params.interval        = APP_ADV_INTERVAL;

    err_code = sd_ble_gap_adv_set_configure(&m_adv_handle, &m_adv_data, &adv_params);
    APP_ERROR_CHECK(err_code);
}

static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

static void services_init(void)
{
    ret_code_t err_code;
    nrf_ble_qwr_init_t qwr_init = {0};

    qwr_init.error_handler = nrf_qwr_error_handler;
    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);

    err_code = ble_emg_service_init(&m_emg_service);
    APP_ERROR_CHECK(err_code);
}



/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module that
 *          are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply
 *       setting the disconnect_on_fail config parameter, but instead we use the event
 *       handler mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    ret_code_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    ret_code_t             err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for starting advertising.
 */
static void advertising_start(void)
{
    ret_code_t           err_code;

    err_code = sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
    APP_ERROR_CHECK(err_code);

    
}


/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    ret_code_t err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected");
            
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);
            m_emg_service.conn_handle = m_conn_handle; // <-- Aqui!
            
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Disconnected");
            
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            m_emg_service.conn_handle = BLE_CONN_HANDLE_INVALID; // <-- Aqui também
            APP_ERROR_CHECK(err_code);
            advertising_start();
            break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle,
                                                   BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP,
                                                   NULL,
                                                   NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            NRF_LOG_DEBUG("GATT Client Timeout.");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            NRF_LOG_DEBUG("GATT Server Timeout.");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}



static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}


/**@brief Function for initializing power management.
 */
static void power_management_init(void)
{
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
static void idle_state_handle(void)
{
    if (NRF_LOG_PROCESS() == false)
    {
        nrf_pwr_mgmt_run();
    }
}


// === DS3502 ===
#define DS3502_I2C_ADDR   0x28
#define DS3502_WIPER_REG  0x00

// === Resistance Settings (Approximate) ===
#define DS3502_RES_0_OHM       0x00  // 0 Ω
#define DS3502_RES_1K_OHM      0x0D  // ~1.0 kΩ
#define DS3502_RES_2K_OHM      0x1A  // ~2.0 kΩ
#define DS3502_RES_3K_OHM      0x27  // ~3.0 kΩ
#define DS3502_RES_4K_OHM      0x34  // ~4.0 kΩ
#define DS3502_RES_5K_OHM      0x3F  // ~5.0 kΩ
#define DS3502_RES_6K_OHM      0x4C  // ~6.0 kΩ
#define DS3502_RES_7K_OHM      0x59  // ~7.0 kΩ
#define DS3502_RES_8K_OHM      0x66  // ~8.0 kΩ
#define DS3502_RES_9K_OHM      0x73  // ~9.0 kΩ
#define DS3502_RES_10K_OHM     0x7F  // ~10.0 kΩ

// === Desired Resistance Setting ===
#define RESISTANCE_SETTING     DS3502_RES_10K_OHM

bool ds3502_set_resistance(nrfx_twi_t *twi, uint8_t value) {
    if (value > 0x7F) value = 0x7F;

    uint8_t data[2] = { DS3502_WIPER_REG, value };
    nrfx_err_t err = nrfx_twi_tx(twi, DS3502_I2C_ADDR, data, sizeof(data), false);

    return (err == NRFX_SUCCESS);
}
// === Butterworth Filter (Order 2, Bandpass 20–400 Hz, Fs = 2000 Hz) ===
#define NZEROS 4
#define NPOLES 4
#define GAIN   5.182411747f

static float xv[NZEROS + 1] = {0}, yv[NPOLES + 1] = {0};

float butterworth_filter(float input) {
    xv[0] = xv[1]; xv[1] = xv[2]; xv[2] = xv[3]; xv[3] = xv[4];
    xv[4] = input / GAIN;
    yv[0] = yv[1]; yv[1] = yv[2]; yv[2] = yv[3]; yv[3] = yv[4];
    yv[4] = (xv[0] + xv[4]) - 2 * xv[2]
          + (-0.2066719852f * yv[0]) + (0.8192636853f * yv[1])
          + (-1.9509646898f * yv[2]) + (2.3350824021f * yv[3]);
    return yv[4];
}


#define ADC_FULL_SCALE 32768  // pois o range vai de -32768 a +32767
#define VREF           5.0f
int16_t remove_offset(int16_t raw) {
    int16_t noOffset = raw - ADC_FULL_SCALE/2;
    return noOffset;
}


// === Hardware Configuration ===
#define LED_PIN           (32 + 13)
#define RST_PIN           28
#define UART_TX_PIN       (32 + 11)
#define UART_RX_PIN       (32 + 12)
#define I2C_SDA_PIN       4
#define I2C_SCL_PIN       5
#define I2C_INSTANCE_ID   0

#define FIFO_SIZE         64
#define UART_BUFFER_SIZE  16

// === Timer Setup ===
static const nrfx_timer_t micros_timer = NRFX_TIMER_INSTANCE(2);
void dummy_timer_handler(nrf_timer_event_t event_type, void *p_context) {}

void micros_timer_init(void) {
    nrfx_timer_config_t config = NRFX_TIMER_DEFAULT_CONFIG;
    config.frequency = NRF_TIMER_FREQ_1MHz;
    config.mode = NRF_TIMER_MODE_TIMER;
    config.bit_width = NRF_TIMER_BIT_WIDTH_32;
    config.interrupt_priority = NRFX_TIMER_DEFAULT_CONFIG_IRQ_PRIORITY;
    nrfx_timer_init(&micros_timer, &config, dummy_timer_handler);
    nrfx_timer_enable(&micros_timer);
}

uint32_t getMicros(void) {
    return nrfx_timer_capture(&micros_timer, NRF_TIMER_CC_CHANNEL0);
}

uint32_t getMillis(void) {
    return getMicros() / 1000;
}

// === UART Setup ===
static const nrfx_uart_t m_uart = NRFX_UART_INSTANCE(0);
static volatile bool m_tx_busy = false;
static uint8_t m_uart_buffer[UART_BUFFER_SIZE][256];
static uint8_t m_uart_buffer_head = 0;
static uint8_t m_uart_buffer_tail = 0;

void uart_handler(nrfx_uart_event_t const *p_event, void *p_context) {
    if (p_event->type == NRFX_UART_EVT_TX_DONE) {
        if (m_uart_buffer_tail != m_uart_buffer_head) {
            size_t chunk_size = strlen((const char *)m_uart_buffer[m_uart_buffer_tail]);
            nrfx_uart_tx(&m_uart, m_uart_buffer[m_uart_buffer_tail], chunk_size);
            m_uart_buffer_tail = (m_uart_buffer_tail + 1) % UART_BUFFER_SIZE;
        } else {
            m_tx_busy = false;
        }
    }
}

void uart_init(void) {
    nrfx_uart_config_t config = {
        .pseltxd = UART_TX_PIN,
        .pselrxd = UART_RX_PIN,
        .pselcts = NRF_UART_PSEL_DISCONNECTED,
        .pselrts = NRF_UART_PSEL_DISCONNECTED,
        .hwfc = NRF_UART_HWFC_DISABLED,
        .parity = NRF_UART_PARITY_EXCLUDED,
        .baudrate = NRF_UART_BAUDRATE_115200,
        .interrupt_priority = NRFX_UART_DEFAULT_CONFIG_IRQ_PRIORITY
    };
    nrfx_uart_init(&m_uart, &config, uart_handler);
}

void uart_print_async(const char *str) {
    size_t len = strlen(str);
    if (len < sizeof(m_uart_buffer[0])) {
        uint8_t next_head = (m_uart_buffer_head + 1) % UART_BUFFER_SIZE;
        if (next_head != m_uart_buffer_tail) {
            memcpy(m_uart_buffer[m_uart_buffer_head], str, len + 1);
            m_uart_buffer_head = next_head;

            if (!m_tx_busy) {
                m_tx_busy = true;
                size_t chunk_size = strlen((const char *)m_uart_buffer[m_uart_buffer_tail]);
                nrfx_uart_tx(&m_uart, m_uart_buffer[m_uart_buffer_tail], chunk_size);
                m_uart_buffer_tail = (m_uart_buffer_tail + 1) % UART_BUFFER_SIZE;
            }
        }
    }
}

// === FIFO Sample Buffer ===
static int16_t fifo[FIFO_SIZE];
static volatile uint8_t fifo_head = 0;
static volatile uint8_t fifo_tail = 0;

void fifo_push(int16_t value) {
    uint8_t next = (fifo_head + 1) % FIFO_SIZE;
    if (next != fifo_tail) {
        fifo[fifo_head] = value;
        fifo_head = next;
    }
}

bool fifo_pop(int16_t *value) {
    if (fifo_head == fifo_tail) return false;
    *value = fifo[fifo_tail];
    fifo_tail = (fifo_tail + 1) % FIFO_SIZE;
    return true;
}

// === I2C Setup ===
static nrfx_twi_t m_twi = NRFX_TWI_INSTANCE(0);

void twi_init(void) {
    nrfx_twi_config_t config = {
        .scl = I2C_SCL_PIN,
        .sda = I2C_SDA_PIN,
        .frequency = NRF_TWI_FREQ_100K,
        .interrupt_priority = NRFX_TWI_DEFAULT_CONFIG_IRQ_PRIORITY,
        .hold_bus_uninit = false
    };
    if (nrfx_twi_init(&m_twi, &config, NULL, NULL) != NRFX_SUCCESS) {
        uart_print_async("ERROR: TWI init failed!\r\n");
    }
    nrfx_twi_enable(&m_twi);
}

// === GPIO ===
void led_init(void) {
    nrf_gpio_cfg_output(LED_PIN);
    nrf_gpio_pin_write(LED_PIN, 1);
}

void gpio_init(void) {
    nrf_gpio_cfg_output(RST_PIN);
    nrf_gpio_pin_write(RST_PIN, 1);
}

// === I2C Scan ===
void i2c_scan(void) {
    uart_print_async("Starting I2C scan...\r\n");
    for (uint8_t addr = 1; addr < 127; addr++) {
        uint8_t data;
        if (nrfx_twi_rx(&m_twi, addr, &data, 1) == NRFX_SUCCESS) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Device found at 0x%02X\r\n", addr);
            uart_print_async(buf);
        }
    }
    uart_print_async("I2C scan complete.\r\n");
}

void check_ads112c04(void) {
    uint8_t data;
    for (uint8_t addr = 0x40; addr <= 0x41; addr++) {
        if (nrfx_twi_rx(&m_twi, addr, &data, 1) == NRFX_SUCCESS) {
            char buf[64];
            snprintf(buf, sizeof(buf), "ADS112C04 detected at address 0x%02X\r\n", addr);
            uart_print_async(buf);
            return;
        }
    }
    uart_print_async("ADS112C04 not detected at 0x40 or 0x41.\r\n");
}

// === Main ===
int main(void) {
    // Initialize.
    log_init();
    led_init();
    timers_init();
    // power_management_init();
    ble_stack_init();
    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();
    conn_params_init();
    // Start execution.
    NRF_LOG_INFO("Blinky example started.");
    advertising_start();
    
    micros_timer_init();
    led_init();
    gpio_init();
    uart_init();
    uart_print_async("\r\nSystem Booting...\r\n");
    
    twi_init();
    uart_print_async("TWI initialized.\r\n");

    i2c_scan(); 
    check_ads112c04();
    
    uart_print_async("Initializing ADS112C04...\r\n");
    if (!ads112c04_init(&m_twi)) {
        uart_print_async("Failed to reset ADS112C04.\r\n");
        while (1);
    }
    uart_print_async("ADS112C04 reset successful.\r\n");

    uart_print_async("Configuring ADS112C04 raw mode...\r\n");
    if (!ads112c04_configure_raw_mode(&m_twi)) {
        uart_print_async("Failed to configure ADS112C04.\r\n");
        while (1);
    }
    uart_print_async("ADS112C04 configured.\r\n");
    // Configura resistência do DS3502
    if (ds3502_set_resistance(&m_twi, RESISTANCE_SETTING)) {
        uart_print_async("DS3502 resistance set successfully.\r\n");
    } else {
        uart_print_async("Failed to set DS3502 resistance.\r\n");
    }
    uint32_t lastBlinkTime = 0;
    const uint32_t blinkInterval = 1000;
    int16_t raw_data = 0;
    int16_t out_sample = 0;
    //Loop principal while
    while (1) {
        if (ads112c04_read_data(&m_twi, &raw_data)) {
            int16_t data = remove_offset(raw_data);
            float filtered = butterworth_filter((int16_t)data);
            fifo_push((int16_t)(filtered)); // opcional: converte para mV se quiser
        }

        if (fifo_pop(&out_sample)) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d\r\n", out_sample);
            uart_print_async(buf);
            // === Enviar via BLE ===
            if (m_conn_handle != BLE_CONN_HANDLE_INVALID) {
                ble_emg_service_notify(&m_emg_service, m_emg_service.conn_handle, (uint16_t)out_sample);
            }
        }

        uint32_t currentMillis = getMillis();
        if (currentMillis - lastBlinkTime >= blinkInterval) {
            lastBlinkTime = currentMillis;
            nrf_gpio_pin_toggle(LED_PIN);
        }
    }
}
