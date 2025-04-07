#include "ADS112C04.h"
#include "nrf_delay.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
// Default raw mode configuration
static const ads122c04_config_t raw_mode_config = {
    .mux_config = 0x08,   // AIN0 to AINP, AVSS to AINN
    .gain = 0x00,         // Gain = 1
    .pga_bypass = 0x00,   // PGA enabled
    .data_rate = 0x06,    // 1000 SPS
    .op_mode = 0x01,      // Turbo mode
    .conv_mode = 0x01,    // Continuous conversion
    .vref = 0x02,         // AVDD as reference
    .temp_sensor = 0x00,  // Temp sensor off
    .idac_current = 0x00, // IDAC off
    .idac1_routing = 0x00,// IDAC1 disabled
    .idac2_routing = 0x00 // IDAC2 disabled
};

bool ads122c04_init(nrfx_twi_t *twi_instance) {
    // Reset the device
    if (!ads122c04_reset(twi_instance)) {
        return false;
    }
    nrf_delay_ms(10);  // Wait for reset to complete
    return true;
}

bool ads122c04_write_reg(nrfx_twi_t *twi_instance, uint8_t reg, uint8_t value) {
    uint8_t command = ADS122C04_WREG_CMD | (reg << 2);
    uint8_t data[2] = {command, value};
    ret_code_t err = nrfx_twi_tx(twi_instance, ADS122C04_ADDRESS, data, sizeof(data), false);
    return (err == NRFX_SUCCESS);
}

bool ads122c04_send_command(nrfx_twi_t *twi_instance, uint8_t command) {
    return (nrfx_twi_tx(twi_instance, ADS122C04_ADDRESS, &command, 1, false) == NRFX_SUCCESS);
}

bool ads122c04_reset(nrfx_twi_t *twi_instance) {
    uint8_t cmd = ADS122C04_RESET_CMD;
    return ads122c04_send_command(twi_instance, cmd);
}

bool ads122c04_start(nrfx_twi_t *twi_instance) {
    uint8_t cmd = ADS122C04_START_CMD;
    return ads122c04_send_command(twi_instance, cmd);
}

bool ads122c04_powerdown(nrfx_twi_t *twi_instance) {
    uint8_t cmd = ADS122C04_POWERDOWN_CMD;
    return ads122c04_send_command(twi_instance, cmd);
}

bool ads122c04_configure_raw_mode(nrfx_twi_t *twi_instance) {
    // Configure registers for raw mode
    bool success = true;
    const ads122c04_config_t *config = &raw_mode_config;
    
    success &= ads122c04_write_reg(twi_instance, ADS122C04_CONFIG_0_REG, 
                                 (config->mux_config << 4) |
                                 (config->gain << 1) |
                                 config->pga_bypass);

    success &= ads122c04_write_reg(twi_instance, ADS122C04_CONFIG_1_REG,
                                 (config->data_rate << 5) |
                                 (config->op_mode << 4) |
                                 (config->conv_mode << 3) |
                                 (config->vref << 1) |
                                 config->temp_sensor);

    success &= ads122c04_write_reg(twi_instance, ADS122C04_CONFIG_2_REG,
                                 (config->idac_current << 5));

    success &= ads122c04_write_reg(twi_instance, ADS122C04_CONFIG_3_REG,
                                 (config->idac1_routing << 5) |
                                 (config->idac2_routing << 2));

    if (success) {
        // Start conversions
        if (!ads122c04_start(twi_instance)) {
            return false;
        }
        return true;
    }
    return false;
}

bool ads122c04_read_data(nrfx_twi_t *twi_instance, int32_t *raw_data) {
    uint8_t rx_data[3] = {0};
    uint8_t read_cmd = ADS122C04_RDATA_CMD;  // Store command in variable
    
    // Send read command
    if (nrfx_twi_tx(twi_instance, ADS122C04_ADDRESS, &read_cmd, 1, false) != NRFX_SUCCESS) {
        return false;
    }
    
    // Read 3 bytes of data
    if (nrfx_twi_rx(twi_instance, ADS122C04_ADDRESS, rx_data, sizeof(rx_data)) != NRFX_SUCCESS) {
        return false;
    }
    
    // Combine bytes into 24-bit signed integer
    *raw_data = ((int32_t)rx_data[0] << 16) | ((int32_t)rx_data[1] << 8) | rx_data[2];
    
    // Sign extend if negative
    if (*raw_data & 0x00800000) {
        *raw_data |= 0xFF000000;
    }
    
    return true;
}