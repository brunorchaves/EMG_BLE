/*
 * Player de estimulo sEMG por DAC (ensaios de qualidade de sinal, artigo 2).
 *
 * Liga e ja toca o laco de estimulo (TRES_ENSAIOS_SINAL 3): no laboratorio
 * nao ha terminal aberto. A CLI pela serial do ST-LINK e o botao azul trocam
 * de modo. Ver stm32_dac_player/README.md.
 */
#include <stddef.h>

#include "board.h"
#include "cli.h"
#include "player.h"
#include "stimulus_table.h"

#define DEBOUNCE_MS 30u

uint32_t app_table_crc32;

/* CRC-32 IEEE (o mesmo do zlib.crc32 do make_stimulus.py), sobre os bytes da tabela. */
static uint32_t crc32_bytes(const uint8_t *p, size_t n)
{
    uint32_t crc = 0xFFFFFFFFu;
    while (n--) {
        crc ^= *p++;
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

/* Botao: loop -> sweep -> sine 100 Hz -> silence -> loop */
static void button_next_mode(void)
{
    player_cfg_t c = player_config();
    switch (c.mode) {
    case PLAYER_LOOP:  c.mode = PLAYER_SWEEP; break;
    case PLAYER_SWEEP: c.mode = PLAYER_SINE; c.sine_hz = 100.0f; break;
    case PLAYER_SINE:  c.mode = PLAYER_SILENCE; break;
    default:           c.mode = PLAYER_LOOP; break;
    }
    player_request(&c, true);
    cli_puts("\n[botao] ");
    cli_print_status();
    cli_puts("> ");
}

static void poll_button(void)
{
    static bool stable, last_raw;
    static uint32_t t_change;
    bool raw = board_button_raw();
    uint32_t now = board_millis();
    if (raw != last_raw) {
        last_raw = raw;
        t_change = now;
    } else if (raw != stable && (now - t_change) >= DEBOUNCE_MS) {
        stable = raw;
        if (stable) button_next_mode();
    }
}

/* LED verde: aceso = loop, pisca 4 Hz = once tocando, 2 Hz = sweep,
 * 1 Hz = sine, apagado = silence/code. */
static void update_status_led(void)
{
    uint32_t t = board_millis();
    bool on;
    switch (player_config().mode) {
    case PLAYER_LOOP:  on = true; break;
    case PLAYER_ONCE:  on = (t / 125u) & 1u; break;
    case PLAYER_SWEEP: on = (t / 250u) & 1u; break;
    case PLAYER_SINE:  on = (t / 500u) & 1u; break;
    default:           on = false; break;
    }
    board_led(BOARD_LED_STATUS, on);
}

static void poll_events(void)
{
    static uint32_t last_seq;
    static player_mode_t last_mode = PLAYER_LOOP;
    player_events_t e = player_events();
    player_mode_t mode = player_config().mode;

    if (cli_events_enabled() && e.seq != last_seq) {
        cli_puts("\nevt t=");
        cli_putu(board_millis());
        cli_puts(" ms ");
        if (mode == PLAYER_SWEEP) {
            cli_puts("sweep step ");
            cli_putu(e.sweep_step + 1u);
            cli_puts(" ");
            cli_putfix(player_sweep_freq(e.sweep_step), 2);
            cli_puts(" Hz\n> ");
        } else {
            cli_puts("loop start, completos=");
            cli_putu(e.loops);
            cli_puts("\n> ");
        }
    }
    last_seq = e.seq;

    if (last_mode == PLAYER_ONCE && mode == PLAYER_SILENCE) cli_puts("\nonce: fim, em silencio\n> ");
    last_mode = mode;
}

int main(void)
{
    board_init();
    app_table_crc32 = crc32_bytes((const uint8_t *)stimulus_table, sizeof(int16_t) * STIMULUS_LEN);

    player_init();
    cli_init();
    board_dac_start(player_fill);

    bool fault = !board_info()->hse_ok || app_table_crc32 != STIMULUS_CRC32;
    board_led(BOARD_LED_ERROR, fault);

    cli_puts("\n\n");
    cli_print_info();
    cli_print_status();
    cli_puts("help lista os comandos\n> ");

    for (;;) {
        cli_poll();
        poll_button();
        poll_events();
        update_status_led();
        if (board_dac_underrun()) {
            board_led(BOARD_LED_ERROR, true);
            cli_puts("\n!! underrun de DMA no DAC\n> ");
        }
        board_wait_for_interrupt();
    }
}
