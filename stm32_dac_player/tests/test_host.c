/*
 * Teste de host do nucleo portavel (player + CLI), com uma placa falsa.
 * Roda no PC: python stm32_dac_player/tests/run_host_tests.py
 *
 * Confere amostra a amostra o que o DMA mandaria para o DAC: o canal do
 * estimulo contra preambulo/segmento/cauda, o trem de pulsos do canal de
 * sync (indice+1 pulsos de 10 ms), a selecao e o ciclo de segmentos, a troca
 * de modo nas fronteiras de meia janela, once, varredura e o parser da CLI.
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

#define HALF   BOARD_DAC_HALF_LEN
#define CELL   PLAYER_SYNC_CELL_SAMPLES

/* Em qual metade da palavra cada canal cai depende de BOARD_STIM_ON_PA4
 * (ver common/board.h). check_word_layout() confere que isto acompanha o que
 * BOARD_DAC_WORD realmente monta, para a troca do pino nao passar batido. */
#if BOARD_STIM_ON_PA4
static uint16_t stim(uint32_t w) { return (uint16_t)(w & 0xFFFu); }
static uint16_t sync(uint32_t w) { return (uint16_t)((w >> 16) & 0xFFFu); }
#else
static uint16_t stim(uint32_t w) { return (uint16_t)((w >> 16) & 0xFFFu); }
static uint16_t sync(uint32_t w) { return (uint16_t)(w & 0xFFFu); }
#endif

static void check_word_layout(void)
{
    uint32_t w = BOARD_DAC_WORD(0xABCu, 0x123u);
    CHECK(stim(w) == 0xABCu && sync(w) == 0x123u,
          "stim()/sync() nao acompanham BOARD_DAC_WORD: estimulo %03x, sync %03x "
          "(esperado abc e 123) - revise o #if BOARD_STIM_ON_PA4 aqui e em board.h",
          stim(w), sync(w));
    printf("layout: estimulo no %s, sync no %s\n",
           BOARD_STIM_ON_PA4 ? "PA4 (canal 1)" : "PA5 (canal 2)",
           BOARD_STIM_ON_PA4 ? "PA5 (canal 2)" : "PA4 (canal 1)");
}

/* Gera n amostras em blocos de meia janela, como o DMA pediria. */
static void run(uint32_t *out, uint32_t n)
{
    for (uint32_t k = 0; k < n; k += HALF) player_fill(&out[k], HALF);
}

/* Referencia independente: mid + amp*q/32768, arredondado para longe de zero. */
static int expected_code(int mid, int amp, int q)
{
    double s = (double)amp * q / 32768.0;
    int r = mid + (int)(s >= 0 ? floor(s + 0.5) : -floor(-s + 0.5));
    if (r < 0) r = 0;
    if (r > 4095) r = 4095;
    return r;
}

/* O que o estimulo deve valer na posicao pos de um laco do segmento seg. */
static int expected_loop_code(uint32_t pos, uint32_t seg, int mid, int amp)
{
    if (pos < STIMULUS_PREAMBLE_LEN) return expected_code(mid, amp, stimulus_preamble[pos]);
    if (pos < STIMULUS_PREAMBLE_LEN + STIMULUS_SEGMENT_LEN)
        return expected_code(mid, amp, stimulus_segments[seg][pos - STIMULUS_PREAMBLE_LEN]);
    return mid;   /* cauda de silencio */
}

/* Nivel esperado do sync na posicao pos de um laco de n pulsos. */
static int expected_sync(uint32_t pos, uint32_t pulses)
{
    uint32_t cells = 2u * pulses - 1u;
    if (pos >= cells * CELL) return 0;
    return ((pos / CELL) & 1u) == 0u ? 4095 : 0;
}

static void cli_cmd(const char *cmd)
{
    uart_out_len = 0;
    uart_out[0] = 0;
    uart_in = cmd;
    cli_poll();
}

static void select_segment(uint32_t seg, bool cycle)
{
    player_cfg_t c = player_config();
    c.mode = PLAYER_LOOP;
    c.segment = (uint8_t)seg;
    c.cycle = cycle;
    player_request(&c, true);
}

/* ---- testes ------------------------------------------------------------ */

#define LOOPBUF ((2u * STIMULUS_LOOP_LEN / HALF + 2u) * HALF)
static uint32_t buf[LOOPBUF];

