/*
 * Teste de host do nucleo portavel (player + CLI), com uma placa falsa.
 * Roda no PC: python stm32_dac_player/tests/run_host_tests.py
 *
 * Confere amostra a amostra o que o DMA mandaria para o DAC: codigo do
 * canal 1 contra a tabela, posicao do pulso de sync no canal 2, troca de
 * modo nas fronteiras de meia janela, once, varredura e o parser da CLI.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "cli.h"
#include "player.h"
#include "stimulus_table.h"

/* ---- placa falsa ------------------------------------------------------- */

static const char *uart_in;
static char uart_out[8192];
static size_t uart_out_len;

uint32_t app_table_crc32 = STIMULUS_CRC32;
static board_info_t fake_info = {"HOST", 250000000u, true};

const board_info_t *board_info(void) { return &fake_info; }
uint32_t board_millis(void) { return 0; }
void board_uart_putc(char c) { if (uart_out_len < sizeof(uart_out) - 1) uart_out[uart_out_len++] = c; uart_out[uart_out_len] = 0; }
int board_uart_getc(void) { return (uart_in && *uart_in) ? (unsigned char)*uart_in++ : -1; }
uint32_t board_irq_save(void) { return 0; }
void board_irq_restore(uint32_t s) { (void)s; }

/* ---- utilidades -------------------------------------------------------- */

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define HALF BOARD_DAC_HALF_LEN
#define SYNC_LEN (BOARD_DAC_FS_HZ / 100u)

/* Gera n amostras em blocos de meia janela, como o DMA pediria. */
static void run(uint32_t *out, uint32_t n)
{
    for (uint32_t k = 0; k < n; k += HALF) player_fill(&out[k], HALF);
}

static uint16_t ch1(uint32_t w) { return (uint16_t)(w & 0xFFFu); }
static uint16_t ch2(uint32_t w) { return (uint16_t)(w >> 16); }

/* Referencia independente: mid + amp*q/32768, arredondado para longe de zero. */
static int expected_code(int mid, int amp, int q)
{
    double s = (double)amp * q / 32768.0;
    int r = mid + (int)(s >= 0 ? floor(s + 0.5) : -floor(-s + 0.5));
    if (r < 0) r = 0;
    if (r > 4095) r = 4095;
    return r;
}

static void cli_cmd(const char *cmd)
{
    uart_out_len = 0;
    uart_out[0] = 0;
    uart_in = cmd;
    cli_poll();
}

/* ---- testes ------------------------------------------------------------ */

static uint32_t buf[2 * STIMULUS_LEN + 4 * HALF];

static void test_loop(void)
{
    player_init();
    uint32_t n = ((2 * STIMULUS_LEN) / HALF + 1) * HALF;
    run(buf, n);
    int bad = 0;
    for (uint32_t i = 0; i < n; i++) {
        int want = expected_code(PLAYER_DEFAULT_MID, PLAYER_DEFAULT_AMP, stimulus_table[i % STIMULUS_LEN]);
        if (ch1(buf[i]) != want && bad++ < 3) CHECK(0, "loop amostra %u: %u != %d", i, ch1(buf[i]), want);
        uint32_t pos = i % STIMULUS_LEN;
        int sync_want = pos < SYNC_LEN ? 4095 : 0;
        if (ch2(buf[i]) != sync_want && bad++ < 6) CHECK(0, "loop sync amostra %u: %u != %d", i, ch2(buf[i]), sync_want);
    }
    CHECK(player_events().loops == 2, "loops = %u, esperado 2", player_events().loops);
    printf("loop: %u amostras conferidas contra a tabela, sync em 0 e em %u\n", n, STIMULUS_LEN);
}

static void test_amp_keeps_position(void)
{
    player_init();
    run(buf, 4 * HALF);
    player_cfg_t c = player_config();
    c.amp = 1000;
    player_request(&c, false);
    run(buf, HALF);
    int want = expected_code(PLAYER_DEFAULT_MID, 1000, stimulus_table[4 * HALF]);
    CHECK(ch1(buf[0]) == want, "amp sem restart: %u != %d (posicao perdida?)", ch1(buf[0]), want);
    CHECK(ch2(buf[0]) == 0, "amp sem restart nao deveria gerar sync");
    printf("amp: troca no meio do laco preserva a posicao\n");
}

static void test_once(void)
{
    player_init();
    run(buf, 2 * HALF);
    player_cfg_t c = player_config();
    c.mode = PLAYER_ONCE;
    player_request(&c, true);
    uint32_t n = ((STIMULUS_LEN + 2000) / HALF) * HALF;
    run(buf, n);
    CHECK(ch2(buf[0]) == 4095, "once deveria comecar com sync");
    CHECK(ch1(buf[0]) == expected_code(PLAYER_DEFAULT_MID, PLAYER_DEFAULT_AMP, stimulus_table[0]), "once comeca na amostra 0");
    int after = 0, sync_after = 0;
    for (uint32_t i = STIMULUS_LEN; i < n; i++) {
        after += ch1(buf[i]) != PLAYER_DEFAULT_MID;
        sync_after += ch2(buf[i]) != 0;
    }
    CHECK(after == 0 && sync_after == 0, "depois do once: %d amostras fora do silencio, %d com sync", after, sync_after);
    CHECK(player_config().mode == PLAYER_SILENCE, "once termina em silence");
    printf("once: toca %u amostras e para em meia escala\n", STIMULUS_LEN);
}

