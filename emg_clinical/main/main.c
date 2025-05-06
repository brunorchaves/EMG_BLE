#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "ADS112C04.h"

#define I2C_MASTER_SCL_IO           7      // GPIO7 = SCL no XIAO ESP32-C3
#define I2C_MASTER_SDA_IO           6      // GPIO6 = SDA no XIAO ESP32-C3
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          400000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_TIMEOUT_MS              1000

static const char *TAG = "ADS112C04_MAIN";

static void i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                                       I2C_MASTER_RX_BUF_DISABLE,
                                       I2C_MASTER_TX_BUF_DISABLE, 0));
}

void adc_task(void *arg)
{
    int16_t adc_raw;

    while (1) {
        if (ads112c04_read_data(&adc_raw) == ESP_OK) {
            printf("%d\n", adc_raw);
        } else {
            ESP_LOGE(TAG, "Falha na leitura do ADC");
        }
        vTaskDelay(pdMS_TO_TICKS(1));  // Alvo: 1 kHz
    }
}

void app_main(void)
{
    i2c_master_init();

    ESP_LOGI(TAG, "Inicializando ADS112C04...");
    ESP_ERROR_CHECK(ads112c04_init());

    ESP_LOGI(TAG, "Configurando ADS112C04 para modo RAW...");
    ESP_ERROR_CHECK(ads112c04_configure_raw_mode());

    xTaskCreate(adc_task, "adc_task", 4096, NULL, 5, NULL);
}