/* Dois lacos inteiros de um segmento, amostra a amostra, estimulo e sync. */
static void test_loop_segment(uint32_t seg)
{
    player_init();
    select_segment(seg, false);
    uint32_t n = (2u * STIMULUS_LOOP_LEN / HALF) * HALF;
    run(buf, n);
    int bad = 0;
    for (uint32_t i = 0; i < n && bad < 6; i++) {
        uint32_t pos = i % STIMULUS_LOOP_LEN;
        int want = expected_loop_code(pos, seg, PLAYER_DEFAULT_MID, PLAYER_DEFAULT_AMP);
        if (stim(buf[i]) != want) {
            CHECK(0, "seg %u amostra %u (pos %u): estimulo %u != %d", seg, i, pos, stim(buf[i]), want);
            bad++;
        }
        int ws = expected_sync(pos, seg + 1u);
        if (sync(buf[i]) != ws) {
            CHECK(0, "seg %u amostra %u (pos %u): sync %u != %d", seg, i, pos, sync(buf[i]), ws);
            bad++;
        }
    }
    /* n cai num multiplo de HALF, que nao bate com o fim do laco */
    uint32_t want_loops = n / STIMULUS_LOOP_LEN;
    CHECK(player_events().loops == want_loops, "seg %u: loops = %u, esperado %u",
          seg, player_events().loops, want_loops);
    CHECK(player_events().segment == seg, "seg %u: events.segment = %u", seg, player_events().segment);
}

static void test_all_segments(void)
{
    for (uint32_t s = 0; s < STIMULUS_SEGMENT_COUNT; s++) test_loop_segment(s);
    printf("segmentos: %u x 2 lacos conferidos amostra a amostra (estimulo e sync)\n",
           STIMULUS_SEGMENT_COUNT);
}

/* O trem de sync tem de ter exatamente indice+1 pulsos, dentro do silencio
 * inicial de 500 ms, antes do marcador de 100 Hz. */
static void test_sync_pulse_count(void)
{
    for (uint32_t s = 0; s < STIMULUS_SEGMENT_COUNT; s++) {
        player_init();
        select_segment(s, false);
        uint32_t n = (STIMULUS_PREAMBLE_LEN / HALF) * HALF;
        run(buf, n);
        uint32_t edges = 0, last = 0, span = 0;
        for (uint32_t i = 0; i < n; i++) {
            uint32_t v = sync(buf[i]) > 0;
            if (v && !last) edges++;
            if (v) span = i + 1;
            last = v;
        }
        CHECK(edges == s + 1u, "seg %u: %u pulsos de sync, esperado %u", s, edges, s + 1u);
        CHECK(span <= STIMULUS_MARKER_START, "seg %u: sync termina em %u, invade o marcador em %u",
              s, span, STIMULUS_MARKER_START);
    }
    printf("sync: 1..%u pulsos, todos terminando antes do marcador (amostra %u)\n",
           STIMULUS_SEGMENT_COUNT, STIMULUS_MARKER_START);
}

/* seg all: cada laco toca o segmento seguinte, e volta ao 0 depois do ultimo.
 * Confere amostra a amostra que o dado vem do segmento certo, e que o trem de
 * sync acompanha o indice. */
#define CYCLE_LOOPS 3u
static uint32_t cyc[(CYCLE_LOOPS * STIMULUS_LOOP_LEN / HALF) * HALF];

static void test_cycle(void)
{
    player_init();
    select_segment(0, true);
    uint32_t n = sizeof(cyc) / sizeof(cyc[0]);
    run(cyc, n);
    int bad = 0;
    for (uint32_t i = 0; i < n && bad < 6; i++) {
        uint32_t k = i / STIMULUS_LOOP_LEN;             /* qual laco */
        uint32_t pos = i % STIMULUS_LOOP_LEN;
        uint32_t seg = k % STIMULUS_SEGMENT_COUNT;      /* avanca um por laco */
        int want = expected_loop_code(pos, seg, PLAYER_DEFAULT_MID, PLAYER_DEFAULT_AMP);
        if (stim(cyc[i]) != want) {
            CHECK(0, "cycle laco %u pos %u: estimulo %u != %d (segmento %u)",
                  k, pos, stim(cyc[i]), want, seg);
            bad++;
        }
        int ws = expected_sync(pos, seg + 1u);
        if (sync(cyc[i]) != ws) {
            CHECK(0, "cycle laco %u pos %u: sync %u != %d (segmento %u)",
                  k, pos, sync(cyc[i]), ws, seg);
            bad++;
        }
    }
    /* e volta ao 0 depois do ultimo: toca ate o fim da volta completa */
    player_init();
    select_segment(STIMULUS_SEGMENT_COUNT - 1u, true);
    for (uint32_t i = 0; i < STIMULUS_LOOP_LEN + HALF; i += HALF) player_fill(buf, HALF);
    CHECK(player_events().segment == 0, "depois do ultimo segmento deveria voltar ao 0, veio %u",
          player_events().segment);
    printf("cycle: %u lacos conferidos amostra a amostra, segmento avancando e voltando ao 0\n",
           CYCLE_LOOPS);
}

