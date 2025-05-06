#ifndef ADS112C04_H
#define ADS112C04_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Endereço I2C padrão do ADS112C04
#define ADS112C04_ADDR 0x40

// Comandos do ADS112C04
#define ADS112C04_RESET_CMD          0x06
#define ADS112C04_START_CMD          0x08
#define ADS112C04_POWERDOWN_CMD      0x02
#define ADS112C04_RDATA_CMD          0x10
#define ADS112C04_WREG_CMD           0x40

// Endereços dos registradores
#define ADS112C04_CONFIG_0_REG       0x00
#define ADS112C04_CONFIG_1_REG       0x01
#define ADS112C04_CONFIG_2_REG       0x02
#define ADS112C04_CONFIG_3_REG       0x03

// Estrutura de configuração do ADS112C04
typedef struct {
    uint8_t mux_config;
    uint8_t gain;
    uint8_t pga_bypass;
    uint8_t data_rate;
    uint8_t op_mode;
    uint8_t conv_mode;
    uint8_t vref;
    uint8_t temp_sensor;
    uint8_t idac_current;
    uint8_t idac1_routing;
    uint8_t idac2_routing;
} ads112c04_config_t;

// Protótipos das funções adaptadas para ESP-IDF
esp_err_t ads112c04_init(void);
esp_err_t ads112c04_write_reg(uint8_t reg, uint8_t value);
esp_err_t ads112c04_reset(void);
esp_err_t ads112c04_start(void);
esp_err_t ads112c04_powerdown(void);
esp_err_t ads112c04_configure_raw_mode(void);
esp_err_t ads112c04_read_data(int16_t *raw_data);

#endif // ADS112C04_H
