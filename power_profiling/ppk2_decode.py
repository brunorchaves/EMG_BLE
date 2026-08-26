"""Decodificação offline e vetorizada do stream bruto da PPK2.

A captura (ppk2_stream.py) grava só palavras de 32 bits cruas em disco - zero
decodificação durante o run. Este módulo faz a decodificação depois, com
numpy, incluindo o contador de sequência de 6 bits que o ppk2-api descarta
(bits 18-23 de cada palavra) - é ele que permite detectar perda de amostra
sem precisar de nenhum timestamp de hardware.

Layout de bits de cada palavra (LSB->MSB), confirmado contra o parser oficial
da Nordic (pc-nrfconnect-ppk):
    [0:14)  MEAS_ADC     - valor bruto do ADC
    [14:17) MEAS_RANGE   - faixa de corrente ativa (0-4)
    [17:18) (não usado)
    [18:24) MEAS_COUNTER - contador de 6 bits, incrementa 1 a cada amostra, mod 64
    [24:32) MEAS_LOGIC   - canais digitais D0-D7 (só têm sinal real se o logic
                           port da PPK2 estiver cabeado)
"""

from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np

from config import (
    ADC_MULT,
    MASK_ADC,
    MASK_COUNTER,
    MASK_LOGIC,
    MASK_RANGE,
    POS_COUNTER,
    POS_LOGIC,
    POS_RANGE,
)


@dataclass
class DecodedBlock:
    current_uA: np.ndarray  # float32, uma por amostra
    range_idx: np.ndarray  # uint8
    counter: np.ndarray  # uint8, 0-63
    logic: np.ndarray  # uint8, bits D0-D7


@dataclass
class CounterReport:
    n_samples: int
    n_gaps: int
    gap_index: np.ndarray
    gap_missing_mod64: np.ndarray
    total_missing_lower_bound: int
    framing_offset: int
    ok: bool
    reasons: list[str] = field(default_factory=list)
    # Classificacao da causa dos gaps (preenchida quando range_idx e passado):
    # a PPK2 descarta uma conversao ao trocar de faixa de medicao, o que
    # aparece no contador como descontinuidade. Isso NAO e perda de captura -
    # e comportamento do instrumento. Distinguir os dois e essencial: um
    # significa "o dado do instrumento tem um furo", o outro significa "meu
    # software perdeu dado" (e invalidaria o run).
    n_gaps_at_range_switch: int = 0
    n_gaps_unexplained: int = 0
    capture_lossless: bool = True


def load_raw(path, mmap: bool = True) -> np.ndarray:
    """Carrega o arquivo de palavras brutas (uint32, little-endian) da captura."""
    if mmap:
        return np.memmap(path, dtype="<u4", mode="r")
    return np.fromfile(path, dtype="<u4")


def find_framing_offset(raw_bytes: bytes | bytearray | memoryview, max_try: int = 4) -> int:
    """Recupera a fase de alinhamento de bytes (0-3) testando qual offset dá
    um contador consistente (incrementando de 1 em 1, mod 64).

    Necessário só se a captura não começar exatamente no boundary de uma
    palavra de 4 bytes (ex: se houvesse bytes de metadata misturados no
    stream - não deveria acontecer com o fluxo desta bancada, mas a checagem
    é barata e detectaria o problema imediatamente em vez de decodificar
    lixo silenciosamente).
    """
    buf = bytes(raw_bytes)
    best_offset, best_score = 0, -1
    for offset in range(max_try):
        usable = len(buf) - offset
        n_words = usable // 4
        if n_words < 16:
            continue
        words = np.frombuffer(buf, dtype="<u4", count=n_words, offset=offset)
        counter = ((words >> POS_COUNTER) & MASK_COUNTER).astype(np.int32)
        diffs = np.diff(counter) % 64
        score = int(np.count_nonzero(diffs == 1))
        if score > best_score:
            best_offset, best_score = offset, score
    return best_offset


