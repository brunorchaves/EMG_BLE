/******************************************************************************
 * @file         : CryptoService.c
 * @authors      : Thales Mol Vinhal
 * @copyright    : Copyright (c) 2023 CMOS Drake
 * @brief        : Serviço para provisão de Algoritmos de Criptografia
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/

#include "Services/CryptoService.h"

/* Private define ------------------------------------------------------------*/

#define IV_SIZE 16

#define INPUT_LENGTH      128
#define SECRET_KEY_LENGTH (AES_KEY_SIZE + 21)

#define TAG "CRYPTO_SERVICE"

/* Private typedef -----------------------------------------------------------*/

static ApiCredentialsTypeDef *const volatile ps_apiCredentials = NULL;

/* Private function prototypes -----------------------------------------------*/

static void BytesToHex(const uint8_t *p_buffer,
                       size_t bufferSize,
                       char *p_hexOutput);
static void GenerateRandomIV(uint8_t *p_iv);
static uint8_t *ScrambleSecretKey(ApiCredentialsTypeDef *ps_apiCredentials);
static void WorkBytes(uint8_t *p_iv,
                      uint8_t *p_output,
                      uint8_t *p_input,
                      size_t numBytes);

/* Private variables ---------------------------------------------------------*/
/* Public functions ----------------------------------------------------------*/

/******************************************************************************
 * @brief Função para gerar a chave secreta e o id para acesso ao Configr.
 *
 * @param p_secretKey Ponteiro para a chave secreta que será gerada.
 *
 * @param p_id Ponteiro para o ID aleatório que será gerado.
 ******************************************************************************/
void SRV_Crypto_GenerateSecretKeyId(char *p_secretKey, char *p_id) {
    if (!ps_apiCredentials) {
        size_t *ps_aux = (size_t *)&ps_apiCredentials;
        *ps_aux        = (size_t)App_AppData_GetApiCredentials();
    }
    
    uint8_t *p_input                    = ScrambleSecretKey(ps_apiCredentials);
    uint8_t p_output[SECRET_KEY_LENGTH] = {0};
    uint8_t ivBuffer[16]                = {0};
    GenerateRandomIV(ivBuffer);

    uint8_t ivBufferAux[IV_SIZE] = {0};
    memcpy(ivBufferAux, ivBuffer, IV_SIZE);

    WorkBytes(ivBufferAux, p_output, p_input, SECRET_KEY_LENGTH);

    char inputString[SECRET_KEY_LENGTH];
    snprintf(inputString, SECRET_KEY_LENGTH + 1, (char *)p_input);

    int offset                                     = 0;
    char encryptedSecretKey[SECRET_KEY_LENGTH * 2] = {'\0'};
    BytesToHex(p_output, SECRET_KEY_LENGTH, encryptedSecretKey);
    strcpy(p_secretKey, encryptedSecretKey);

    char idBuffer[IV_SIZE * 2 + 1] = {'\0'};
    BytesToHex(ivBuffer, IV_SIZE, idBuffer);

    strcpy(p_id, idBuffer);

    vPortFree((void *)p_input);
}

/* Private functions ---------------------------------------------------------*/

/******************************************************************************
 * @brief Função para transformar um vetor de bytes em sua representação
 * em string hexadecimal.
 *
 * @param p_buffer Ponteiro para o buffer de bytes.
 *
 * @param bufferSize Tamanho do buffer em bytes.
 *
 * @param p_hexOutput Ponteiro para o buffer já convertido em caracteres hexade-
 * cimais.
 ******************************************************************************/
static void BytesToHex(const uint8_t *p_buffer,
                       size_t bufferSize,
                       char *p_hexOutput) {
    for (size_t i = 0; i < bufferSize; i++) {
        sprintf(p_hexOutput + (i * 2), "%02x", p_buffer[i]);
    }
    p_hexOutput[2 * bufferSize] = '\0';
}

/******************************************************************************
 * @brief Função para gerar um vetor de inicialização (IV) randômico para o
 * algoritmo de criptografia.
 *
 * @param p_iv Ponteiro para o vetor de inicialização (IV).
 ******************************************************************************/
static void GenerateRandomIV(uint8_t *p_iv) {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbgContext;

    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&drbgContext);

    mbedtls_ctr_drbg_seed(&drbgContext,
                          mbedtls_entropy_func,
                          &entropy,
                          NULL,
                          0);

    mbedtls_ctr_drbg_random(&drbgContext, p_iv, IV_SIZE);

    mbedtls_ctr_drbg_free(&drbgContext);
    mbedtls_entropy_free(&entropy);
}