static void test_amp_keeps_position(void)
{
    player_init();
    run(buf, 4 * HALF);
    player_cfg_t c = player_config();
    c.amp = 1000;
    player_request(&c, false);
    run(buf, HALF);
    int want = expected_loop_code(4 * HALF, 0, PLAYER_DEFAULT_MID, 1000);
    CHECK(stim(buf[0]) == want, "amp sem restart: %u != %d (posicao perdida?)", stim(buf[0]), want);
    CHECK(sync(buf[0]) == 0, "amp sem restart nao deveria rearmar o sync");
    printf("amp: troca no meio do laco preserva a posicao\n");
}

static void test_once(void)
{
    player_init();
    run(buf, 2 * HALF);
    player_cfg_t c = player_config();
    c.mode = PLAYER_ONCE;
    c.segment = 2;
    player_request(&c, true);
    uint32_t n = ((STIMULUS_LOOP_LEN + 2000u) / HALF) * HALF;
    run(buf, n);
    CHECK(sync(buf[0]) == 4095, "once deveria comecar com sync");
    CHECK(stim(buf[0]) == expected_loop_code(0, 2, PLAYER_DEFAULT_MID, PLAYER_DEFAULT_AMP),
          "once comeca na amostra 0 do preambulo");
    int after = 0, sync_after = 0;
    for (uint32_t i = STIMULUS_LOOP_LEN; i < n; i++) {
        after += stim(buf[i]) != PLAYER_DEFAULT_MID;
        sync_after += sync(buf[i]) != 0;
    }
    CHECK(after == 0 && sync_after == 0, "depois do once: %d fora do silencio, %d com sync",
          after, sync_after);
    CHECK(player_config().mode == PLAYER_SILENCE, "once termina em silence");
    printf("once: toca %u amostras e para em meia escala\n", STIMULUS_LOOP_LEN);
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
        CHECK(sync(sw[s * step_n]) == 4095, "sync no inicio do degrau %u", s);
        CHECK(s == 0 || sync(sw[s * step_n - 1]) == 0, "sync so comeca no degrau %u", s);
        /* a varredura usa 1 pulso so, nao o trem dos segmentos */
        uint32_t edges = 0, last = 0;
        for (uint32_t i = s * step_n; i < (s + 1) * step_n && i < n; i++) {
            uint32_t v = sync(sw[i]) > 0;
            if (v && !last) edges++;
            last = v;
        }
        CHECK(edges == 1, "degrau %u: %u pulsos de sync, esperado 1", s, edges);
        /* frequencia pelos cruzamentos ascendentes do meio de escala */
        uint32_t cross = 0;
        for (uint32_t i = s * step_n + 1; i < (s + 1) * step_n && i < n; i++)
            cross += stim(sw[i - 1]) < PLAYER_DEFAULT_MID && stim(sw[i]) >= PLAYER_DEFAULT_MID;
        float f_meas = cross / (float)PLAYER_SWEEP_STEP_S;
        float f_want = player_sweep_freq(s);
        CHECK(fabsf(f_meas - f_want) <= 1.0f, "degrau %u: %.2f Hz medido, %.2f Hz esperado",
              s, f_meas, f_want);
    }
    CHECK(fabsf(player_sweep_freq(0) - 10.0f) < 0.001f && fabsf(player_sweep_freq(19) - 794.33f) < 0.01f,
          "serie de 1/3 de oitava: %.2f .. %.2f", player_sweep_freq(0), player_sweep_freq(19));
    printf("sweep: 12 degraus de 2 s, 1 pulso de sync cada, frequencia conferida (%.2f .. %.1f Hz)\n",
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
        if (stim(buf[i]) < lo) lo = stim(buf[i]);
        if (stim(buf[i]) > hi) hi = stim(buf[i]);
    }
    CHECK(hi == 2048 + 1738 && lo == 2048 - 1738, "sine 100: pico %d..%d, esperado 310..3786", lo, hi);

    c.mode = PLAYER_CODE;
    c.code = 123;
    player_request(&c, true);
    run(buf, HALF);
    CHECK(stim(buf[0]) == 123 && stim(buf[HALF - 1]) == 123, "code 123");
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
    cli_cmd("amp 3000\r");
    CHECK(strstr(uart_out, "erro") != NULL, "amp 3000 deveria ser recusado");
    cli_cmd("sine 37.5\r");
    CHECK(player_config().mode == PLAYER_SINE && fabsf(player_config().sine_hz - 37.5f) < 1e-4f, "sine 37.5");
    cli_cmd("code 5000\r");
    CHECK(strstr(uart_out, "erro") != NULL, "code 5000 deveria ser recusado");

    /* seg por nome, por indice, limites, e all */
    cli_cmd("seg Walking\r");
    int want = player_segment_by_name("Walking");
    CHECK(want >= 0, "player_segment_by_name(Walking)");
    CHECK(player_config().segment == want && player_config().mode == PLAYER_LOOP && !player_config().cycle,
          "seg Walking -> %u (esperado %d), mode %s", player_config().segment, want,
          player_mode_name(player_config().mode));
    cli_cmd("seg walking\r");
    CHECK(player_config().segment == want, "seg sem diferenciar maiuscula");
    cli_cmd("seg 1\r");
    CHECK(player_config().segment == 1, "seg 1 -> %u", player_config().segment);
    cli_cmd("seg 99\r");
    CHECK(player_config().segment == 1 && strstr(uart_out, "erro"), "seg 99 deveria ser recusado");
    cli_cmd("seg Nope\r");
    CHECK(player_config().segment == 1 && strstr(uart_out, "erro"), "seg com nome invalido");
    cli_cmd("seg all\r");
    CHECK(player_config().cycle, "seg all liga o ciclo");
    cli_cmd("seg 3\r");
    CHECK(player_config().segment == 3 && !player_config().cycle, "seg <n> desliga o ciclo");
    cli_cmd("seg\r");
    CHECK(strstr(uart_out, stimulus_segment_names[0]) && strstr(uart_out, stimulus_segment_names[STIMULUS_SEGMENT_COUNT - 1]),
          "seg sem argumento lista todos os nomes");

    cli_cmd("info\r");
    CHECK(strstr(uart_out, STIMULUS_SHA256) && strstr(uart_out, "OK"), "info mostra SHA e CRC ok");
    /* O status tem de refletir o segmento JA no mesmo comando: ele vinha de
     * player_events(), que a interrupcao so atualiza na proxima meia janela,
     * e por isso aparecia um comando atrasado. */
    for (uint32_t i = 0; i < STIMULUS_SEGMENT_COUNT; i++) {
        char cmd[24];
        snprintf(cmd, sizeof(cmd), "seg %u\r", i);
        cli_cmd(cmd);
        CHECK(strstr(uart_out, stimulus_segment_names[i]) != NULL,
              "seg %u: a resposta nao cita %s (status atrasado?): %s",
              i, stimulus_segment_names[i], uart_out);
        cli_cmd("status\r");
        CHECK(strstr(uart_out, stimulus_segment_names[i]) != NULL,
              "status depois de seg %u nao cita %s", i, stimulus_segment_names[i]);
    }
    cli_cmd("status\r");
    CHECK(strstr(uart_out, "seg ") != NULL, "status mostra o segmento");
    cli_cmd("xyz\r");
    CHECK(strstr(uart_out, "desconhecido") != NULL, "comando desconhecido");
    printf("cli: amp, sine, limites, seg por nome/indice/all, info, status\n");
}

int main(void)
{
    printf("tabela: %u segmentos, preambulo %u, segmento %u, laco %u amostras\n",
           STIMULUS_SEGMENT_COUNT, STIMULUS_PREAMBLE_LEN, STIMULUS_SEGMENT_LEN, STIMULUS_LOOP_LEN);
    check_word_layout();
    if (failures) {   /* sem o layout certo, todo o resto falha em cascata */
        printf("\nlayout da palavra do DMA errado - corrija antes de ler o resto\n");
        return 1;
    }
    printf("\n");
    test_all_segments();
    test_sync_pulse_count();
    test_cycle();
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
