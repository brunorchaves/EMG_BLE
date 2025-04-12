#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "nrfx_uart.h"
#include "nrfx_twi.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "ADS112C04.h"
#include "nrfx_timer.h"

// === Hardware Configuration ===
#define LED_PIN        (32 + 13)  // P1.13
#define RST_PIN        28
#define UART_TX_PIN    (32 + 11)  // P1.11
#define UART_RX_PIN    (32 + 12)  // P1.12
#define I2C_SDA_PIN    4          // P0.04
#define I2C_SCL_PIN    5          // P0.05
#define I2C_INSTANCE_ID 0
#define BUFFER_SIZE 10  // Define the size of the buffer for storing 10 samples

// === Timer Setup ===
static const nrfx_timer_t micros_timer = NRFX_TIMER_INSTANCE(2);  // Pode ser 0, 1, 2...
void dummy_timer_handler(nrf_timer_event_t event_type, void *p_context) {
    // Nada a fazer
}
void micros_timer_init(void) {
    nrfx_timer_config_t config = NRFX_TIMER_DEFAULT_CONFIG;
    config.frequency = NRF_TIMER_FREQ_1MHz; // 1 tick = 1 us
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
static uint8_t m_tx_buffer[128];
static volatile bool m_tx_busy = false;
static uint8_t m_uart_buffer[BUFFER_SIZE][64];  // Buffer to store 10 samples as strings
static uint8_t m_uart_buffer_head = 0;
static uint8_t m_uart_buffer_tail = 0;

void uart_handler(nrfx_uart_event_t const *p_event, void *p_context) {
    if (p_event->type == NRFX_UART_EVT_TX_DONE) {
        m_tx_busy = false;
        // Check if there are more items in the buffer to send
        if (m_uart_buffer_head != m_uart_buffer_tail) {
            size_t chunk_size = strlen((const char *)m_uart_buffer[m_uart_buffer_tail]);
            nrfx_uart_tx(&m_uart, m_uart_buffer[m_uart_buffer_tail], chunk_size);
            m_uart_buffer_tail = (m_uart_buffer_tail + 1) % BUFFER_SIZE;
            m_tx_busy = true;
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
        .baudrate = NRF_UART_BAUDRATE_250000,
        .interrupt_priority = NRFX_UART_DEFAULT_CONFIG_IRQ_PRIORITY
    };
    nrfx_uart_init(&m_uart, &config, uart_handler);
}

void uart_print_async(const char *str) {
    size_t len = strlen(str);
    if (len < 64) {  // Ensure the string is small enough to fit in the buffer
        memcpy(m_uart_buffer[m_uart_buffer_head], str, len);
        m_uart_buffer_head = (m_uart_buffer_head + 1) % BUFFER_SIZE;
        if (m_uart_buffer_head != m_uart_buffer_tail) {
            // If there's a buffer space, send the data
            if (!m_tx_busy) {
                nrfx_uart_tx(&m_uart, m_uart_buffer[m_uart_buffer_tail], len);
                m_uart_buffer_tail = (m_uart_buffer_tail + 1) % BUFFER_SIZE;
                m_tx_busy = true;
            }
        }
    }
}

// === Circular Buffer for 10 samples ===
#define BUFFER_SIZE 10
int16_t sample_buffer[BUFFER_SIZE];
uint8_t buffer_head = 0;
uint8_t buffer_tail = 0;

void buffer_add_sample(int16_t sample) {
    sample_buffer[buffer_head] = sample;
    buffer_head = (buffer_head + 1) % BUFFER_SIZE;
    if (buffer_head == buffer_tail) {
        buffer_tail = (buffer_tail + 1) % BUFFER_SIZE;  // Overwrite the oldest sample
    }
}

void buffer_send_samples(void) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%d\r\n", sample_buffer[buffer_tail]);
    uart_print_async(buf);
    buffer_tail = (buffer_tail + 1) % BUFFER_SIZE;
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
        //while (1);  // Halt
    }

    nrfx_twi_enable(&m_twi);
}

// === LED and GPIO Setup ===
void led_init(void) {
    nrf_gpio_cfg_output(LED_PIN);
    nrf_gpio_pin_write(LED_PIN, 1); // Off (active-low)
}

void gpio_init(void) {
    nrf_gpio_cfg_output(RST_PIN);
    nrf_gpio_pin_write(RST_PIN, 1); // Keep device out of reset
}

// === I2C Scan ===
void i2c_scan(void) {
    uart_print_async("Starting I2C scan...\r\n");
    for (uint8_t addr = 1; addr < 127; addr++) {
        uint8_t data;
        ret_code_t err = nrfx_twi_rx(&m_twi, addr, &data, 1);
        if (err == NRFX_SUCCESS) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Device found at 0x%02X\r\n", addr);
            uart_print_async(buf);
        }
        //nrf_delay_ms(5);
    }
    uart_print_async("I2C scan complete.\r\n");
}

// === ADS112C04 Detection ===
void check_ads112c04(void) {
    uint8_t data;
    for (uint8_t addr = 0x40; addr <= 0x41; addr++) {
        ret_code_t err = nrfx_twi_rx(&m_twi, addr, &data, 1);
        if (err == NRFX_SUCCESS) {
            char buf[64];
            snprintf(buf, sizeof(buf), "ADS112C04 detected at address 0x%02X\r\n", addr);
            uart_print_async(buf);
            return;
        }
    }
    uart_print_async("ADS112C04 not detected at 0x40 or 0x41.\r\n");
}

int main(void) {
    micros_timer_init();
    led_init();
    gpio_init();
    uart_init();
    uart_print_async("\r\nSystem Booting...\r\n");

    twi_init();
    uart_print_async("TWI initialized.\r\n");

    i2c_scan();  // One-time scan
    check_ads112c04();

    // === Initialize ADS112C04 ===
    uart_print_async("Initializing ADS112C04...\r\n");
    if (!ads112c04_init(&m_twi)) {
        uart_print_async("Failed to reset ADS112C04.\r\n");
        while (1); // Halt
    }
    uart_print_async("ADS112C04 reset successful.\r\n");

    // === Configure ADS112C04 for raw mode ===
    uart_print_async("Configuring ADS112C04 raw mode...\r\n");
    if (!ads112c04_configure_raw_mode(&m_twi)) {
        uart_print_async("Failed to configure ADS112C04.\r\n");
        while (1); // Halt
    }
    uart_print_async("ADS112C04 configured.\r\n");

    // === Blink control ===
    uint32_t lastBlinkTime = 0;
    const uint32_t blinkInterval = 1000; // ms
    int16_t raw_data = 0;
    // === Continuous Data Reading Loop ===
    while (1) {
        if (ads112c04_read_data(&m_twi, &raw_data)) {
            buffer_add_sample(raw_data);
            buffer_send_samples();  // Send the sample from the buffer
        } else {
            uart_print_async("Failed to read data from ADS112C04.\r\n");
        }
        
        // === Non-blocking LED blink using millis ===
        uint32_t currentMillis = getMillis();
        if (currentMillis - lastBlinkTime >= blinkInterval) {
            lastBlinkTime = currentMillis;
            nrf_gpio_pin_toggle(LED_PIN);
        }
    }
}
