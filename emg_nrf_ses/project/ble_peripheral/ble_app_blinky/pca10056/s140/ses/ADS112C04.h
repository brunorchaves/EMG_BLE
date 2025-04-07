#ifndef ADS122C04_H
#define ADS122C04_H

#include <stdint.h>
#include <stdbool.h>
#include "nrfx_twi.h"

// Default I2C address
#define ADS122C04_ADDRESS 0x40

// Commands
#define ADS122C04_RESET_CMD          0x06
#define ADS122C04_START_CMD          0x08
#define ADS122C04_POWERDOWN_CMD      0x02
#define ADS122C04_RDATA_CMD          0x10
#define ADS122C04_WREG_CMD           0x40

// Register addresses
#define ADS122C04_CONFIG_0_REG      0x00
#define ADS122C04_CONFIG_1_REG      0x01
#define ADS122C04_CONFIG_2_REG      0x02
#define ADS122C04_CONFIG_3_REG      0x03

// Raw mode configuration
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
} ads122c04_config_t;

// Function prototypes
bool ads122c04_init(nrfx_twi_t *twi_instance);
bool ads122c04_write_reg(nrfx_twi_t *twi_instance, uint8_t reg, uint8_t value);
bool ads122c04_reset(nrfx_twi_t *twi_instance);
bool ads122c04_start(nrfx_twi_t *twi_instance);
bool ads122c04_powerdown(nrfx_twi_t *twi_instance);
bool ads122c04_configure_raw_mode(nrfx_twi_t *twi_instance);
bool ads122c04_read_data(nrfx_twi_t *twi_instance, int32_t *raw_data);

#endif // ADS122C04_H