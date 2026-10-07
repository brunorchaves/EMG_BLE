#include "player.h"

#include <math.h>

#include "board.h"
#include "stimulus_table.h"

#define TWO_PI       6.28318530718f
#define SYNC_HIGH    PLAYER_DAC_MAX
#define SYNC_LEN     (BOARD_DAC_FS_HZ / 100u)   /* pulso de 10 ms no canal 2 */
#define SWEEP_STEP_SAMPLES (PLAYER_SWEEP_STEP_S * BOARD_DAC_FS_HZ)

/* Estado do gerador: so a interrupcao do DMA mexe aqui. */
static player_cfg_t active;
static uint32_t idx;          /* posicao na tabela (LOOP/ONCE) */
static uint32_t step;         /* degrau da varredura */
static uint32_t step_pos;     /* amostra dentro do degrau */
static float phase;           /* 0..1, senoide e varredura */
static float sweep_inc;       /* incremento de fase do degrau atual */
static uint32_t sync_left;    /* amostras restantes do pulso de sync */

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

static void mark_event(void)
{
    sync_left = SYNC_LEN;
    events.seq++;
}

float player_sweep_freq(uint32_t s)
{
    /* 1/3 de oitava de base 10 referida a 1 kHz (IEC 61260): 10,0 Hz exatos no
     * degrau 0, ..., 794,3 Hz no ultimo - os nominais 10 ... 800 Hz da ISO 266 */
    return 1000.0f * powf(10.0f, ((float)s - (float)PLAYER_SWEEP_STEPS) / 10.0f);
}

static void restart_mode(void)
{
    idx = 0;
    step = 0;
    step_pos = 0;
    phase = 0.0f;
    sweep_inc = player_sweep_freq(0) / (float)BOARD_DAC_FS_HZ;
    if (active.mode == PLAYER_LOOP || active.mode == PLAYER_ONCE || active.mode == PLAYER_SWEEP) {
        mark_event();
    } else {
        sync_left = 0;
    }
    events.sweep_step = 0;
}

static void apply_pending(void)
{
    if (!pending_flag) return;
    player_mode_t old_mode = active.mode;
    active.mode = pending.mode;
    active.mid = pending.mid;
    active.amp = pending.amp;
    active.code = pending.code;
    active.sine_hz = pending.sine_hz;
    if (pending_restart || active.mode != old_mode) restart_mode();
    pending_flag = false;
}

static inline uint16_t next_sample(void)
{
    switch (active.mode) {
    case PLAYER_LOOP:
    case PLAYER_ONCE: {
        uint16_t c = q15_to_code(stimulus_table[idx]);
        if (++idx >= STIMULUS_LEN) {
            idx = 0;
            events.loops++;
            if (active.mode == PLAYER_ONCE) {
                active.mode = PLAYER_SILENCE;
            } else {
                mark_event();
            }
        }
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
            mark_event();
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
        uint16_t sync = 0;
        if (sync_left) {
            sync = SYNC_HIGH;
            sync_left--;
        }
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
    pending_flag = false;
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
