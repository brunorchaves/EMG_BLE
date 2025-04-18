#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "nrfx_uart.h"
#include "nrfx_twi.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "ADS112C04.h"
#include "nrfx_timer.h"

// === Butterworth Filter (Order 2, Bandpass 20–480 Hz, Fs = 2000 Hz) ===
#define NZEROS 4
#define NPOLES 4
#define GAIN   3.851631157f
static float xv[NZEROS+1] = {0}, yv[NPOLES+1] = {0};

float butterworth_filter(float input) {
    xv[0] = xv[1]; xv[1] = xv[2]; xv[2] = xv[3]; xv[3] = xv[4];
    xv[4] = input / GAIN;
    yv[0] = yv[1]; yv[1] = yv[2]; yv[2] = yv[3]; yv[3] = yv[4];
    yv[4] = (xv[0] + xv[4]) - 2 * xv[2]
          + (-0.1754120032f * yv[0]) + (0.4695834221f * yv[1])
          + (-1.3227301903f * yv[2]) + (2.0242112308f * yv[3]);
    return yv[4];
}

// === Convert raw ADC value to voltage and remove offset ===
#define ADC_MAX_VALUE 65535.0f
#define VREF           5.0f
#define OFFSET_VOLTAGE 2.5f

float convert_to_voltage(int16_t raw) {
    float voltage = ((float)raw / ADC_MAX_VALUE) * VREF;
    return voltage - OFFSET_VOLTAGE;
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

    uint32_t lastBlinkTime = 0;
    const uint32_t blinkInterval = 1000;
    int16_t raw_data = 0;
    int16_t out_sample = 0;

    while (1) {
        if (ads112c04_read_data(&m_twi, &raw_data)) {
            float filtered = butterworth_filter(raw_data);
            float voltage = convert_to_voltage(filtered);
            fifo_push((int16_t)(filtered * 1000)); // opcional: converte para mV se quiser
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
