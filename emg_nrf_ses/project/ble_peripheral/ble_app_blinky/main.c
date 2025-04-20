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
 
 #define DEVICE_NAME                     "EMG_Device"
 #define APP_BLE_CONN_CFG_TAG            1
 #define APP_BLE_OBSERVER_PRIO           3
 #define APP_ADV_INTERVAL                64
 #define APP_ADV_DURATION                BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED
 
 static uint8_t m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
 static uint8_t m_enc_advdata[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
 static uint8_t m_enc_scan_response_data[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
 
 static ble_gap_adv_data_t m_adv_data = {
     .adv_data = {
         .p_data = m_enc_advdata,
         .len    = BLE_GAP_ADV_SET_DATA_SIZE_MAX
     },
     .scan_rsp_data = {
         .p_data = m_enc_scan_response_data,
         .len    = BLE_GAP_ADV_SET_DATA_SIZE_MAX
     }
 };
 
 void ble_stack_init(void) {
     ret_code_t err_code = nrf_sdh_enable_request();
     APP_ERROR_CHECK(err_code);
 
     uint32_t ram_start = 0;
     err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
     APP_ERROR_CHECK(err_code);
 
     err_code = nrf_sdh_ble_enable(&ram_start);
     APP_ERROR_CHECK(err_code);
 }
 
 void advertising_init(void) {
     ret_code_t err_code;
     ble_advdata_t advdata;
     ble_advdata_t srdata;
 
     ble_uuid_t adv_uuids[] = {{0x180A, BLE_UUID_TYPE_BLE}};
 
     memset(&advdata, 0, sizeof(advdata));
     advdata.name_type = BLE_ADVDATA_FULL_NAME;
     advdata.include_appearance = false;
     advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
 
     memset(&srdata, 0, sizeof(srdata));
     srdata.uuids_complete.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
     srdata.uuids_complete.p_uuids = adv_uuids;
 
     err_code = ble_advdata_encode(&advdata, m_adv_data.adv_data.p_data, &m_adv_data.adv_data.len);
     APP_ERROR_CHECK(err_code);
 
     err_code = ble_advdata_encode(&srdata, m_adv_data.scan_rsp_data.p_data, &m_adv_data.scan_rsp_data.len);
     APP_ERROR_CHECK(err_code);
 
     ble_gap_adv_params_t adv_params = {0};
     adv_params.primary_phy = BLE_GAP_PHY_1MBPS;
     adv_params.duration = APP_ADV_DURATION;
     adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
     adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
     adv_params.interval = APP_ADV_INTERVAL;
 
     err_code = sd_ble_gap_adv_set_configure(&m_adv_handle, &m_adv_data, &adv_params);
     APP_ERROR_CHECK(err_code);
 }
 
 void advertising_start(void) {
     ret_code_t err_code = sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
     APP_ERROR_CHECK(err_code);
 }
 
 void log_init(void) {
     APP_ERROR_CHECK(NRF_LOG_INIT(NULL));
     NRF_LOG_DEFAULT_BACKENDS_INIT();
 }
 
 void power_management_init(void) {
     APP_ERROR_CHECK(nrf_pwr_mgmt_init());
 }

void gap_params_init(void) {
    ble_gap_conn_sec_mode_t sec_mode;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    ret_code_t err_code = sd_ble_gap_device_name_set(&sec_mode,
        (const uint8_t *)"EMG_BLE",
        strlen("EMG_BLE"));
    APP_ERROR_CHECK(err_code);
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
    log_init();
    //power_management_init();
    ble_stack_init();
    gap_params_init();
    advertising_init();
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
        }

        uint32_t currentMillis = getMillis();
        if (currentMillis - lastBlinkTime >= blinkInterval) {
            lastBlinkTime = currentMillis;
            nrf_gpio_pin_toggle(LED_PIN);
        }
    }
}
