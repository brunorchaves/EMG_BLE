/*
 * Interface entre o nucleo portavel (player, CLI) e cada placa Nucleo.
 *
 * Cada boards/<placa>/board.c implementa estas funcoes com acesso direto a
 * registrador. O DAC roda em modo dual: os dois canais saem da mesma palavra
 * de 32 bits do DMA, disparados pelo mesmo TRGO do TIM6. Por isso o pulso de
 * sincronismo cai na amostra exata do inicio do laco, e nao 0..32 ms depois
 * como seria com um GPIO acionado na interrupcao.
 *
 * O ESTIMULO sai no canal 2 = PA5, nao no canal 1. Na NUCLEO-H563ZI o PA4
 * (canal 1) esta amarrado ao VBUS_SENSE pela ponte de solda SB56 - o proprio
 * suporte de placa do Zephyr usa dac1_out2_pa5 por esse motivo. Sobrou para o
 * canal 1 / PA4 o pulso de sync, que e so gatilho de osciloscopio e aguenta a
 * carga do divisor (valido enquanto o USB de usuario ficar desconectado; a
 * bancada do ensaio ja exige isso). Abrir SB56 libera o PA4 por completo.
 */
#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stdint.h>

/* Taxa do DAC. Tem de bater com STIMULUS_FS da tabela gerada. */
#define BOARD_DAC_FS_HZ      8000u

/* Meia janela do pingue-pongue, em amostras. 256 amostras = 32 ms a 8 kS/s. */
#define BOARD_DAC_HALF_LEN   256u

/* Palavra do DMA = DHR12RD: bits 11:0 vao para o canal 1 (PA4, sync) e
 * bits 27:16 para o canal 2 (PA5, estimulo). */
#define BOARD_DAC_WORD(stim, sync) ((uint32_t)(sync) | ((uint32_t)(stim) << 16))

typedef enum {
    BOARD_LED_STATUS = 0, /* verde (LD1, PB0) */
    BOARD_LED_ERROR  = 1, /* vermelho (LD3) */
} board_led_t;

/* Chamado da interrupcao do DMA para preencher n palavras de buf. */
typedef void (*board_fill_cb_t)(uint32_t *buf, uint32_t n);

typedef struct {
    const char *name;      /* "NUCLEO-H563ZI" */
    uint32_t sysclk_hz;
    bool     hse_ok;       /* false = caiu no HSI (base de tempo +-1%, nao serve para ensaio) */
} board_info_t;

void board_init(void);
const board_info_t *board_info(void);

/* Pre-preenche o buffer inteiro com cb e liga TIM6 + DMA + DAC. */
void board_dac_start(board_fill_cb_t cb);
/* true se o DAC sinalizou underrun de DMA desde a ultima chamada. */
bool board_dac_underrun(void);

void board_led(board_led_t led, bool on);
bool board_button_raw(void);      /* nivel atual do botao azul (true = apertado) */
uint32_t board_millis(void);

void board_uart_putc(char c);
int  board_uart_getc(void);       /* -1 se nao ha caractere */

/* Secao critica curta (mascara interrupcoes). */
uint32_t board_irq_save(void);
void board_irq_restore(uint32_t state);

void board_wait_for_interrupt(void);

#endif /* BOARD_H */