static void test_sweep(void)
{
    player_init();
    player_cfg_t c = player_config();
    c.mode = PLAYER_SWEEP;
    player_request(&c, true);
    const uint32_t step_n = PLAYER_SWEEP_STEP_S * BOARD_DAC_FS_HZ;
    static uint32_t sw[12 * 16000 + HALF];
    uint32_t n = ((12 * step_n) / HALF) * HALF;
    run(sw, n);
    for (uint32_t s = 0; s < 12; s++) {
        CHECK(ch2(sw[s * step_n]) == 4095, "sync no inicio do degrau %u", s);
        CHECK(s == 0 || ch2(sw[s * step_n - 1]) == 0, "sync so comeca no degrau %u", s);
        /* frequencia pelos cruzamentos ascendentes do meio de escala */
        int cross = 0;
        for (uint32_t i = s * step_n + 1; i < (s + 1) * step_n && i < n; i++)
            cross += ch1(sw[i - 1]) < PLAYER_DEFAULT_MID && ch1(sw[i]) >= PLAYER_DEFAULT_MID;
        float f_meas = cross / (float)PLAYER_SWEEP_STEP_S;
        float f_want = player_sweep_freq(s);
        CHECK(fabsf(f_meas - f_want) <= 1.0f, "degrau %u: %.2f Hz medido, %.2f Hz esperado", s, f_meas, f_want);
    }
    CHECK(fabsf(player_sweep_freq(0) - 10.0f) < 0.001f && fabsf(player_sweep_freq(19) - 794.33f) < 0.01f,
          "serie de 1/3 de oitava: %.2f .. %.2f", player_sweep_freq(0), player_sweep_freq(19));
    printf("sweep: 12 degraus de 2 s com sync e frequencia conferidos (%.2f .. %.1f Hz na serie)\n",
           player_sweep_freq(0), player_sweep_freq(PLAYER_SWEEP_STEPS - 1));
}

static void test_sine_and_code(void)
{
    player_init();
    player_cfg_t c = player_config();
    c.mode = PLAYER_SINE;
    c.sine_hz = 100.0f;
    player_request(&c, true);
    run(buf, 8000);
    int lo = 4095, hi = 0;
    for (int i = 0; i < 8000; i++) {
        if (ch1(buf[i]) < lo) lo = ch1(buf[i]);
        if (ch1(buf[i]) > hi) hi = ch1(buf[i]);
    }
    CHECK(hi == 2048 + 1738 && lo == 2048 - 1738, "sine 100: pico %d..%d, esperado 310..3786", lo, hi);

    c.mode = PLAYER_CODE;
    c.code = 123;
    player_request(&c, true);
    run(buf, HALF);
    CHECK(ch1(buf[0]) == 123 && ch1(buf[HALF - 1]) == 123, "code 123");
    printf("sine/code: +-1738 codigos em torno de 2048, codigo DC ok\n");
}

static void test_cli(void)
{
    player_init();
    cli_init();
    cli_cmd("amp 1400mv\r");
    CHECK(player_config().amp == 1738, "amp 1400mv -> %u, esperado 1738", player_config().amp);
    cli_cmd("amp -6db\r");
    CHECK(player_config().amp == 1026, "amp -6db -> %u, esperado 1026", player_config().amp);
    cli_cmd("amp 500\r");
    CHECK(player_config().amp == 500, "amp 500");
    cli_cmd("amp 3000\r");
    CHECK(player_config().amp == 500 && strstr(uart_out, "erro"), "amp 3000 deveria ser recusado");
    cli_cmd("sine 37.5\r");
    CHECK(player_config().mode == PLAYER_SINE && fabsf(player_config().sine_hz - 37.5f) < 1e-4f, "sine 37.5");
    cli_cmd("code 5000\r");
    CHECK(player_config().mode == PLAYER_SINE && strstr(uart_out, "erro"), "code 5000 deveria ser recusado");
    cli_cmd("mid 2000\rsweep\r");
    CHECK(player_config().mode == PLAYER_SWEEP && player_config().mid == 2000, "mid + sweep na mesma rajada");
    cli_cmd("xyz\r");
    CHECK(strstr(uart_out, "desconhecido") != NULL, "comando desconhecido");
    cli_cmd("info\r");
    CHECK(strstr(uart_out, STIMULUS_SHA256) && strstr(uart_out, "OK"), "info mostra SHA e CRC ok");
    printf("cli: amp em codigos/mV/dB, limites, sine, mid, info\n");
}

int main(void)
{
    test_loop();
    test_amp_keeps_position();
    test_once();
    test_sweep();
    test_sine_and_code();
    test_cli();
    if (failures) {
        printf("\n%d falha(s)\n", failures);
        return 1;
    }
    printf("\ntodos os testes passaram\n");
    return 0;
}
