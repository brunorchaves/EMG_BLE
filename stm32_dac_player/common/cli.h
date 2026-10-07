/*
 * Linha de comando pela porta serial virtual do ST-LINK (115200 8N1).
 * "help" lista os comandos.
 */
#ifndef CLI_H
#define CLI_H

#include <stdbool.h>
#include <stdint.h>

void cli_init(void);
/* Consome os caracteres disponiveis na UART; executa linhas completas. */
void cli_poll(void);

bool cli_events_enabled(void);

/* Saida formatada minima (sem printf de ponto flutuante). */
void cli_puts(const char *s);
void cli_putu(uint32_t v);
void cli_puthex(uint32_t v, unsigned digits);
void cli_putfix(float v, unsigned decimals);
void cli_nl(void);

/* Impressos pelo "info" e no boot. */
void cli_print_info(void);
void cli_print_status(void);

#endif /* CLI_H */