def check_counter(counter: np.ndarray, range_idx: np.ndarray | None = None) -> CounterReport:
    """Verifica continuidade do contador de 6 bits - a prova de que não
    perdemos amostras entre o dispositivo e o disco, sem precisar de nenhum
    timestamp de hardware.

    Se ``range_idx`` for passado, classifica cada gap por causa. A PPK2
    descarta uma conversão ao trocar de faixa de medição, e isso aparece no
    contador como descontinuidade - comportamento do INSTRUMENTO, não perda da
    captura. Medido nesta bancada: num run de 127 s, 100% dos 5394 gaps
    coincidiam com troca de faixa, e os estados com a placa desligada (faixa
    constante, 1,4 M amostras) tiveram ZERO gaps. Sem essa classificação o
    relatório acusava "172 mil amostras perdidas" e dava a impressão de que a
    captura estava furada, quando o software não perdeu nada.
    """
    n = len(counter)
    reasons: list[str] = []
    if n < 2:
        return CounterReport(n, 0, np.array([], dtype=np.int64), np.array([], dtype=np.int64), 0, 0, True)

    c = counter.astype(np.int64)
    d = (c[1:] - c[:-1]) % 64
    gap_mask = d != 1
    gap_index = np.flatnonzero(gap_mask) + 1  # índice da amostra APÓS o gap
    missing_mod64 = (d[gap_mask] - 1) % 64
    total_missing_lb = int(missing_mod64.sum())

    n_at_switch = 0
    n_unexplained = int(gap_index.size)
    capture_lossless = gap_index.size == 0

    if range_idx is not None and gap_index.size:
        switches = np.flatnonzero(np.diff(range_idx.astype(np.int16)) != 0) + 1
        # tolerancia de +-2 amostras: o descarte pode nao cair exatamente no
        # indice onde a faixa muda
        near = np.zeros(n, dtype=bool)
        for off in (-2, -1, 0, 1, 2):
            idx = switches + off
            idx = idx[(idx >= 0) & (idx < n)]
            near[idx] = True
        at_switch = near[gap_index]
        n_at_switch = int(at_switch.sum())
        n_unexplained = int((~at_switch).sum())
        capture_lossless = n_unexplained == 0

    if gap_index.size:
        if range_idx is None:
            reasons.append(
                f"{gap_index.size} gap(s) no contador (passe range_idx para "
                f"classificar entre descarte do instrumento e perda de captura)"
            )
        elif capture_lossless:
            reasons.append(
                f"{n_at_switch} descontinuidade(s) do contador, TODAS em troca de "
                f"faixa de medicao da PPK2 (descarte do instrumento, ~"
                f"{n_at_switch / n * 100:.3f}% das amostras). Captura sem perda."
            )
        else:
            reasons.append(
                f"{n_unexplained} gap(s) NAO explicado(s) por troca de faixa - "
                f"provavel PERDA DE CAPTURA (overflow de buffer serial). "
                f"{n_at_switch} outros sao descarte do instrumento em troca de faixa."
            )

    return CounterReport(
        n_samples=n,
        n_gaps=int(gap_index.size),
        gap_index=gap_index,
        gap_missing_mod64=missing_mod64,
        total_missing_lower_bound=total_missing_lb,
        framing_offset=0,
        ok=capture_lossless,
        reasons=reasons,
        n_gaps_at_range_switch=n_at_switch,
        n_gaps_unexplained=n_unexplained,
        capture_lossless=capture_lossless,
    )


