/*
 * Gerador de estimulo: maquina de modos que preenche o buffer do DAC.
 *
 * Toda saida passa por player_fill(), chamada da interrupcao do DMA a cada
 * meia janela. A CLI e o botao nao mexem no estado do gerador diretamente:
 * eles postam um pedido, que player_fill() aplica no inicio da proxima
 * meia janela. Assim a troca de modo nunca corta uma amostra pela metade e
 * nao precisa de trava.
 */
#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PLAYER_LOOP = 0, /* tabela de estimulo em laco (modo de boot) */
    PLAYER_ONCE,     /* tabela uma vez, depois silencio */
    PLAYER_SWEEP,    /* senoide em degraus de 1/3 de oitava, 10-800 Hz, 2 s cada */
    PLAYER_SINE,     /* senoide fixa */
    PLAYER_SILENCE,  /* codigo de meia escala, parado */
    PLAYER_CODE,     /* codigo DC fixo */
    PLAYER_MODE_COUNT
} player_mode_t;

#define PLAYER_DAC_MAX       4095
#define PLAYER_DEFAULT_MID   2048
#define PLAYER_DEFAULT_AMP   1738   /* +-1,400 V com VREF = 3,3 V (TRES_ENSAIOS 2) */

#define PLAYER_SWEEP_STEPS   20     /* 1000*10^(n/10), n = -20..-1: 10,0 ... 794,3 Hz */
#define PLAYER_SWEEP_STEP_S  2u

typedef struct {
    player_mode_t mode;
    uint16_t mid;
    uint16_t amp;
    uint16_t code;      /* PLAYER_CODE */
    float    sine_hz;   /* PLAYER_SINE */
} player_cfg_t;

/* Eventos que a interrupcao deixa para o laco principal imprimir. */
typedef struct {
    uint32_t loops;       /* lacos completos tocados desde o boot */
    uint32_t sweep_step;  /* degrau atual da varredura */
    uint32_t seq;         /* incrementa a cada evento (inicio de laco / degrau) */
} player_events_t;

void player_init(void);
void player_fill(uint32_t *buf, uint32_t n);

/* Pede uma nova configuracao; vale a partir da proxima meia janela.
 * restart = true recomeca o modo do inicio (tabela na amostra 0, varredura no
 * 1o degrau) mesmo se o modo nao mudou; false preserva a posicao (troca de
 * amplitude no meio do laco). */
void player_request(const player_cfg_t *cfg, bool restart);
/* Ultima configuracao pedida (o que vai estar tocando em ate 32 ms). */
player_cfg_t player_config(void);
player_events_t player_events(void);

const char *player_mode_name(player_mode_t mode);
float player_sweep_freq(uint32_t step);

#endif /* PLAYER_H */
