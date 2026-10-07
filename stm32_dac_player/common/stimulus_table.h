/* GERADO por tools/make_stimulus.py - nao editar. Regerar invalida a comparacao entre sessoes. */
#ifndef STIMULUS_TABLE_H
#define STIMULUS_TABLE_H

#include <stdint.h>

#define STIMULUS_FS             8000u
#define STIMULUS_SHA256         "e9625fae6b306603f6e4c4097cab93c2ae003a7e6422762006ea6d9783d84094"
#define STIMULUS_CRC32          0x16ed33e5u

/* Um laco = preambulo + um segmento + zona morta de saida = 7.5 s exatos:
 *
 *   0,000 - 0,200 s  silencio (trem de pulsos de sync)
 *   0,200 - 0,230 s  marcador, 3 ciclos de 100 Hz
 *   0,230 - 2,000 s  zona morta, 0 V
 *   2,000 - 6,000 s  A AMOSTRA do dataset
 *   6,000 - 7,500 s  zona morta, 0 V -> repete
 *
 * A zona morta de saida o firmware gera na hora (e so o codigo de meia escala),
 * entao nao custa flash. */
#define STIMULUS_PREAMBLE_LEN   16000u   /* 2.0 s */
#define STIMULUS_SEGMENT_LEN    32000u   /* 4.0 s */
#define STIMULUS_TAIL_LEN       12000u   /* 1.5 s */
#define STIMULUS_LOOP_LEN       60000u
#define STIMULUS_SEGMENT_COUNT  8u

/* Indices no laco. A amostra vai de SEGMENT_START a SEGMENT_END. */
#define STIMULUS_MARKER_START   1600u
#define STIMULUS_MARKER_END     1840u
#define STIMULUS_SEGMENT_START  16000u
#define STIMULUS_SEGMENT_END    48000u
#define STIMULUS_DEAD_ZONE_LEN  12000u   /* 1.5 s de cada lado */

/* Q15: -32768..32767 = -1..+1 do pico; o firmware escala por amp e soma mid */
extern const int16_t stimulus_preamble[STIMULUS_PREAMBLE_LEN];
extern const int16_t stimulus_segments[STIMULUS_SEGMENT_COUNT][STIMULUS_SEGMENT_LEN];

/* Nomes na ordem dos segmentos: "Sidekicking", "Elbowing", "Frontkicking", "Slapping", "Walking", "Handshaking", "Clapping", "Standing" */
extern const char *const stimulus_segment_names[STIMULUS_SEGMENT_COUNT];
/* "sub4 Aggressive/Sidekicking R-Thi t=4.90 s, SNR 10.7 dB" */
extern const char *const stimulus_segment_source[STIMULUS_SEGMENT_COUNT];

#endif /* STIMULUS_TABLE_H */
