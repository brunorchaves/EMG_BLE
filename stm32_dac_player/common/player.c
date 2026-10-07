#include "player.h"

#include <math.h>
#include <string.h>

#include "board.h"
#include "stimulus_table.h"

#define TWO_PI       6.28318530718f
#define SYNC_HIGH    PLAYER_DAC_MAX
#define CELL         PLAYER_SYNC_CELL_SAMPLES
#define SWEEP_STEP_SAMPLES (PLAYER_SWEEP_STEP_S * BOARD_DAC_FS_HZ)

/* Estado do gerador: so a interrupcao do DMA mexe aqui. */
static player_cfg_t active;
static uint32_t pos;          /* posicao no laco, 0..STIMULUS_LOOP_LEN-1 */
static uint32_t step;         /* degrau da varredura */
static uint32_t step_pos;     /* amostra dentro do degrau */
static float phase;           /* 0..1, senoide e varredura */
static float sweep_inc;       /* incremento de fase do degrau atual */

/* Trem de pulsos de sync: celulas de CELL amostras, alternando alto/baixo a
 * partir de alto. cells_total impar => comeca e termina em alto. */
static uint32_t sync_cells_total;
static uint32_t sync_cells_left;
static uint32_t sync_cell_pos;

/* Pedido pendente: escrito pelo laco principal, consumido na interrupcao. */
static volatile player_cfg_t pending;
static volatile bool pending_flag;
static volatile bool pending_restart;

static volatile player_events_t events;

static inline uint16_t clamp_code(int32_t v)
{
    if (v < 0) return 0;
    if (v > PLAYER_DAC_MAX) return PLAYER_DAC_MAX;
    return (uint16_t)v;
}

/* x em Q15 (-32768..32767) -> codigo do DAC, com arredondamento. */
static inline uint16_t q15_to_code(int32_t x)
{
    int32_t scaled = (int32_t)active.amp * x;
    scaled += (scaled >= 0) ? (1 << 14) : -(1 << 14);
    return clamp_code((int32_t)active.mid + scaled / 32768);
}

static inline uint16_t float_to_code(float x)
{
    return clamp_code((int32_t)active.mid + (int32_t)lrintf((float)active.amp * x));
}

/* Arma n pulsos de sync (n >= 1) e conta o evento. */
static void mark_event(uint32_t pulses)
{
    if (pulses < 1u) pulses = 1u;
    sync_cells_total = 2u * pulses - 1u;
    sync_cells_left = sync_cells_total;
    sync_cell_pos = 0;
    events.seq++;
}

/* Nivel do canal de sync para a amostra atual, consumindo o trem. */
static inline uint16_t sync_sample(void)
{
    if (!sync_cells_left) return 0;
    /* celulas pares (contadas do inicio) sao as altas */
    bool high = ((sync_cells_total - sync_cells_left) & 1u) == 0u;
    if (++sync_cell_pos >= CELL) {
        sync_cell_pos = 0;
        sync_cells_left--;
    }
    return high ? SYNC_HIGH : 0u;
}

float player_sweep_freq(uint32_t s)
{
    /* 1/3 de oitava de base 10 referida a 1 kHz (IEC 61260): 10,0 Hz exatos no
     * degrau 0, ..., 794,3 Hz no ultimo - os nominais 10 ... 800 Hz da ISO 266 */
    return 1000.0f * powf(10.0f, ((float)s - (float)PLAYER_SWEEP_STEPS) / 10.0f);
}

int player_segment_by_name(const char *s)
{
    for (uint32_t i = 0; i < STIMULUS_SEGMENT_COUNT; i++) {
        const char *n = stimulus_segment_names[i];
        uint32_t k = 0;
        while (s[k] && n[k]) {
            char a = s[k], b = n[k];
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) break;
            k++;
        }
        if (!s[k] && !n[k]) return (int)i;
    }
    return -1;
}

static void restart_mode(void)
{
    pos = 0;
    step = 0;
    step_pos = 0;
    phase = 0.0f;
    sweep_inc = player_sweep_freq(0) / (float)BOARD_DAC_FS_HZ;
    events.sweep_step = 0;
    events.segment = active.segment;
    if (active.mode == PLAYER_LOOP || active.mode == PLAYER_ONCE) {
        mark_event((uint32_t)active.segment + 1u);
    } else if (active.mode == PLAYER_SWEEP) {
        mark_event(1u);
    } else {
        sync_cells_left = 0;
    }
}

static void apply_pending(void)
{
    if (!pending_flag) return;
    player_mode_t old_mode = active.mode;
    uint8_t old_segment = active.segment;
    active.mode = pending.mode;
    active.mid = pending.mid;
    active.amp = pending.amp;
    active.code = pending.code;
    active.sine_hz = pending.sine_hz;
    active.segment = pending.segment;
    active.cycle = pending.cycle;
    if (active.segment >= STIMULUS_SEGMENT_COUNT) active.segment = 0;
    if (pending_restart || active.mode != old_mode || active.segment != old_segment) restart_mode();
    pending_flag = false;
}

