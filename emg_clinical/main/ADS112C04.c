#include "ADS112C04.h"
#include "driver/i2c.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#define I2C_PORT        I2C_NUM_0
#define I2C_TIMEOUT_MS  1000
#define ADS112C04_ADDR  0x40

static const ads112c04_config_t raw_mode_config = {
    .mux_config = 0x08,   // AIN0 to AINP, AVSS to AINN
    .gain = 0x00,
    .pga_bypass = 0x00,
    .data_rate = 0x06,
    .op_mode = 0x01,
    .conv_mode = 0x01,
    .vref = 0x02,
    .temp_sensor = 0x00,
    .idac_current = 0x00,
    .idac1_routing = 0x00,
    .idac2_routing = 0x00
};

esp_err_t ads112c04_write_reg(uint8_t reg, uint8_t value) {
    uint8_t command = ADS112C04_WREG_CMD | (reg << 2);
    uint8_t data[2] = {command, value};
    return i2c_master_write_to_device(I2C_PORT, ADS112C04_ADDR, data, 2, I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
}

esp_err_t ads112c04_send_command(uint8_t command) {
    return i2c_master_write_to_device(I2C_PORT, ADS112C04_ADDR, &command, 1, I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
}

esp_err_t ads112c04_reset(void) {
    return ads112c04_send_command(ADS112C04_RESET_CMD);
}

esp_err_t ads112c04_start(void) {
    return ads112c04_send_command(ADS112C04_START_CMD);
}

esp_err_t ads112c04_powerdown(void) {
    return ads112c04_send_command(ADS112C04_POWERDOWN_CMD);
}

esp_err_t ads112c04_init(void) {
    ESP_ERROR_CHECK(ads112c04_reset());
    vTaskDelay(pdMS_TO_TICKS(10));
    return ESP_OK;
}

esp_err_t ads112c04_configure_raw_mode(void) {
    const ads112c04_config_t *cfg = &raw_mode_config;

    ESP_ERROR_CHECK(ads112c04_write_reg(ADS112C04_CONFIG_0_REG,
        (cfg->mux_config << 4) | (cfg->gain << 1) | cfg->pga_bypass));

    ESP_ERROR_CHECK(ads112c04_write_reg(ADS112C04_CONFIG_1_REG,
        (cfg->data_rate << 5) | (cfg->op_mode << 4) | (cfg->conv_mode << 3) |
         (cfg->vref << 1) | cfg->temp_sensor));

    ESP_ERROR_CHECK(ads112c04_write_reg(ADS112C04_CONFIG_2_REG,
        (cfg->idac_current << 5)));

    ESP_ERROR_CHECK(ads112c04_write_reg(ADS112C04_CONFIG_3_REG,
        (cfg->idac1_routing << 5) | (cfg->idac2_routing << 2)));

    return ads112c04_start();
}

esp_err_t ads112c04_read_data(int16_t *raw_data) {
    uint8_t cmd = ADS112C04_RDATA_CMD;
    uint8_t rx[2];

    ESP_ERROR_CHECK(i2c_master_write_to_device(I2C_PORT, ADS112C04_ADDR, &cmd, 1, I2C_TIMEOUT_MS / portTICK_PERIOD_MS));
    ESP_ERROR_CHECK(i2c_master_read_from_device(I2C_PORT, ADS112C04_ADDR, rx, 2, I2C_TIMEOUT_MS / portTICK_PERIOD_MS));

    *raw_data = ((int16_t)rx[0] << 8) | rx[1];
    if (*raw_data & 0x8000)
        *raw_data |= 0xFFFF0000;

    return ESP_OK;
}
