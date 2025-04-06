#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "nrfx_uart.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"

// Hardware Configuration (XIAO nRF52840)
// P1 port pins need +32 offset in nRF SDK
#define LED_PIN      (32 + 13)  // P1.13 (45)
#define UART_TX_PIN  (32 + 11)  // P1.11 (43)
#define UART_RX_PIN  (32 + 12)  // P1.12 (44)

// UART Instance (UART0)
static const nrfx_uart_t m_uart = NRFX_UART_INSTANCE(0);

// UART Buffers and Flags
static uint8_t m_tx_buffer[128];
static volatile bool m_tx_busy = false;

// Event Handler
void uart_handler(nrfx_uart_event_t const *p_event, void *p_context)
{
    switch (p_event->type) {
        case NRFX_UART_EVT_TX_DONE:
            m_tx_busy = false;
            break;
        case NRFX_UART_EVT_ERROR:
            // Handle errors here
            break;
        default:
            break;
    }
}

// Initialize UART
void uart_init(void)
{
    nrfx_uart_config_t config = {
        .pseltxd = UART_TX_PIN,  // Now correctly set to P1.11 (43)
        .pselrxd = UART_RX_PIN,  // Now correctly set to P1.12 (44)
        .pselcts = NRF_UART_PSEL_DISCONNECTED,
        .pselrts = NRF_UART_PSEL_DISCONNECTED,
        .hwfc = NRF_UART_HWFC_DISABLED,
        .parity = NRF_UART_PARITY_EXCLUDED,
        .baudrate = NRF_UART_BAUDRATE_115200,
        .interrupt_priority = NRFX_UART_DEFAULT_CONFIG_IRQ_PRIORITY
    };
    
    nrfx_uart_init(&m_uart, &config, uart_handler);
}

// Print String
void uart_print(const char *str)
{
    size_t len = strlen(str);
    const char *ptr = str;
    
    while (len > 0) {
        size_t chunk = len > sizeof(m_tx_buffer) ? sizeof(m_tx_buffer) : len;
        
        // Wait for previous transmission
        while (m_tx_busy) { __WFE(); }
        
        memcpy(m_tx_buffer, ptr, chunk);
        m_tx_busy = true;
        
        if (nrfx_uart_tx(&m_uart, m_tx_buffer, chunk) != NRFX_SUCCESS) {
            // Handle error
            break;
        }
        
        ptr += chunk;
        len -= chunk;
    }
}

// Initialize LED
void led_init(void)
{
    nrf_gpio_cfg_output(LED_PIN);  // Now correctly set to P1.13 (45)
    nrf_gpio_pin_write(LED_PIN, 1); // Active low
}

int main(void)
{
    led_init();
    uart_init();
    
    uart_print("\r\nSystem Started\r\n");
    uart_print("UART Demo Ready\r\n");

    uint32_t counter = 0;
    while (1) {
        // Toggle LED
        nrf_gpio_pin_toggle(LED_PIN);
        
        // Print formatted message
        char buf[32];
        snprintf(buf, sizeof(buf), "Count: %lu\r\n", counter++);
        uart_print(buf);
        
        nrf_delay_ms(500);
    }
}