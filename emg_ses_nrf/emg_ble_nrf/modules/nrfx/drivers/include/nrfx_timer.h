#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "nrf52840.h"
#include "nrf_gpio.h"
#include "nrfx_usbd.h"
#include "app_usbd.h"
#include "app_usbd_serial_num.h"
#include "app_usbd_cdc_acm.h"
#include "app_error.h"
#include "nrfx_timer.h"

// LED Pins (XIAO nRF52840)
#define LED1_PIN     NRF_GPIO_PIN_MAP(1, 12)
#define LED2_PIN     NRF_GPIO_PIN_MAP(1, 13)
#define RGB_BLUE_PIN NRF_GPIO_PIN_MAP(0, 6)  // Active-low

// USB Configuration
#define CDC_ACM_COMM_INTERFACE  0
#define CDC_ACM_DATA_INTERFACE 1
#define CDC_ACM_COMM_EPIN       NRF_DRV_USBD_EPIN2
#define CDC_ACM_DATA_EPIN       NRF_DRV_USBD_EPIN1
#define CDC_ACM_DATA_EPOUT      NRF_DRV_USBD_EPOUT1

// Timer configuration
#define TIMER_INSTANCE 0
static const nrfx_timer_t timer = NRFX_TIMER_INSTANCE(TIMER_INSTANCE);
static volatile uint32_t timer_ticks = 0;
static volatile bool m_usb_connected = false;

// Forward declaration of CDC ACM event handler
static void cdc_acm_user_ev_handler(app_usbd_class_inst_t const * p_inst,
                                   app_usbd_cdc_acm_user_event_t event);

// CDC ACM instance
APP_USBD_CDC_ACM_GLOBAL_DEF(m_app_cdc_acm,
                           cdc_acm_user_ev_handler,
                           CDC_ACM_COMM_INTERFACE,
                           CDC_ACM_DATA_INTERFACE,
                           CDC_ACM_COMM_EPIN,
                           CDC_ACM_DATA_EPIN,
                           CDC_ACM_DATA_EPOUT,
                           APP_USBD_CDC_COMM_PROTOCOL_AT_V250);

// Timer event handler
static void timer_event_handler(nrf_timer_event_t event, void * p_context)
{
    if (event == NRF_TIMER_EVENT_COMPARE0)
    {
        timer_ticks++;
    }
}

static void timer_init(void)
{
    nrfx_timer_config_t timer_config = NRFX_TIMER_DEFAULT_CONFIG;
    timer_config.frequency = NRF_TIMER_FREQ_1MHz;
    timer_config.mode = NRF_TIMER_MODE_TIMER;
    timer_config.bit_width = NRF_TIMER_BIT_WIDTH_32;
    
    APP_ERROR_CHECK(nrfx_timer_init(&timer, &timer_config, timer_event_handler));
    
    uint32_t ticks = nrfx_timer_ms_to_ticks(&timer, 1);
    nrfx_timer_extended_compare(&timer,
                               NRF_TIMER_CC_CHANNEL0,
                               ticks,
                               NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK,
                               true);
    nrfx_timer_enable(&timer);
}

static uint32_t get_current_time_ms(void)
{
    return timer_ticks;
}

// USB event handler
static void usbd_user_event_handler(nrfx_usbd_evt_t const * p_event)
{
    switch (p_event->type)
    {
        case NRFX_USBD_EVT_DRV_SUSPEND:
            break;
            
        case NRFX_USBD_EVT_DRV_RESUME:
            break;
            
        case NRFX_USBD_EVT_STARTED:
            break;
            
        case NRFX_USBD_EVT_STOPPED:
            app_usbd_disable();
            break;
            
        case NRFX_USBD_EVT_POWER_DETECTED:
            if (!nrfx_usbd_is_enabled())
            {
                app_usbd_enable();
            }
            break;
            
        case NRFX_USBD_EVT_POWER_REMOVED:
            app_usbd_stop();
            m_usb_connected = false;
            break;
            
        case NRFX_USBD_EVT_POWER_READY:
            app_usbd_start();
            m_usb_connected = true;
            break;
            
        default:
            break;
    }
}

// CDC ACM event handler implementation
static void cdc_acm_user_ev_handler(app_usbd_class_inst_t const * p_inst,
                                   app_usbd_cdc_acm_user_event_t event)
{
    switch (event)
    {
        case APP_USBD_CDC_ACM_USER_EVT_PORT_OPEN:
            m_usb_connected = true;
            break;
            
        case APP_USBD_CDC_ACM_USER_EVT_PORT_CLOSE:
            m_usb_connected = false;
            break;
            
        case APP_USBD_CDC_ACM_USER_EVT_TX_DONE:
            break;
            
        case APP_USBD_CDC_ACM_USER_EVT_RX_DONE:
            break;
            
        default:
            break;
    }
}

static void usb_init(void)
{
    ret_code_t ret;
    
    // Initialize USB driver
    ret = nrfx_usbd_init(usbd_user_event_handler);
    APP_ERROR_CHECK(ret);
    
    // Generate serial number
    app_usbd_serial_num_generate();
    
    // Initialize CDC ACM
    ret = app_usbd_class_append(app_usbd_cdc_acm_class_inst_get(&m_app_cdc_acm));
    APP_ERROR_CHECK(ret);
    
    // Enable USB
    app_usbd_enable();
    app_usbd_start();
}

static void usb_print(const char* str)
{
    if (!m_usb_connected) return;
    app_usbd_cdc_acm_write(&m_app_cdc_acm, (const uint8_t*)str, strlen(str));
}

int main(void)
{
    // Initialize peripherals
    timer_init();
    usb_init();
    
    // Configure LEDs
    nrf_gpio_cfg_output(LED1_PIN);
    nrf_gpio_cfg_output(LED2_PIN);
    nrf_gpio_cfg_output(RGB_BLUE_PIN);
    
    // Initial state
    nrf_gpio_pin_set(LED1_PIN);
    nrf_gpio_pin_clear(LED2_PIN);
    nrf_gpio_pin_clear(RGB_BLUE_PIN);

    uint32_t last_print_time = 0;
    uint32_t last_toggle_time = 0;
    bool led_state = false;

    while (1)
    {
        uint32_t current_time = get_current_time_ms();
        
        // Toggle LEDs every 500ms
        if (current_time - last_toggle_time >= 500)
        {
            led_state = !led_state;
            nrf_gpio_pin_write(LED1_PIN, led_state);
            nrf_gpio_pin_write(LED2_PIN, !led_state);
            nrf_gpio_pin_write(RGB_BLUE_PIN, !led_state);
            last_toggle_time = current_time;
        }
        
        // Print status every 1000ms
        if (current_time - last_print_time >= 1000)
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "LED1: %s | LED2: %s | Blue: %s\r\n",
                    led_state ? "ON" : "OFF", 
                    !led_state ? "ON" : "OFF",
                    !led_state ? "ON" : "OFF");
            usb_print(buf);
            last_print_time = current_time;
        }
    }
}