#include "cli.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "player.h"
#include "stimulus_table.h"

#define FW_VERSION   "1.0"
#define LINE_MAX     64
#define VREF_MV      3300.0f   /* VDDA/VREF+ da Nucleo */

static char line[LINE_MAX];
static uint32_t line_len;
static bool events_on;

extern uint32_t app_table_crc32;   /* main.c: CRC32 calculado no boot */

/* ---- saida ------------------------------------------------------------- */

void cli_puts(const char *s)
{
    while (*s) {
        if (*s == '\n') board_uart_putc('\r');
        board_uart_putc(*s++);
    }
}

void cli_nl(void) { cli_puts("\n"); }

void cli_putu(uint32_t v)
{
    char b[11];
    int i = 10;
    b[i] = '\0';
    do {
        b[--i] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    cli_puts(&b[i]);
}

void cli_puthex(uint32_t v, unsigned digits)
{
    static const char hx[] = "0123456789abcdef";
    char b[9];
    if (digits > 8) digits = 8;
    for (unsigned i = 0; i < digits; i++) b[digits - 1 - i] = hx[(v >> (4 * i)) & 0xF];
    b[digits] = '\0';
    cli_puts(b);
}

void cli_putfix(float v, unsigned decimals)
{
    uint32_t scale = 1;
    for (unsigned i = 0; i < decimals; i++) scale *= 10u;
    if (v < 0) {
        cli_puts("-");
        v = -v;
    }
    uint32_t q = (uint32_t)lrintf(v * (float)scale);
    cli_putu(q / scale);
    if (decimals) {
        cli_puts(".");
        uint32_t frac = q % scale;
        for (uint32_t d = scale / 10u; d; d /= 10u) {
            board_uart_putc((char)('0' + (frac / d) % 10u));
        }
    }
}

static void put_kv(const char *k, const char *v)
{
    cli_puts(k);
    cli_puts(v);
}

/* ---- relatorios -------------------------------------------------------- */

static float code_to_mv(float codes) { return codes * VREF_MV / 4096.0f; }

void cli_print_info(void)
{
    const board_info_t *bi = board_info();
    cli_puts("stm32_dac_player v" FW_VERSION " (build " __DATE__ " " __TIME__ ")\n");
    put_kv("board    ", bi->name);
    cli_puts("  SYSCLK ");
    cli_putu(bi->sysclk_hz / 1000000u);
    cli_puts(" MHz  clock ");
    cli_puts(bi->hse_ok ? "HSE (MCO 8 MHz do ST-LINK)\n"
                        : "HSI (!! HSE falhou: base de tempo +-1%, nao use para ensaio)\n");
    cli_puts("dac      PA5 = ESTIMULO (Zio D13), PA4 = sync, ");
    cli_putu(BOARD_DAC_FS_HZ);
    cli_puts(" S/s\n");
    cli_puts("         PA4 e VBUS_SENSE nesta placa (SB56) - so gatilho, nao meca nele\n");
    cli_puts("sync     (segmento+1) pulsos de 10 ms no inicio de cada laco\n");
    cli_puts("table    laco de ");
    cli_putfix((float)STIMULUS_LOOP_LEN / (float)STIMULUS_FS, 3);
    cli_puts(" s @ ");
    cli_putu(STIMULUS_FS);
    cli_puts(" S/s = preambulo ");
    cli_putu(STIMULUS_PREAMBLE_LEN);
    cli_puts(" + segmento ");
    cli_putu(STIMULUS_SEGMENT_LEN);
    cli_puts(" + cauda ");
    cli_putu(STIMULUS_TAIL_LEN);
    cli_puts("\n");
    cli_puts("flash    ");
    cli_putu(2u * (STIMULUS_PREAMBLE_LEN + STIMULUS_SEGMENT_COUNT * STIMULUS_SEGMENT_LEN) / 1024u);
    cli_puts(" kB de tabela em ");
    cli_putu(STIMULUS_SEGMENT_COUNT);
    cli_puts(" segmentos (seg lista)\n");
    put_kv("sha256   ", STIMULUS_SHA256 "\n");
    cli_puts("crc32    esperado ");
    cli_puthex(STIMULUS_CRC32, 8);
    cli_puts(" calculado ");
    cli_puthex(app_table_crc32, 8);
    cli_puts(app_table_crc32 == STIMULUS_CRC32 ? "  OK\n" : "  FALHOU (flash corrompida?)\n");
    if (STIMULUS_FS != BOARD_DAC_FS_HZ) cli_puts("!! STIMULUS_FS difere da taxa do DAC\n");
}

void cli_print_status(void)
{
    player_cfg_t c = player_config();
    player_events_t e = player_events();
    cli_puts("mode ");
    cli_puts(player_mode_name(c.mode));
    if (c.mode == PLAYER_LOOP || c.mode == PLAYER_ONCE) {
        cli_puts("  seg ");
        cli_putu(e.segment);
        cli_puts(" ");
        cli_puts(stimulus_segment_names[e.segment]);
        if (c.cycle) cli_puts(" (cycle)");
    }
    if (c.mode == PLAYER_SINE) {
        cli_puts(" ");
        cli_putfix(c.sine_hz, 2);
        cli_puts(" Hz");
    } else if (c.mode == PLAYER_CODE) {
        cli_puts(" ");
        cli_putu(c.code);
    } else if (c.mode == PLAYER_SWEEP) {
        cli_puts(" step ");
        cli_putu(e.sweep_step + 1u);
        cli_puts("/");
        cli_putu(PLAYER_SWEEP_STEPS);
        cli_puts(" = ");
        cli_putfix(player_sweep_freq(e.sweep_step), 2);
        cli_puts(" Hz");
    }
    cli_puts("  amp ");
    cli_putu(c.amp);
    cli_puts(" (+-");
    cli_putfix(code_to_mv(c.amp), 1);
    cli_puts(" mV, ");
    cli_putfix(20.0f * log10f((c.amp ? c.amp : 1) / 2047.0f), 1);
    cli_puts(" dBFS)  mid ");
    cli_putu(c.mid);
    cli_puts(" (");
    cli_putfix(code_to_mv(c.mid), 1);
    cli_puts(" mV)  loops ");
    cli_putu(e.loops);
    cli_puts("  uptime ");
    cli_putfix(board_millis() / 1000.0f, 1);
    cli_puts(" s\n");
}

static void print_help(void)
{
    cli_puts(
        "loop             toca o laco de estimulo (recomeca da amostra 0)\n"
        "once             toca o laco uma vez e para em silencio\n"
        "sweep            degraus de 1/3 oit., 10-800 Hz, 2 s cada (resposta em freq.)\n"
        "sine <Hz>        senoide fixa (ex.: sine 100)\n"
        "silence          meia escala, parado (piso de ruido)\n"
        "code <0-4095>    codigo DC fixo\n"
        "amp <n>          amplitude de pico em codigos (padrao 1738 = +-1,400 V)\n"
        "amp <n>mv        ... em mV de pico na saida do DAC (ex.: amp 1400mv)\n"
        "amp <n>db        ... em dBFS, 0 dB = 2047 codigos (ex.: amp -20db)\n"
        "mid <0-4095>     codigo do centro (padrao 2048)\n"
        "seg              lista os segmentos do dataset\n"
        "seg <n|nome>     escolhe o segmento (ex.: seg 4, seg Walking)\n"
        "seg all          cicla por todos, um por laco\n"
        "status           modo, segmento, amplitude, lacos tocados\n"
        "info             placa, relogio, tabela, SHA-256 e CRC\n"
        "events on|off    imprime cada inicio de laco / degrau\n");
}

/* Imprime s e completa com espacos ate width colunas. */
static void put_padded(const char *s, uint32_t width)
{
    uint32_t k = 0;
    for (; s[k]; k++) board_uart_putc(s[k]);
    for (; k < width; k++) board_uart_putc(' ');
}

static void print_segments(void)
{
    player_events_t e = player_events();
    player_cfg_t c = player_config();
    for (uint32_t i = 0; i < STIMULUS_SEGMENT_COUNT; i++) {
        cli_puts(i == e.segment ? " *" : "  ");
        cli_putu(i);
        cli_puts("  ");
        put_padded(stimulus_segment_names[i], 14);
        cli_puts(stimulus_segment_source[i]);
        cli_puts("\n");
    }
    cli_puts("(* = tocando agora");
    if (c.cycle) cli_puts(", ciclando por todos");
    cli_puts(")  sync: indice+1 pulsos de 10 ms\n");
}

/* ---- comandos ---------------------------------------------------------- */

static bool parse_uint(const char *s, uint32_t max, uint32_t *out)
{
    char *end;
    if (!s || !*s) return false;
    unsigned long v = strtoul(s, &end, 10);
    if (*end || v > max) return false;
    *out = (uint32_t)v;
    return true;
}

static bool parse_amp(const char *s, uint32_t *out)
{
    char *end;
    if (!s || !*s) return false;
    float v = strtof(s, &end);
    float codes;
    if (*end == '\0') {
        codes = v;
    } else if ((end[0] == 'm' || end[0] == 'M') && (end[1] == 'v' || end[1] == 'V') && !end[2]) {
        codes = v * 4096.0f / VREF_MV;
    } else if ((end[0] == 'd' || end[0] == 'D') && (end[1] == 'b' || end[1] == 'B') && !end[2]) {
        codes = 2047.0f * powf(10.0f, v / 20.0f);
    } else {
        return false;
    }
    if (!(codes >= 0.0f) || codes > 2047.5f) return false;
    *out = (uint32_t)lrintf(codes);
    return true;
}

static void err(const char *msg)
{
    cli_puts("erro: ");
    cli_puts(msg);
    cli_nl();
}

static void exec(char *s)
{
    char *argv[3] = {0};
    int argc = 0;
    for (char *tok = strtok(s, " \t"); tok && argc < 3; tok = strtok(NULL, " \t")) argv[argc++] = tok;
    if (argc == 0) return;

    player_cfg_t c = player_config();
    const char *cmd = argv[0];
    uint32_t v;

    if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) {
        print_help();
        return;
    } else if (!strcmp(cmd, "info")) {
        cli_print_info();
        return;
    } else if (!strcmp(cmd, "status")) {
        cli_print_status();
        return;
    } else if (!strcmp(cmd, "events")) {
        if (argc == 2 && !strcmp(argv[1], "on")) events_on = true;
        else if (argc == 2 && !strcmp(argv[1], "off")) events_on = false;
        else { err("use: events on|off"); return; }
        cli_puts(events_on ? "events on\n" : "events off\n");
        return;
    } else if (!strcmp(cmd, "seg")) {
        if (argc == 1) {
            print_segments();
            return;
        }
        if (!strcmp(argv[1], "all")) {
            c.cycle = true;
        } else {
            int found = player_segment_by_name(argv[1]);
            if (found < 0) {
                if (!parse_uint(argv[1], STIMULUS_SEGMENT_COUNT - 1u, &v)) {
                    err("use: seg | seg <n|nome> | seg all   (seg lista os nomes)");
                    return;
                }
                found = (int)v;
            }
            c.segment = (uint8_t)found;
            c.cycle = false;
        }
        /* escolher segmento so faz sentido tocando o laco */
        if (c.mode != PLAYER_LOOP && c.mode != PLAYER_ONCE) c.mode = PLAYER_LOOP;
        player_request(&c, true);
    } else if (!strcmp(cmd, "loop")) {
        c.mode = PLAYER_LOOP;
        player_request(&c, true);
    } else if (!strcmp(cmd, "once")) {
        c.mode = PLAYER_ONCE;
        player_request(&c, true);
    } else if (!strcmp(cmd, "sweep")) {
        c.mode = PLAYER_SWEEP;
        player_request(&c, true);
    } else if (!strcmp(cmd, "silence")) {
        c.mode = PLAYER_SILENCE;
        player_request(&c, true);
    } else if (!strcmp(cmd, "sine")) {
        char *end;
        float hz = argc == 2 ? strtof(argv[1], &end) : -1.0f;
        if (argc != 2 || *end || !(hz > 0.0f) || hz > BOARD_DAC_FS_HZ / 2.0f) { err("use: sine <Hz>, 0 < Hz <= 4000"); return; }
        bool restart = c.mode != PLAYER_SINE;
        c.mode = PLAYER_SINE;
        c.sine_hz = hz;
        player_request(&c, restart);
    } else if (!strcmp(cmd, "code")) {
        if (argc != 2 || !parse_uint(argv[1], PLAYER_DAC_MAX, &v)) { err("use: code <0-4095>"); return; }
        c.mode = PLAYER_CODE;
        c.code = (uint16_t)v;
        player_request(&c, true);
    } else if (!strcmp(cmd, "amp")) {
        if (argc != 2 || !parse_amp(argv[1], &v)) { err("use: amp <codigos> | <mV>mv | <dB>db  (pico <= 2047 codigos)"); return; }
        c.amp = (uint16_t)v;
        player_request(&c, false);
    } else if (!strcmp(cmd, "mid")) {
        if (argc != 2 || !parse_uint(argv[1], PLAYER_DAC_MAX, &v)) { err("use: mid <0-4095>"); return; }
        c.mid = (uint16_t)v;
        player_request(&c, false);
    } else {
        { err("comando desconhecido (help)"); return; }
    }

    if (c.mid < c.amp || c.mid + c.amp > PLAYER_DAC_MAX) cli_puts("aviso: mid +- amp sai de 0..4095, o pico vai ceifar\n");
    cli_print_status();
}

/* ---- entrada ----------------------------------------------------------- */

void cli_init(void)
{
    line_len = 0;
    events_on = false;
}

bool cli_events_enabled(void) { return events_on; }

void cli_poll(void)
{
    int ch;
    while ((ch = board_uart_getc()) >= 0) {
        if (ch == '\r' || ch == '\n') {
            if (ch == '\n' && line_len == 0) continue; /* CRLF */
            cli_nl();
            line[line_len] = '\0';
            exec(line);
            line_len = 0;
            cli_puts("> ");
        } else if (ch == 0x08 || ch == 0x7F) {
            if (line_len) {
                line_len--;
                cli_puts("\b \b");
            }
        } else if (ch >= 0x20 && ch < 0x7F && line_len < LINE_MAX - 1) {
            line[line_len++] = (char)ch;
            board_uart_putc((char)ch);
        }
    }
}
