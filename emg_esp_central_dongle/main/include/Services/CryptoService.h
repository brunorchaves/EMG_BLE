/******************************************************************************
 * @file         : CryptoService.h
 * @authors      : Thales Mol Vinhal
 * @copyright    : Copyright (c) 2023 CMOS Drake
 * @brief        : Serviço para provisão de Algoritmos de Criptografia
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef CRYPTO_SERVICE_H
#define CRYPTO_SERVICE_H

#ifdef __cplusplus
extern "C" 
{
#endif

/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <esp_log.h>

#include "mbedtls/aes.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/platform.h"

#include "App/AppData.h"
#include "Services/NTPService.h"

/* Public functions ----------------------------------------------------------*/

void SRV_Crypto_GenerateSecretKeyId(char * p_secretKey, char * p_id);

#ifdef __cplusplus
}
#endif

#endif /* CRYPTO_SERVICE_H */

/*****************************END OF FILE**************************************/