def decode_words(
    words: np.ndarray,
    calibration: dict,
    source_v: float,
    *,
    spike_filter: bool = False,
) -> DecodedBlock:
    """Decodifica um array de palavras de 32 bits em corrente (uA) + metadados.

    Reimplementação vetorizada de ``PPK2_API.get_adc_result`` (ppk2_api.py) a
    partir da calibração lida uma vez no início do run
    (``PPK2_API.get_modifiers()``). Por padrão SEM o filtro de "spike" que a
    lib aplica por default para exibição em tempo real: esse filtro substitui
    amostras reais por uma média móvel nas transições de faixa, o que
    tenderia a enviesar qualquer integral de carga/energia calculada sobre um
    evento como o BOOT (que é exatamente onde a faixa de corrente muda mais).
    Quando necessário, roda com spike_filter=True e compara a diferença.
    """
    adc_raw = (words & MASK_ADC).astype(np.float64) * 4.0
    range_idx = np.minimum(((words >> POS_RANGE) & MASK_RANGE).astype(np.uint8), 4)
    counter = ((words >> POS_COUNTER) & MASK_COUNTER).astype(np.uint8)
    logic = ((words >> POS_LOGIC) & MASK_LOGIC).astype(np.uint8)

    r = calibration["R"]
    gs = calibration["GS"]
    gi = calibration["GI"]
    o = calibration["O"]
    s = calibration["S"]
    i_ = calibration["I"]
    ug = calibration["UG"]

    current_uA = np.zeros_like(adc_raw)
    for rng in range(5):
        mask = range_idx == rng
        if not np.any(mask):
            continue
        key = str(rng)
        result_without_gain = (adc_raw[mask] - o[key]) * (ADC_MULT / r[key])
        adc = ug[key] * (
            result_without_gain * (gs[key] * result_without_gain + gi[key])
            + (s[key] * (source_v / 1000.0) + i_[key])
        )
        current_uA[mask] = adc * 1e6

    if spike_filter:
        current_uA = _apply_spike_filter(current_uA, range_idx)

    return DecodedBlock(
        current_uA=current_uA.astype(np.float32),
        range_idx=range_idx,
        counter=counter,
        logic=logic,
    )


def _apply_spike_filter(current_uA: np.ndarray, range_idx: np.ndarray, alpha: float = 0.18) -> np.ndarray:
    """Réplica simplificada, vetorizável a posteriori, do filtro de exibição
    do app oficial - só para fins de comparação (ver decode_words)."""
    out = current_uA.copy()
    switches = np.flatnonzero(np.diff(range_idx.astype(np.int16)) != 0) + 1
    for idx in switches:
        lo = max(0, idx - 1)
        hi = min(len(out), idx + 3)
        window = out[lo:hi]
        avg = window.mean()
        out[lo:hi] = alpha * window + (1 - alpha) * avg
    return out


def digital_bit(logic: np.ndarray, ch: int) -> np.ndarray:
    """Extrai o bit de um canal digital (0-7) como array de 0/1."""
    return (logic >> ch) & 1


def find_edges(bit: np.ndarray, kind: str = "rising") -> np.ndarray:
    """Índices das amostras onde ``bit`` sobe (rising), desce (falling) ou
    qualquer transição (both). O índice retornado é o da amostra JÁ no novo
    estado."""
    d = np.diff(bit.astype(np.int8))
    if kind == "rising":
        return np.flatnonzero(d == 1) + 1
    if kind == "falling":
        return np.flatnonzero(d == -1) + 1
    if kind == "both":
        return np.flatnonzero(d != 0) + 1
    raise ValueError(f"kind desconhecido: {kind}")


def decimate_minmaxmean(uA: np.ndarray, factor: int) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Decimação estilo osciloscópio: cada bin de ``factor`` amostras vira
    (min, max, mean). Preserva transientes visíveis exatamente (nenhum pico é
    perdido), ao contrário de subsampling por passo fixo. Usado para gerar as
    versões plotáveis (current_1k.npy / current_100.npy) do traço completo.
    """
    n = len(uA) // factor
    if n == 0:
        return (np.array([]), np.array([]), np.array([]))
    trimmed = uA[: n * factor].reshape(n, factor)
    return trimmed.min(axis=1), trimmed.max(axis=1), trimmed.mean(axis=1)
