"""Exporta corrente vs tempo de um run, em janelas selecionadas por estado.

Existe para atender um pedido concreto: entregar os pontos de corrente para
quem vai plotar e recalcular estatisticas por conta propria. Isso impoe uma
restricao que nao e obvia.

O PROBLEMA DA DECIMACAO
A captura e a 100 kS/s, entao 2 min de janela sao 12 milhoes de pontos -
inviavel de abrir numa planilha. Mas decimar por media simples DESTROI o RMS:
a media dentro de cada bin remove a variancia, e o RMS calculado sobre o sinal
decimado sai sistematicamente MENOR que o verdadeiro. Quem recebesse esse
arquivo e calculasse RMS chegaria a um numero errado sem perceber.

A SOLUCAO
Guardar, por bin, a media E o RMS. Com bins de tamanho igual, as duas
estatisticas globais sao recuperaveis EXATAMENTE:

    media_total = mean(i_mean)
    RMS_total   = sqrt(mean(i_rms**2))

O CSV carrega essas formulas no cabecalho. min e max por bin vao junto para
que o grafico mostre a envoltoria real (nenhum transiente desaparece) em vez
de uma linha suavizada.

Uso:
    python export_data.py <run_dir>
    python export_data.py <run_dir> --decim-hz 1000
"""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path

import numpy as np

import config
from ppk2_decode import decode_words, load_raw
from timeline import EventLog, build_bands, fit_stream_clock

# Janelas exportadas. A segunda e o "pior caso" de operacao: sem as fases
# desligada e desconectada, que puxam a media para baixo e mascaram o consumo
# real durante o uso.
WINDOWS: dict[str, tuple[str, ...]] = {
    "todas_etapas": tuple(s.value for s in config.State),
    "conectado_streaming": ("CONNECTED_IDLE", "STREAMING", "CONNECTED_IDLE_2"),
}


@dataclass
class WindowStats:
    name: str
    states: tuple[str, ...]
    n_samples: int
    duration_s: float
    mean_mA: float
    rms_mA: float
    min_mA: float
    max_mA: float
    p95_mA: float
    p99_9_mA: float
    mean_mW: float
    rms_mW: float
    # os mesmos numeros sobre o sinal BRUTO, para expor a sensibilidade do RMS
    mean_raw_mA: float
    rms_raw_mA: float
    max_raw_mA: float


def _load(run_dir: Path, spike_filter: bool = True):
    meta = json.loads((run_dir / "run_meta.json").read_text(encoding="utf-8"))
    source_v = meta["stream_config"]["source_mv"] / 1000.0
    w = np.asarray(load_raw(run_dir / "current_raw.u32")).astype(np.uint32)
    blk = decode_words(w, meta["ppk2_calibration"], source_v, spike_filter=spike_filter)
    raw = decode_words(w, meta["ppk2_calibration"], source_v, spike_filter=False)
    fit = fit_stream_clock(np.load(run_dir / "chunks.npy"), config.PPK2_FS_NOMINAL)
    log = EventLog.from_jsonl(run_dir / "events.jsonl")
    bands = build_bands(log, [s.value for s in config.State], fit)
    return meta, source_v, blk, raw, fit, bands


def _slices(bands, states, fs) -> list[tuple[int, int, str]]:
    out = []
    for b in bands:
        if b.state not in states:
            continue
        lo, hi = b.guarded_indices(fs)
        if hi > lo:
            out.append((lo, hi, b.state))
    return out


