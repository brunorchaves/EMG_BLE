#include "ADS112C04.h"
#include "nrf_delay.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
// ADS_TURBO_MODE e ADS_SAMPLE_RATE_SPS vem de ADS112C04.h, para que main.c
// use o MESMO flag ao escolher os coeficientes do filtro digital.
//
// Nota sobre anti-aliasing: este e um ADC delta-sigma. O modulador roda a
// centenas de kHz e o filtro digital interno atenua de fDR/2 ate fMOD, onde a
// resposta se repete. Ou seja, o filtro analogico da placa (Sallen-Key de 2a
// ordem em 482 Hz) so precisa bloquear conteudo perto de fMOD, onde tem folga
// enorme (~109 dB em 256 kHz) - nao precisa ser brando ou agressivo em torno
// de 500 Hz. Manter o analogico como esta e correto tanto a 1 kSPS quanto a
// 2 kSPS; o que depende da taxa sao apenas os coeficientes do filtro digital.

// Default raw mode configuration
static const ads112c04_config_t raw_mode_config = {
    .mux_config = 0x08,   // AIN0 to AINP, AVSS to AINN
    .gain = 0x00,         // Gain = 1
    .pga_bypass = 0x00,   // PGA enabled
    .data_rate = 0x06,    // DR=110: 1000 SPS em normal, 2000 SPS em turbo
    .op_mode = ADS_TURBO_MODE,  // 1 = turbo (2000 SPS), 0 = normal (1000 SPS)
    .conv_mode = 0x01,    // Continuous conversion
    .vref = 0x02,         // AVDD as reference
    .temp_sensor = 0x00,  // Temp sensor off
    .idac_current = 0x00, // IDAC off
    .idac1_routing = 0x00,// IDAC1 disabled
    .idac2_routing = 0x00 // IDAC2 disabled
};

bool ads112c04_init(nrfx_twi_t *twi_instance) {
    // Reset the device
    if (!ads112c04_reset(twi_instance)) {
        return false;
    }
    nrf_delay_ms(10);  // Wait for reset to complete
    return true;
}

bool ads112c04_write_reg(nrfx_twi_t *twi_instance, uint8_t reg, uint8_t value) {
    uint8_t command = ADS112C04_WREG_CMD | (reg << 2);
    uint8_t data[2] = {command, value};
    ret_code_t err = nrfx_twi_tx(twi_instance, ADS112C04_ADDRESS, data, sizeof(data), false);
    return (err == NRFX_SUCCESS);
}

bool ads112c04_send_command(nrfx_twi_t *twi_instance, uint8_t command) {
    return (nrfx_twi_tx(twi_instance, ADS112C04_ADDRESS, &command, 1, false) == NRFX_SUCCESS);
}

bool ads112c04_reset(nrfx_twi_t *twi_instance) {
    uint8_t cmd = ADS112C04_RESET_CMD;
    return ads112c04_send_command(twi_instance, cmd);
}

bool ads112c04_start(nrfx_twi_t *twi_instance) {
    uint8_t cmd = ADS112C04_START_CMD;
    return ads112c04_send_command(twi_instance, cmd);
}

bool ads112c04_powerdown(nrfx_twi_t *twi_instance) {
    uint8_t cmd = ADS112C04_POWERDOWN_CMD;
    return ads112c04_send_command(twi_instance, cmd);
}

bool ads112c04_configure_raw_mode(nrfx_twi_t *twi_instance) {
    // Configure registers for raw mode
    bool success = true;
    const ads112c04_config_t *config = &raw_mode_config;
    
    success &= ads112c04_write_reg(twi_instance, ADS112C04_CONFIG_0_REG, 
                                 (config->mux_config << 4) |
                                 (config->gain << 1) |
                                 config->pga_bypass);

    success &= ads112c04_write_reg(twi_instance, ADS112C04_CONFIG_1_REG,
                                 (config->data_rate << 5) |
                                 (config->op_mode << 4) |
                                 (config->conv_mode << 3) |
                                 (config->vref << 1) |
                                 config->temp_sensor);

    success &= ads112c04_write_reg(twi_instance, ADS112C04_CONFIG_2_REG,
                                 (config->idac_current << 5));

    success &= ads112c04_write_reg(twi_instance, ADS112C04_CONFIG_3_REG,
                                 (config->idac1_routing << 5) |
                                 (config->idac2_routing << 2));

    if (success) {
        // Start conversions
        if (!ads112c04_start(twi_instance)) {
            return false;
        }
        return true;
    }
    return false;
}

bool ads112c04_read_data(nrfx_twi_t *twi_instance, int16_t *raw_data) {
    uint8_t rx_data[2] = {0};
    uint8_t read_cmd = ADS112C04_RDATA_CMD;  // Store command in variable
    
    // Send read command
    if (nrfx_twi_tx(twi_instance, ADS112C04_ADDRESS, &read_cmd, 1, false) != NRFX_SUCCESS) {
        return false;
    }
    
    // Read 2 bytes of data
    if (nrfx_twi_rx(twi_instance, ADS112C04_ADDRESS, rx_data, sizeof(rx_data)) != NRFX_SUCCESS) {
        return false;
    }
    
    // Combine bytes into 16-bit signed integer
    *raw_data = ((int16_t)rx_data[0] << 8) | rx_data[1];
    
    // Sign extend if negative
    if (*raw_data & 0x8000) {
        *raw_data |= 0xFFFF0000;
    }
    
    return true;
}
