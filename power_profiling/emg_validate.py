"""Validação de integridade do stream EMG recebido via BLE.

O dispositivo não manda número de sequência (main.c/ble_emg_service.c: o
payload é um reinterpret-cast cru do buffer, sem header). Mas o firmware
aplica um filtro IIR com estado persistente entre pacotes
(``butterworth_filter``, main.c:492-500) - então dois pacotes consecutivos
DEVEM se encaixar suavemente (a última amostra de um e a primeira do
próximo são vizinhas no tempo do filtro). Isso dá um detector de perda real,
sem precisar de nenhum suporte de protocolo: ``frac_continuous_joins``.

Também não assumimos a taxa de pacote "documentada" (16,67/s) como
referência - o firmware permite só 1 notificação pendente por vez
(``tx_in_progress``), então a taxa real é limitada pelo intervalo de
conexão (75-100 ms => 10-13,3 pacotes/s), e blocos são DESCARTADOS (não
reenviados) quando ``NRF_ERROR_BUSY``. A validação mede a taxa real e a
compara com o intervalo de conexão estimado, não com um número fixo.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Literal

import numpy as np

import dsp
from config import EMG_SAMPLES_PER_PACKET


@dataclass
class EmgValidation:
    n_packets: int
    duration_s: float
    received_hz: float
    conn_interval_est_ms: float
    conn_interval_from_current_ms: float
    expected_hz: float
    rate_ok: bool
    iat_p50_ms: float
    iat_p95_ms: float
    iat_max_ms: float

    frac_continuous_joins: float
    est_air_dropped_fraction: float

    n_bad_length: int
    n_duplicate_payload: int
    is_constant: bool
    frac_at_rail: float
    mean: float
    std: float

    fs_effective_sps: float
    corner_lo_hz: float
    corner_hi_hz: float
    corners_ok: bool
    band_frac: float
    dc_frac: float
    hf_frac: float
    line_50_60_frac: float
    spectral_ok: bool

    verdict: Literal["real", "suspect", "garbage", "inconclusive"]
    reasons: list[str] = field(default_factory=list)


def _iat_stats(t: np.ndarray) -> tuple[float, float, float, float]:
    """Estatisticas do intervalo entre chegadas, e estimativa do intervalo de
    conexao.

    A distribuicao de IAT e BIMODAL desde que o firmware passou a enviar
    varias notificacoes por evento de conexao: intervalos quase nulos DENTRO
    de uma rajada, e ~intervalo_de_conexao ENTRE rajadas. A versao anterior
    tomava a moda de todos os IATs, que nessa situacao cai no primeiro bin
    (~0 ms) e nao representa o intervalo de conexao - chegava a dar divisao
    por zero. Agora a estimativa usa so os intervalos ENTRE rajadas.
    """
    if len(t) < 3:
        return float("nan"), float("nan"), float("nan"), float("nan")
    iat = np.diff(np.sort(t)) * 1000.0
    if len(iat) == 0:
        return float("nan"), float("nan"), float("nan"), float("nan")

    # separa "entre rajadas" de "dentro da rajada": 2 ms e bem abaixo de
    # qualquer intervalo de conexao BLE valido (minimo 7,5 ms) e bem acima do
    # tempo entre notificacoes consecutivas de uma mesma rajada
    between = iat[iat > 2.0]
    if between.size >= 3:
        hist, edges = np.histogram(between, bins=min(30, max(3, between.size // 3)))
        # centro do bin, nao a borda esquerda
        idx = int(np.argmax(hist))
        mode_ms = float((edges[idx] + edges[idx + 1]) / 2.0)
    else:
        mode_ms = float(np.median(iat))

    if not np.isfinite(mode_ms) or mode_ms <= 0:
        mode_ms = float("nan")

    return mode_ms, float(np.percentile(iat, 50)), float(np.percentile(iat, 95)), float(iat.max())


def _conn_interval_from_current(uA: np.ndarray | None, fs: float | None) -> float:
    """Autocorrelação do traço de corrente decimado, pra achar o período dos
    bursts de rádio de forma independente do host - a checagem cruzada mais
    forte que a bancada tem (dois instrumentos sem relógio compartilhado
    concordando no mesmo período físico)."""
    if uA is None or fs is None or len(uA) < 100:
        return float("nan")
    decim = max(1, int(fs / 1000))  # decima pra ~1 kHz antes de autocorrelacionar
    x = uA[: (len(uA) // decim) * decim].reshape(-1, decim).mean(axis=1)
    x = x - x.mean()
    if np.allclose(x, 0):
        return float("nan")
    fs_dec = fs / decim
    n = len(x)
    ac = np.correlate(x, x, mode="full")[n - 1 :]
    ac /= ac[0] or 1.0
    # ignora o lag ~0 e procura o primeiro pico secundário significativo
    min_lag = max(1, int(fs_dec * 0.02))  # >= 20 ms (abaixo do menor intervalo de conexao plausivel)
    max_lag = min(len(ac) - 1, int(fs_dec * 0.5))
    if max_lag <= min_lag:
        return float("nan")
    window = ac[min_lag:max_lag]
    peak_idx = int(np.argmax(window)) + min_lag
    if ac[peak_idx] < 0.15:
        return float("nan")
    return (peak_idx / fs_dec) * 1000.0


def _join_continuity(packets: list, thr_quantile: float = 0.999) -> tuple[float, list[float]]:
    seqs = [p.samples for p in packets if len(p.samples) > 0]
    if len(seqs) < 2:
        return float("nan"), []

    within_diffs: list[float] = []
    for s in seqs:
        if len(s) > 1:
            within_diffs.extend(np.abs(np.diff(s.astype(np.float64))).tolist())
    if not within_diffs:
        return float("nan"), []
    thr = float(np.quantile(within_diffs, thr_quantile))

    joins = []
    for prev, cur in zip(seqs[:-1], seqs[1:]):
        d = abs(float(cur[0]) - float(prev[-1]))
        joins.append(d <= thr)
    frac = float(np.mean(joins)) if joins else float("nan")
    return frac, within_diffs


def validate_stream(
    packets: list,
    *,
    uA: np.ndarray | None = None,
    fs_ppk2: float | None = None,
    duration_s: float | None = None,
    fs_acquisition: float | None = None,
) -> EmgValidation:
    """Valida o stream EMG recebido.

    ``fs_acquisition``: taxa de amostragem REAL do ADC (ex: lida do contador
    g_adc_ok_count do firmware via fw_counters.py). Se informada, e usada nas
    checagens espectrais em vez da taxa de ENTREGA.

    Essa distincao e essencial e nao e detalhe: quando blocos sao descartados
    pelo firmware, a taxa de entrega e menor que a de aquisicao, mas as
    amostras DENTRO de cada bloco continuam espacadas por 1/fs_aquisicao.
    Usar a taxa de entrega no espectro escala todo o eixo de frequencia pelo
    fator entre as duas. Foi exatamente o que aconteceu numa medicao real
    desta bancada: a captura mostrava pico em ~14 Hz com fs de entrega
    (394 S/s), quando o pico verdadeiro era 62,3 Hz (rede eletrica de 60 Hz)
    com fs de aquisicao (1772 S/s) - um erro de 4,5x que faria qualquer
    analise de banda mioeletrica dar conclusao errada.
    """
    reasons: list[str] = []

    n = len(packets)
    t_arr = np.array([p.t_os if p.t_os is not None else p.t_host for p in packets])
    dur = duration_s if duration_s is not None else (float(t_arr.max() - t_arr.min()) if n > 1 else float("nan"))
    received_hz = n / dur if dur else float("nan")

    conn_iv_mode_ms, iat_p50, iat_p95, iat_max = _iat_stats(t_arr)
    conn_iv_current_ms = _conn_interval_from_current(uA, fs_ppk2)
    expected_hz = 1000.0 / conn_iv_mode_ms if conn_iv_mode_ms and conn_iv_mode_ms > 0 else float("nan")

    # Taxa esperada = taxa de PRODUCAO de blocos pelo firmware
    # (fs_aquisicao / amostras_por_bloco), nao 1/intervalo_de_conexao.
    # A versao anterior comparava com 1/intervalo porque o firmware so
    # permitia UMA notificacao pendente por vez (o gate tx_in_progress), o que
    # travava o throughput em 1 notificacao por evento de conexao. Esse gate
    # foi removido e a fila da SoftDevice aprofundada, entao agora varias
    # notificacoes podem sair por evento e received_hz pode legitimamente
    # EXCEDER 1/intervalo - o teste antigo passou a reprovar hardware saudavel.
    block_hz = (fs_acquisition / EMG_SAMPLES_PER_PACKET) if fs_acquisition else float("nan")
    if block_hz == block_hz and block_hz > 0:
        # nao pode entregar mais blocos do que produz (folga de 5% p/ jitter de
        # janela); entregar menos e perda, reportada separadamente abaixo
        rate_ok = bool(0 < received_hz <= block_hz * 1.05)
        if not rate_ok:
            reasons.append(
                f"taxa recebida ({received_hz:.2f}/s) incompativel com a producao "
                f"de blocos do firmware ({block_hz:.2f}/s)"
            )
    else:
        rate_ok = bool(expected_hz and abs(received_hz - expected_hz) / expected_hz < 0.30)
        if not rate_ok:
            reasons.append(
                f"taxa recebida ({received_hz:.2f}/s) fora de 30% da esperada pelo "
                f"intervalo de conexao ({expected_hz:.2f}/s); passe fs_acquisition "
                f"para um teste mais preciso"
            )

    frac_joins, _ = _join_continuity(packets)
    est_air_dropped = 1.0 - frac_joins if frac_joins == frac_joins else float("nan")  # NaN-safe

    all_samples = np.concatenate([p.samples for p in packets]) if packets else np.array([], dtype=np.int16)
    n_bad_length = sum(1 for p in packets if p.n_bytes != 120)
    n_dup = 0  # contado no ble_client; se precisar recomputar aqui, comparar payloads brutos
    is_constant = bool(len(all_samples) > 1 and np.std(all_samples) == 0)
    frac_at_rail = float(np.mean(np.abs(all_samples) >= 32700)) if len(all_samples) else float("nan")
    mean = float(np.mean(all_samples)) if len(all_samples) else float("nan")
    std = float(np.std(all_samples)) if len(all_samples) else float("nan")

    fs_delivered = (len(all_samples) / dur) if dur else float("nan")
    # taxa usada NO ESPECTRO: a de aquisicao quando conhecida (ver docstring)
    fs_eff = fs_acquisition if fs_acquisition else fs_delivered
    if fs_acquisition and fs_delivered == fs_delivered and fs_delivered > 0:
        lost = 1.0 - fs_delivered / fs_acquisition
        if lost > 0.05:
            reasons.append(
                f"{lost:.1%} das amostras adquiridas nao foram entregues "
                f"(aquisicao {fs_acquisition:.0f} S/s vs entrega {fs_delivered:.0f} S/s) "
                f"- blocos descartados pelo firmware"
            )

    corner_lo = corner_hi = float("nan")
    band_frac = dc_frac = hf_frac = line_frac = float("nan")
    spectral_ok = False
    if len(all_samples) >= 256 and fs_eff and fs_eff > 0:
        x = all_samples.astype(np.float64)
        x -= x.mean()
        freqs, psd = dsp.welch_psd(x, fs_eff, nperseg=min(512, len(x)))
        corner_lo, corner_hi = dsp.find_corner_frequencies(freqs, psd)
        band_frac = dsp.spectral_band_fraction(freqs, psd, max(1.0, corner_lo), max(2.0, corner_hi))
        dc_frac = dsp.spectral_band_fraction(freqs, psd, 0.0, 5.0)
        hf_frac = dsp.spectral_band_fraction(freqs, psd, corner_hi * 1.5, freqs.max()) if corner_hi == corner_hi else float("nan")
        # rede eletrica: fundamental (50 ou 60 Hz) + harmonicos, com janela
        # larga o suficiente para o desvio real de frequencia da rede e para o
        # vazamento espectral da janela de Hann
        line_frac = 0.0
        for f0 in (50.0, 60.0):
            for h in (1, 2, 3):
                f = f0 * h
                if f < freqs.max():
                    bf = dsp.spectral_band_fraction(freqs, psd, f - 6.0, f + 6.0)
                    if bf == bf:
                        line_frac += bf
        line_frac = min(line_frac, 1.0)

        # a banda passante escala com a taxa de loop efetiva, nao eh fixa em
        # 20-500 Hz: gate contra fs_eff * (0.01, 0.20), coerente com o
        # relatorio de exploracao (loop de I2C bloqueante, nao o clock do ADC)
        corners_ok = bool(
            corner_lo == corner_lo
            and corner_hi == corner_hi
            and fs_eff * 0.005 <= corner_lo <= fs_eff * 0.03
            and fs_eff * 0.10 <= corner_hi <= fs_eff * 0.30
        )
        mains_dominated = line_frac > 0.30
        spectral_ok = bool(corners_ok and band_frac > 0.60 and dc_frac < 0.10)
        if mains_dominated:
            # Com um interferente de banda estreita dominando (rede eletrica
            # captada por eletrodo aberto/flutuante e amplificada por toda a
            # cadeia), NAO da para medir os cortes do filtro: a heuristica de
            # -3 dB do plato mede onde esta a energia do interferente, nao a
            # resposta do filtro. Isso e propriedade da ENTRADA, nao defeito do
            # dispositivo - e na verdade evidencia positiva de que a cadeia
            # analogica esta viva e com ganho.
            reasons.append(
                f"espectro dominado por rede eletrica ({line_frac:.0%} da potencia em "
                f"50/60 Hz e harmonicos) - caminho analogico comprovadamente vivo, mas "
                f"nao da para medir a banda do filtro nem chamar de EMG real sem "
                f"eletrodos num sujeito"
            )
        elif not spectral_ok:
            reasons.append(
                f"espectro fora do esperado: corners=({corner_lo:.1f},{corner_hi:.1f}) Hz, "
                f"band_frac={band_frac:.2f}, dc_frac={dc_frac:.2f}"
            )
    else:
        corners_ok = False
        reasons.append("amostras insuficientes para checagem espectral")

    if is_constant:
        reasons.append("sinal constante (std=0) - stream provavelmente morto/travado")
    if frac_at_rail == frac_at_rail and frac_at_rail > 0.01:
        reasons.append(f"{frac_at_rail:.1%} das amostras saturadas nos limites do int16")
    if n_bad_length:
        reasons.append(f"{n_bad_length} pacote(s) com tamanho != 120 bytes (MTU truncando?)")

    cross_instrument_ok = True
    if (
        conn_iv_current_ms == conn_iv_current_ms
        and conn_iv_mode_ms == conn_iv_mode_ms
        and conn_iv_mode_ms > 0
    ):
        cross_instrument_ok = abs(conn_iv_current_ms - conn_iv_mode_ms) / conn_iv_mode_ms < 0.15
        if not cross_instrument_ok:
            reasons.append(
                f"intervalo de conexao pelo host ({conn_iv_mode_ms:.1f} ms) discorda "
                f"do estimado pela corrente ({conn_iv_current_ms:.1f} ms)"
            )

    if n == 0:
        verdict = "garbage"
        reasons.append("nenhum pacote recebido")
    elif is_constant or (frac_at_rail == frac_at_rail and frac_at_rail > 0.5):
        verdict = "garbage"
    elif rate_ok and spectral_ok and n_bad_length == 0 and cross_instrument_ok:
        verdict = "real"
    else:
        verdict = "suspect"

    return EmgValidation(
        n_packets=n,
        duration_s=dur,
        received_hz=received_hz,
        conn_interval_est_ms=conn_iv_mode_ms,
        conn_interval_from_current_ms=conn_iv_current_ms,
        expected_hz=expected_hz,
        rate_ok=rate_ok,
        iat_p50_ms=iat_p50,
        iat_p95_ms=iat_p95,
        iat_max_ms=iat_max,
        frac_continuous_joins=frac_joins,
        est_air_dropped_fraction=est_air_dropped,
        n_bad_length=n_bad_length,
        n_duplicate_payload=n_dup,
        is_constant=is_constant,
        frac_at_rail=frac_at_rail,
        mean=mean,
        std=std,
        fs_effective_sps=fs_eff,
        corner_lo_hz=corner_lo,
        corner_hi_hz=corner_hi,
        corners_ok=corners_ok,
        band_frac=band_frac,
        dc_frac=dc_frac,
        hf_frac=hf_frac,
        line_50_60_frac=line_frac,
        spectral_ok=spectral_ok,
        verdict=verdict,
        reasons=reasons,
    )


@dataclass
class GainResponse:
    levels: list[int]
    rms: list[float]
    ratio_vs_ref: list[float]
    spearman_rho: float
    monotonic: bool
    input_condition: str
    passed: bool
    verdict: Literal["pass", "fail", "inconclusive"]


def _spearman(a: np.ndarray, b: np.ndarray) -> float:
    ra = np.argsort(np.argsort(a)).astype(np.float64)
    rb = np.argsort(np.argsort(b)).astype(np.float64)
    if ra.std() == 0 or rb.std() == 0:
        return float("nan")
    return float(np.corrcoef(ra, rb)[0, 1])


def validate_gain_response(packets: list, input_condition: str = "unknown") -> GainResponse:
    by_level: dict[int, list[np.ndarray]] = {}
    for p in packets:
        by_level.setdefault(p.gain_level, []).append(p.samples)

    levels = sorted(by_level.keys())
    rms = []
    for lvl in levels:
        arr = np.concatenate(by_level[lvl]).astype(np.float64)
        rms.append(float(np.sqrt(np.mean(arr**2))))

    if input_condition == "open" or len(levels) < 2:
        return GainResponse(levels, rms, [], float("nan"), False, input_condition, False, "inconclusive")

    ref = rms[0] or 1.0
    ratio = [r / ref for r in rms]
    rho = _spearman(np.array(levels, dtype=np.float64), np.array(rms))
    monotonic = bool(rho == rho and rho > 0.9)
    passed = monotonic
    verdict = "pass" if passed else "fail"
    return GainResponse(levels, rms, ratio, rho, monotonic, input_condition, passed, verdict)