def export_window(
    run_dir: Path,
    name: str,
    states: tuple[str, ...],
    decim_hz: float,
    out_dir: Path,
) -> WindowStats:
    meta, source_v, blk, raw, fit, bands = _load(run_dir)
    fs = fit.fs_effective
    sl = _slices(bands, states, fs)
    if not sl:
        raise SystemExit("janela " + repr(name) + " nao tem nenhuma banda no run")

    cur = np.concatenate([blk.current_uA[lo:hi].astype(np.float64) for lo, hi, _ in sl])
    cur_raw = np.concatenate([raw.current_uA[lo:hi].astype(np.float64) for lo, hi, _ in sl])
    # etiqueta de estado por amostra, para o CSV dizer de onde veio cada ponto
    labels = np.concatenate([np.full(hi - lo, st, dtype=object) for lo, hi, st in sl])

    # --- resolucao cheia (.npy) -------------------------------------------
    np.save(out_dir / (name + "_full_uA.npy"), cur.astype(np.float32))
    (out_dir / (name + "_full.json")).write_text(
        json.dumps(
            {
                "unidade": "uA",
                "fs_hz": fs,
                "n_amostras": int(cur.size),
                "duracao_s": float(cur.size / fs),
                "tensao_trilho_V": source_v,
                "estados_incluidos": list(states),
                "bandas": [
                    {
                        "estado": st,
                        "i_inicio": int(lo),
                        "i_fim": int(hi),
                        "t_inicio_s": float(lo / fs),
                        "duracao_s": float((hi - lo) / fs),
                    }
                    for lo, hi, st in sl
                ],
                "spike_filter": "aplicado (metodo da Nordic)",
                "nota": (
                    "tempo da amostra i = i/fs_hz, contiguo dentro de cada banda; "
                    "as bandas foram concatenadas, entao ha descontinuidade de tempo "
                    "real entre elas quando a janela nao e o run inteiro"
                ),
            },
            indent=2,
        ),
        encoding="utf-8",
    )

    # --- decimado com media+RMS por bin (.csv) ----------------------------
    bin_n = max(1, int(round(fs / decim_hz)))
    n_bins = cur.size // bin_n  # descarta a cauda parcial: bins de tamanho
    usable = n_bins * bin_n     # IGUAL e o que torna a recuperacao exata
    m = cur[:usable].reshape(n_bins, bin_n)
    b_mean = m.mean(axis=1)
    b_rms = np.sqrt((m ** 2).mean(axis=1))
    b_min = m.min(axis=1)
    b_max = m.max(axis=1)
    b_state = labels[:usable].reshape(n_bins, bin_n)[:, 0]
    t = np.arange(n_bins) * bin_n / fs

    full_mean = cur[:usable].mean() / 1000.0
    full_rms = float(np.sqrt(np.mean(cur[:usable] ** 2))) / 1000.0

    header = [
        "# janela: " + name + "  (" + ", ".join(states) + ")",
        "# trilho: {:.2f} V   fs_original: {:.1f} Hz   decimado para: {:.1f} Hz"
        " ({} amostras por bin)".format(source_v, fs, fs / bin_n, bin_n),
        "# amostras originais: {}   duracao: {:.3f} s".format(usable, usable / fs),
        "# corrente em mA. Cada linha resume UM bin.",
        "#",
        "# COMO RECUPERAR AS ESTATISTICAS GLOBAIS EXATAMENTE:",
        "#   media_total = mean(i_mean_mA)",
        "#   RMS_total   = sqrt(mean(i_rms_mA**2))",
        "# NAO calcule RMS a partir de i_mean_mA: a media dentro do bin remove a",
        "# variancia e o resultado sai sistematicamente baixo.",
        "#",
        "# conferido contra a resolucao cheia: media {:.6f} mA, RMS {:.6f} mA".format(
            full_mean, full_rms
        ),
    ]

    csv_path = out_dir / (name + "_" + str(int(decim_hz)) + "Hz.csv")
    with csv_path.open("w", encoding="utf-8", newline="") as f:
        for line in header:
            f.write(line + "\n")
        f.write("t_s,i_mean_mA,i_rms_mA,i_min_mA,i_max_mA,estado\n")
        for i in range(n_bins):
            f.write(
                "{:.6f},{:.6f},{:.6f},{:.6f},{:.6f},{}\n".format(
                    t[i], b_mean[i] / 1000, b_rms[i] / 1000,
                    b_min[i] / 1000, b_max[i] / 1000, b_state[i],
                )
            )

    return WindowStats(
        name=name,
        states=states,
        n_samples=int(cur.size),
        duration_s=float(cur.size / fs),
        mean_mA=float(cur.mean() / 1000),
        rms_mA=float(np.sqrt(np.mean(cur ** 2)) / 1000),
        min_mA=float(cur.min() / 1000),
        max_mA=float(cur.max() / 1000),
        p95_mA=float(np.percentile(cur, 95) / 1000),
        p99_9_mA=float(np.percentile(cur, 99.9) / 1000),
        mean_mW=float(cur.mean() / 1000 * source_v),
        rms_mW=float(np.sqrt(np.mean(cur ** 2)) / 1000 * source_v),
        mean_raw_mA=float(cur_raw.mean() / 1000),
        rms_raw_mA=float(np.sqrt(np.mean(cur_raw ** 2)) / 1000),
        max_raw_mA=float(cur_raw.max() / 1000),
    )


