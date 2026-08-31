#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Monta o pacote de entrega dos dados de corrente: CSV + script de plot + LEIA-ME.

Existe para que a entrega seja reprodutivel e para que o LEIA-ME fique
versionado junto do codigo que o gera, em vez de ser um texto solto escrito uma
vez e esquecido. Os arquivos de dados em si nao vao para o git (dezenas de MB,
e runs/ ja esta no .gitignore); o que fica versionado e a receita.

Uso:
    python make_entrega.py power_profiling/runs/<RUN>
    python make_entrega.py power_profiling/runs/<RUN> --com-npy   # inclui os 175 MB
"""

from __future__ import annotations

import argparse
import json
import shutil
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent

# .npy fica fora por default: sao ~175 MB e so servem a quem for reprocessar em
# resolucao cheia. O CSV a 1 kHz responde a pergunta e cabe num anexo.
CORE = [
    "conectado_streaming_1000Hz.csv",
    "todas_etapas_1000Hz.csv",
    "conectado_streaming_full.json",
    "todas_etapas_full.json",
    "resumo_janelas.json",
]
NPY = ["conectado_streaming_full_uA.npy", "todas_etapas_full_uA.npy"]


def leia_me(run_dir: Path, resumo: dict) -> str:
    c = resumo.get("conectado_streaming", {})
    t = resumo.get("todas_etapas", {})

    def linha(d: dict) -> str:
        return ("media {:.3f} mA   RMS {:.3f} mA   p99,9 {:.2f} mA   "
                "duracao {:.1f} s".format(d.get("mean_mA", float("nan")),
                                          d.get("rms_mA", float("nan")),
                                          d.get("p99_9_mA", float("nan")),
                                          d.get("duration_s", float("nan"))))

    return f"""CORRENTE vs TEMPO - sensor sEMG BLE (nRF52840)
Run {run_dir.name}   |   trilho 3,3 V   |   ADC a 1 kSPS
Medido com Nordic Power Profiler Kit II (PCA63100), source meter, 100 kS/s.


OS ARQUIVOS
-----------
conectado_streaming_1000Hz.csv   janela de operacao: conectado + streaming
                                 (este e o pior caso, e a janela que interessa)
todas_etapas_1000Hz.csv          o ciclo completo, do desligado ao desligado
*_full.json                      metadados: fs real, tensao, limites das bandas
resumo_janelas.json              as estatisticas ja calculadas, se preferir
plot_export_csv.py               script de plot, autocontido (stdlib + matplotlib)


COLUNAS DO CSV
--------------
t_s          tempo em segundos, desde o inicio da janela
i_mean_mA    media da corrente naquele bin
i_rms_mA     RMS da corrente naquele bin
i_min_mA     minimo do bin
i_max_mA     maximo do bin
estado       de qual fase de operacao o ponto veio

Cada linha resume UM bin de 100 amostras: a captura e a 100 kS/s e o arquivo
esta decimado para 1 kHz. Sem decimar, a janela de operacao teria 18,5 milhoes
de linhas.


COMO RECUPERAR MEDIA E RMS - EXATAMENTE
---------------------------------------
    media_total = mean(i_mean_mA)
    RMS_total   = sqrt(mean(i_rms_mA^2))

Os bins tem todos o mesmo tamanho, e e por isso que as duas formulas devolvem
o valor exato da resolucao cheia. Cada arquivo traz no cabecalho os valores de
referencia para conferencia.

NAO calcule RMS a partir de i_mean_mA. A media dentro do bin ja removeu a
variancia, e o resultado sai ~22% BAIXO. Foi justamente para permitir o calculo
correto que o arquivo carrega a coluna i_rms_mA em vez de so a media.


RESULTADO
---------
Conectado + streaming   {linha(c)}
Todas as etapas         {linha(t)}

A janela completa subestima a corrente de operacao, porque inclui as fases
desligada e desconectada.