/******************************************************************************
 * @brief Função para embaralhar as credenciais de acesso às APIs da plataforma.
 *
 * @param ps_apiCredentials Ponteiro para a estrutura de armazenamento das cre-
 * denciais de acesso à plataforma.
 *
 * @returns uint8_t* Ponteiro para a chave secreta embaralhada (Após o uso faça
 * a liberação do ponteiro, que foi alocado dinamicamente).
 ******************************************************************************/
static uint8_t *ScrambleSecretKey(ApiCredentialsTypeDef *ps_apiCredentials) {
    char *p_apiKey = ps_apiCredentials->cpapSecretKey;

    time_t t       = time(NULL);
    struct tm *now = localtime(&t);

    struct tm timeWithOffset;
    memcpy(&timeWithOffset, now, sizeof(*now));

    timeWithOffset.tm_hour -= 24;

    char timeMillis[11];
    snprintf(timeMillis, sizeof(timeMillis), "%lld", mktime(&timeWithOffset));

    char scrambledField[SECRET_KEY_SIZE];
    strcpy(scrambledField, p_apiKey);

    for (int i = 0; i < strlen(scrambledField); i++) {
        if (scrambledField[i] == '-')
            scrambledField[i] = 'T';
        else if (scrambledField[i] == '3')
            scrambledField[i] = 'B';
        else if (scrambledField[i] == 'e')
            scrambledField[i] = '3';
        else if (scrambledField[i] == 'a')
            scrambledField[i] = '!';
        else if (scrambledField[i] == '4')
            scrambledField[i] = 'a';
    }

    char *fieldArrayKey[5] = {0};
    char *token            = strtok(scrambledField, "T");
    int i                  = 0;
    while (token != NULL) {
        fieldArrayKey[i++] = token;
        token              = strtok(NULL, "T");
    }

    char *secretKey = (char *)pvPortMalloc(AES_KEY_SIZE * 8);
    if (i != 5) {
        strcpy(secretKey, p_apiKey);
    } else {
        snprintf(secretKey,
                 AES_KEY_SIZE * 8,
                 "%c%c%cS%c%c%cS%sT%sT%sT%sT%sS%c%c%c%cS%c%c%c",
                 timeMillis[3],
                 timeMillis[4],
                 timeMillis[5],
                 timeMillis[0],
                 timeMillis[1],
                 timeMillis[2],
                 fieldArrayKey[1],
                 fieldArrayKey[4],
                 fieldArrayKey[0],
                 fieldArrayKey[3],
                 fieldArrayKey[2],
                 timeMillis[6],
                 timeMillis[7],
                 timeMillis[8],
                 timeMillis[9],
                 timeMillis[6],
                 timeMillis[7],
                 timeMillis[8]);
    }

    return (uint8_t *)secretKey;
}

/******************************************************************************
 * @brief Função para aplicar o algoritmo de criptografia nas credenciais de
 * acesso à plataforma.
 *
 * @param p_iv Ponteiro para o vetor de inicialização (IV).
 *
 * @param p_output Ponteiro para o vetor contendo o resultado da criptografia.
 *
 * @param p_input Ponteiro para o vetor contendo os dados de entrada para o
 *algo- ritmo de criptografia.
 ******************************************************************************/
static void WorkBytes(uint8_t *p_iv,
                      uint8_t *p_output,
                      uint8_t *p_input,
                      size_t numBytes) {
    mbedtls_aes_context aes;
    uint8_t nonceCounter[16]        = {0};
    unsigned int nonceCounterOffset = 0;

    mbedtls_aes_init(&aes);
    uint8_t cryptoKey[AES_KEY_SIZE] = {0};

    for (int i = 0; i < AES_KEY_SIZE; i++) {
        cryptoKey[i] = ps_apiCredentials->hashCryptoKey[i];
    }
    mbedtls_aes_setkey_enc(&aes, cryptoKey, AES_KEY_SIZE * 8);

    int offset      = 0;
    uint8_t tmp[16] = {0};

    while (numBytes > 0) {
        mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, p_iv, tmp);
        for (int i = 15; i >= 0; i--) {
            if (p_iv[i] == 255) {
                p_iv[i] = 0;
            } else {
                p_iv[i] += 1;
                break;
            }
        }

        if (numBytes < 16) {
            for (int i = 0; i < numBytes; i++) {
                p_output[offset + i] = p_input[offset + i] ^ tmp[i];
            }

            mbedtls_aes_free(&aes);
            return;
        }

        for (int i = 0; i < 16; i++) {
            p_output[offset + i] = p_input[offset + i] ^ tmp[i];
        }

        numBytes -= 16;
        offset += 16;
    }
}

/*****************************END OF FILE**************************************/