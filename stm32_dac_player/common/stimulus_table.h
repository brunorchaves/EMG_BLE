/* GERADO por tools/make_stimulus.py - nao editar. Regerar invalida a comparacao entre sessoes. */
#ifndef STIMULUS_TABLE_H
#define STIMULUS_TABLE_H

#include <stdint.h>

#define STIMULUS_LEN    52000u
#define STIMULUS_FS     8000u
#define STIMULUS_SHA256 "b145c17c2bcf125bee1cda7f18b5bb8a801dec087caec36a2e58aea1dbdb2570"
#define STIMULUS_CRC32  0xcb4abf10u
#define STIMULUS_SOURCE "UCI EMG Physical Action, sub4 Aggressive/Sidekicking col4 R-Thi, t=4.90..8.90 s"

/* Indices [inicio, fim) de cada trecho do laco */
#define STIMULUS_MARKER_START   4000u
#define STIMULUS_SWEEP_START    10400u
#define STIMULUS_SEGMENT_START  18400u
#define STIMULUS_SEGMENT_END    50400u

/* Q15: -32768..32767 = -1..+1 do pico; o firmware escala por amp e soma mid */
extern const int16_t stimulus_table[STIMULUS_LEN];

#endif /* STIMULUS_TABLE_H */
