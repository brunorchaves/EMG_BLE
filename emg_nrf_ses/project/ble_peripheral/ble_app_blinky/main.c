#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "nrfx_uart.h"
#include "nrfx_twi.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"

// === Hardware Configuration ===
#define LED_PIN        (32 + 13)  // P1.13
#define RST_PIN        28
#define UART_TX_PIN    (32 + 11)  // P1.11
#define UART_RX_PIN    (32 + 12)  // P1.12
#define I2C_SDA_PIN    4          // P0.04
#define I2C_SCL_PIN    5          // P0.05
#define I2C_INSTANCE_ID 0

// === UART Setup ===
static const nrfx_uart_t m_uart = NRFX_UART_INSTANCE(0);
static uint8_t m_tx_buffer[128];
static volatile bool m_tx_busy = false;

void uart_handler(nrfx_uart_event_t const *p_event, void *p_context) {
    if (p_event->type == NRFX_UART_EVT_TX_DONE) {
        m_tx_busy = false;
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

void uart_print(const char *str) {
    size_t len = strlen(str);
    const char *ptr = str;

    while (len > 0) {
        size_t chunk = len > sizeof(m_tx_buffer) ? sizeof(m_tx_buffer) : len;

        while (m_tx_busy) { __WFE(); }

        memcpy(m_tx_buffer, ptr, chunk);
        if (nrfx_uart_tx(&m_uart, m_tx_buffer, chunk) == NRFX_SUCCESS) {
            m_tx_busy = true;
        } else {
            break;
        }

        ptr += chunk;
        len -= chunk;
    }
}

// === I2C Setup ===
static const nrfx_twi_t m_twi = NRFX_TWI_INSTANCE(0);

void twi_init(void) {
    nrfx_twi_config_t config = {
        .scl = I2C_SCL_PIN,
        .sda = I2C_SDA_PIN,
        .frequency = NRF_TWI_FREQ_100K,
        .interrupt_priority = NRFX_TWI_DEFAULT_CONFIG_IRQ_PRIORITY,
        .hold_bus_uninit = false
    };

    if (nrfx_twi_init(&m_twi, &config, NULL, NULL) != NRFX_SUCCESS) {
        uart_print("ERROR: TWI init failed!\r\n");
        while (1);  // Halt
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
    uart_print("Starting I2C scan...\r\n");
    for (uint8_t addr = 1; addr < 127; addr++) {
        uint8_t data;
        ret_code_t err = nrfx_twi_rx(&m_twi, addr, &data, 1);
        if (err == NRFX_SUCCESS) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Device found at 0x%02X\r\n", addr);
            uart_print(buf);
        }
        nrf_delay_ms(5);
    }
    uart_print("I2C scan complete.\r\n");
}

// === ADS1120 Detection ===
void check_ads1120(void) {
    uint8_t data;
    for (uint8_t addr = 0x40; addr <= 0x41; addr++) {
        ret_code_t err = nrfx_twi_rx(&m_twi, addr, &data, 1);
        if (err == NRFX_SUCCESS) {
            char buf[64];
            snprintf(buf, sizeof(buf), "ADS1120 detected at address 0x%02X\r\n", addr);
            uart_print(buf);
            return;
        }
    }
    uart_print("ADS1120 not detected at 0x40 or 0x41.\r\n");
}

// === Main ===
int main(void) {
    led_init();
    gpio_init();
    uart_init();
    uart_print("\r\nSystem Booting...\r\n");

    twi_init();
    uart_print("TWI initialized.\r\n");

    i2c_scan();  // One-time scan
    check_ads1120();

    while (1) {
        nrf_gpio_pin_toggle(LED_PIN);
        nrf_delay_ms(1000);
    }
}