def verify_csv(csv_path: Path) -> tuple[float, float]:
    """Auto-checagem: recalcula media e RMS a partir do CSV decimado pelas
    formulas do cabecalho. Se a decimacao estiver errada, isso pega na hora em
    vez de entregar dado silenciosamente enviesado.

    Parseia a mao em vez de usar genfromtxt: as linhas de comentario do
    cabecalho contem virgulas (lista de estados, valores de conferencia), e o
    genfromtxt as confundia com a linha de nomes de coluna.
    """
    means: list[float] = []
    rmss: list[float] = []
    with csv_path.open(encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or line.startswith("t_s,"):
                continue
            parts = line.strip().split(",")
            if len(parts) < 3:
                continue
            means.append(float(parts[1]))
            rmss.append(float(parts[2]))
    if not means:
        raise SystemExit("CSV sem linhas de dados: " + str(csv_path))
    a = np.asarray(means)
    b = np.asarray(rmss)
    return float(a.mean()), float(np.sqrt(np.mean(b ** 2)))


def main() -> None:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("run_dir")
    ap.add_argument("--decim-hz", type=float, default=1000.0)
    ap.add_argument("--out", default=None, help="default: <run_dir>/export")
    a = ap.parse_args()

    run_dir = Path(a.run_dir)
    out_dir = Path(a.out) if a.out else run_dir / "export"
    out_dir.mkdir(parents=True, exist_ok=True)

    summary = {}
    for name, states in WINDOWS.items():
        st = export_window(run_dir, name, states, a.decim_hz, out_dir)
        csv_path = out_dir / (name + "_" + str(int(a.decim_hz)) + "Hz.csv")
        mean_rec, rms_rec = verify_csv(csv_path)
        d_mean = abs(mean_rec - st.mean_mA) / st.mean_mA if st.mean_mA else 0.0
        d_rms = abs(rms_rec - st.rms_mA) / st.rms_mA if st.rms_mA else 0.0
        # tolerancia frouxa o suficiente para a cauda parcial descartada, mas
        # apertada o suficiente para pegar um erro de metodo na decimacao
        ok = d_mean < 5e-3 and d_rms < 5e-3

        print("\n[" + name + "]  " + ", ".join(states))
        print("  {:.1f} s, {:,} amostras".format(st.duration_s, st.n_samples))
        print("  media {:.3f} mA   RMS {:.3f} mA   p99,9 {:.2f}   max {:.2f} mA".format(
            st.mean_mA, st.rms_mA, st.p99_9_mA, st.max_mA))
        print("  potencia: media {:.2f} mW   RMS {:.2f} mW".format(st.mean_mW, st.rms_mW))
        print("  (bruto, sem spike filter: media {:.3f}  RMS {:.3f}  max {:.1f} mA)".format(
            st.mean_raw_mA, st.rms_raw_mA, st.max_raw_mA))
        print("  auto-checagem do CSV: media {:.6f} / RMS {:.6f}  -> {} (erro {:.2e} / {:.2e})".format(
            mean_rec, rms_rec, "OK" if ok else "DIVERGE", d_mean, d_rms))
        if not ok:
            raise SystemExit("a decimacao nao preserva as estatisticas - nao entregar este CSV")
        summary[name] = dict(st.__dict__, states=list(st.states),
                             csv_check_mean_mA=mean_rec, csv_check_rms_mA=rms_rec)

    (out_dir / "resumo_janelas.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print("\narquivos em " + str(out_dir))
    for f in sorted(out_dir.iterdir()):
        print("  {}  ({:.0f} kB)".format(f.name, f.stat().st_size / 1024))


if __name__ == "__main__":
    main()