TRES RESSALVAS
--------------
1. O RMS depende do tratamento dos artefatos do instrumento. A PPK2 troca a
   faixa de medicao quando a corrente muda de ordem de grandeza, e as amostras
   seguintes a transicao sao artefato. Como as trocas de faixa sao CAUSADAS
   pelos picos reais, artefato e sinal ocorrem no mesmo instante.
   Estes arquivos usam o spike filter do proprio fabricante, que substitui as 3
   amostras seguintes a cada troca por uma media movel causal.
   Efeito: a media varia ~8% entre bruto e tratado, mas o RMS varia ~45%
   ({c.get('rms_mA', float('nan')):.2f} mA tratado contra {c.get('rms_raw_mA', float('nan')):.2f} mA bruto).
   Qualquer RMS informado sem declarar o tratamento e ambiguo por um fator de ~1,5.

2. O MAXIMO nao e um pico da placa. Mesmo apos o filtro sobram ~0,001% de
   amostras acima de 25 mA, todas na mesma faixa de medicao - residuo de
   artefato. Use o p99,9 ou o p99,99 como pico representativo.

3. No arquivo todas_etapas, a banda RE_ADVERTISING NAO mede advertising: o link
   BLE continuou de pe (o central no Windows nao derruba a conexao ao chamar
   disconnect()). Ao sombrear o grafico por estado, trate-a como conectado. O
   aviso completo esta no cabecalho do proprio arquivo. A banda ADVERTISING e
   valida, e a janela conectado+streaming nao e afetada.


PARA PLOTAR
-----------
    python plot_export_csv.py conectado_streaming_1000Hz.csv
    python plot_export_csv.py todas_etapas_1000Hz.csv --ymax 25
    python plot_export_csv.py conectado_streaming_1000Hz.csv --de 100 --ate 101

O script imprime media e RMS pelas formulas acima, repassa os avisos do
cabecalho e gera o PNG. Precisa apenas de matplotlib.

Um detalhe que vale saber se for plotar por conta propria: tracar as centenas
de milhares de bins ponto a ponto produz uma mancha solida que parece corrente
continua de 0 a 15 mA. Nao e - sao excursoes de ~1% dos bins, os que coincidem
com um evento de conexao BLE (a cada ~97 ms). O script agrega por coluna de
pixel (min/max/media) para nao criar essa impressao. Com o recorte de 1 segundo
do terceiro exemplo acima, os eventos aparecem individualmente.
"""


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("run_dir")
    ap.add_argument("--out", default=None, help="default: <run_dir>/entrega")
    ap.add_argument("--com-npy", action="store_true",
                    help="inclui os .npy de resolucao cheia (~175 MB)")
    a = ap.parse_args()

    run_dir = Path(a.run_dir)
    src = run_dir / "export"
    if not src.is_dir():
        raise SystemExit(f"nao achei {src} - rode export_data.py primeiro")
    out = Path(a.out) if a.out else run_dir / "entrega"
    out.mkdir(parents=True, exist_ok=True)

    resumo = json.loads((src / "resumo_janelas.json").read_text(encoding="utf-8"))

    wanted = CORE + (NPY if a.com_npy else [])
    copied = []
    for name in wanted:
        f = src / name
        if not f.exists():
            print(f"  aviso: {name} nao existe, pulando")
            continue
        shutil.copy2(f, out / name)
        copied.append(name)

    shutil.copy2(HERE / "plot_export_csv.py", out / "plot_export_csv.py")
    copied.append("plot_export_csv.py")

    (out / "LEIA-ME.txt").write_text(leia_me(run_dir, resumo), encoding="utf-8")
    copied.append("LEIA-ME.txt")

    zip_path = out.parent / f"entrega_{run_dir.name}.zip"
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name in copied:
            z.write(out / name, name)

    print(f"\npasta:  {out}")
    total = 0
    for name in copied:
        sz = (out / name).stat().st_size
        total += sz
        print(f"  {name:38s} {sz / 1e6:8.2f} MB")
    print(f"  {'':38s} {'-' * 8}")
    print(f"  {'total solto':38s} {total / 1e6:8.2f} MB")
    print(f"\nzip:    {zip_path}")
    print(f"        {zip_path.stat().st_size / 1e6:.2f} MB "
          f"(compressao de {total / max(zip_path.stat().st_size, 1):.1f}x)")


if __name__ == "__main__":
    main()