/* Fim de um laco: conta, avanca o segmento se estiver ciclando, e rearma. */
static void loop_wrapped(void)
{
    pos = 0;
    events.loops++;
    if (active.mode == PLAYER_ONCE) {
        active.mode = PLAYER_SILENCE;
        sync_cells_left = 0;
        return;
    }
    if (active.cycle) {
        active.segment = (uint8_t)((active.segment + 1u) % STIMULUS_SEGMENT_COUNT);
    }
    events.segment = active.segment;
    mark_event((uint32_t)active.segment + 1u);
}

static inline uint16_t next_sample(void)
{
    switch (active.mode) {
    case PLAYER_LOOP:
    case PLAYER_ONCE: {
        uint16_t c;
        if (pos < STIMULUS_PREAMBLE_LEN) {
            c = q15_to_code(stimulus_preamble[pos]);
        } else if (pos < STIMULUS_PREAMBLE_LEN + STIMULUS_SEGMENT_LEN) {
            c = q15_to_code(stimulus_segments[active.segment][pos - STIMULUS_PREAMBLE_LEN]);
        } else {
            c = clamp_code(active.mid);   /* cauda de silencio */
        }
        if (++pos >= STIMULUS_LOOP_LEN) loop_wrapped();
        return c;
    }
    case PLAYER_SWEEP: {
        uint16_t c = float_to_code(sinf(TWO_PI * phase));
        phase += sweep_inc;
        if (phase >= 1.0f) phase -= 1.0f;
        if (++step_pos >= SWEEP_STEP_SAMPLES) {
            step_pos = 0;
            phase = 0.0f;
            if (++step >= PLAYER_SWEEP_STEPS) step = 0;
            sweep_inc = player_sweep_freq(step) / (float)BOARD_DAC_FS_HZ;
            events.sweep_step = step;
            mark_event(1u);
        }
        return c;
    }
    case PLAYER_SINE: {
        uint16_t c = float_to_code(sinf(TWO_PI * phase));
        phase += active.sine_hz / (float)BOARD_DAC_FS_HZ;
        if (phase >= 1.0f) phase -= 1.0f;
        return c;
    }
    case PLAYER_CODE:
        return clamp_code(active.code);
    case PLAYER_SILENCE:
    default:
        return clamp_code(active.mid);
    }
}

void player_fill(uint32_t *buf, uint32_t n)
{
    apply_pending();
    for (uint32_t i = 0; i < n; i++) {
        /* O sync e lido antes de gerar a amostra: o evento marcado ao emitir a
         * ultima amostra de um laco (ou degrau) acende o pulso exatamente na
         * amostra 0 do seguinte. */
        uint16_t sync = sync_sample();
        buf[i] = BOARD_DAC_WORD(next_sample(), sync);
    }
}

void player_init(void)
{
    active.mode = PLAYER_LOOP;
    active.mid = PLAYER_DEFAULT_MID;
    active.amp = PLAYER_DEFAULT_AMP;
    active.code = PLAYER_DEFAULT_MID;
    active.sine_hz = 100.0f;
    active.segment = 0;
    active.cycle = false;
    pending_flag = false;
    events.loops = 0;
    restart_mode();
}

void player_request(const player_cfg_t *cfg, bool restart)
{
    uint32_t s = board_irq_save();
    pending.mode = cfg->mode;
    pending.mid = cfg->mid;
    pending.amp = cfg->amp;
    pending.code = cfg->code;
    pending.sine_hz = cfg->sine_hz;
    pending.segment = cfg->segment;
    pending.cycle = cfg->cycle;
    pending_restart = restart;
    pending_flag = true;
    board_irq_restore(s);
}

player_cfg_t player_config(void)
{
    player_cfg_t c;
    uint32_t s = board_irq_save();
    if (pending_flag) {
        c.mode = pending.mode;
        c.mid = pending.mid;
        c.amp = pending.amp;
        c.code = pending.code;
        c.sine_hz = pending.sine_hz;
        c.segment = pending.segment;
        c.cycle = pending.cycle;
    } else {
        c = active;
    }
    board_irq_restore(s);
    return c;
}

player_events_t player_events(void)
{
    player_events_t e;
    uint32_t s = board_irq_save();
    e.loops = events.loops;
    e.sweep_step = events.sweep_step;
    e.segment = events.segment;
    e.seq = events.seq;
    board_irq_restore(s);
    return e;
}

const char *player_mode_name(player_mode_t mode)
{
    static const char *const names[PLAYER_MODE_COUNT] = {
        "loop", "once", "sweep", "sine", "silence", "code",
    };
    return (mode < PLAYER_MODE_COUNT) ? names[mode] : "?";
}
