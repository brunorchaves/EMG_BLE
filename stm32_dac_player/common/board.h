/*
 * Interface entre o nucleo portavel (player, CLI) e cada placa Nucleo.
 *
 * Cada boards/<placa>/board.c implementa estas funcoes com acesso direto a
 * registrador. O DAC roda em modo dual: canal 1 (PA4) e o estimulo, canal 2
 * (PA5) e o sincronismo, e os dois saem da mesma palavra de 32 bits do DMA,
 * disparados pelo mesmo TRGO do TIM6. Por isso o pulso de sincronismo cai na
 * amostra exata do inicio do laco, e nao 0..32 ms depois como seria com um
 * GPIO acionado na interrupcao.
 */
#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stdint.h>

/* Taxa do DAC. Tem de bater com STIMULUS_FS da tabela gerada. */
#define BOARD_DAC_FS_HZ      8000u

/* Meia janela do pingue-pongue, em amostras. 256 amostras = 32 ms a 8 kS/s. */
#define BOARD_DAC_HALF_LEN   256u

/* Palavra do DMA: bits 11:0 = canal 1 (estimulo), bits 27:16 = canal 2 (sync) */
#define BOARD_DAC_WORD(ch1, ch2) ((uint32_t)(ch1) | ((uint32_t)(ch2) << 16))

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
