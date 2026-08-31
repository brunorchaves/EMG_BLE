#ifndef ADS112C04_H
#define ADS112C04_H

#include <stdint.h>
#include <stdbool.h>
#include "nrfx_twi.h"

// Default I2C address
#define ADS112C04_ADDRESS 0x40

// === Taxa de amostragem ===
// Com data_rate = 0x06 (DR=110), o ADS112C04 entrega 1000 SPS em modo NORMAL e
// 2000 SPS em modo TURBO (o turbo dobra o clock interno do modulador e, com
// ele, a corrente do ADC).
//
// Fica no HEADER, e nao no .c, porque o main.c precisa do mesmo valor para
// escolher os coeficientes do filtro digital: eles dependem da taxa, e antes
// so existia o conjunto de 2000 Hz. Rodar a 1000 SPS dividia por dois todas as
// frequencias de corte em silencio - a banda de 20-400 Hz virava 10-200 Hz.
// Amarrando os dois ao mesmo flag, nao ha como sairem de sincronia.
//
// 1 -> 2000 SPS (validado)
// 0 -> 1000 SPS (~37% menos consumo, medido; exige os coeficientes de 1 kHz,
//                que ja estao em main.c e sao selecionados por este flag)
#ifndef ADS_TURBO_MODE
#define ADS_TURBO_MODE 0
#endif

#if ADS_TURBO_MODE
#define ADS_SAMPLE_RATE_SPS 2000
#else
#define ADS_SAMPLE_RATE_SPS 1000
#endif

// Commands
#define ADS112C04_RESET_CMD          0x06
#define ADS112C04_START_CMD          0x08
#define ADS112C04_POWERDOWN_CMD      0x02
#define ADS112C04_RDATA_CMD          0x10
#define ADS112C04_WREG_CMD           0x40

// Register addresses
#define ADS112C04_CONFIG_0_REG      0x00
#define ADS112C04_CONFIG_1_REG      0x01
#define ADS112C04_CONFIG_2_REG      0x02
#define ADS112C04_CONFIG_3_REG      0x03

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
} ads112c04_config_t;

// Function prototypes
bool ads112c04_init(nrfx_twi_t *twi_instance);
bool ads112c04_write_reg(nrfx_twi_t *twi_instance, uint8_t reg, uint8_t value);
bool ads112c04_reset(nrfx_twi_t *twi_instance);
bool ads112c04_start(nrfx_twi_t *twi_instance);
bool ads112c04_powerdown(nrfx_twi_t *twi_instance);
bool ads112c04_configure_raw_mode(nrfx_twi_t *twi_instance);
bool ads112c04_read_data(nrfx_twi_t *twi_instance, int16_t *raw_data);

#endif // ADS112C04_H
