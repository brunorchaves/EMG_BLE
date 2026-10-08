#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Monta o pacote de entrega do ensaio de qualidade de sinal: sinal injetado,
folha da sessao, capturas e um script de plot autocontido.

Mesma ideia do power_profiling/make_entrega.py: a receita fica versionada junto
do codigo que a gera, em vez de um zip montado a mao e esquecido. O que vai no
pacote e o que o Robert precisa para refazer a analise sem este repositorio.

Uso:
    python tools/make_entrega.py                             # so o lado do estimulo
    python tools/make_entrega.py --capturas /mnt/pendrive     # com as capturas do osciloscopio
    python tools/make_entrega.py --capturas /mnt/pendrive --saida ~/entregas
"""

from __future__ import annotations

import argparse
import json
import shutil
import zipfile
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STIM = ROOT / "stimulus"
BUILD = ROOT / "build" / "h563zi"

PLOT_SCRIPT = '''#!/usr/bin/env python3
"""Plota o sinal injetado, direto do reference.npz deste pacote.

Autocontido: so precisa de numpy e matplotlib, e nao depende do repositorio.

    python plot_estimulo.py                 # todas as acoes
    python plot_estimulo.py Sidekicking     # so uma
"""
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

AQUI = Path(__file__).resolve().parent
d = np.load(AQUI / "estimulo" / "reference.npz", allow_pickle=False)
fs = int(d["fs"])
pre, segs, tail = d["preamble"], d["segments"], d["tail"]
nomes = [str(n) for n in d["names"]]

quais = sys.argv[1:] or nomes
faltando = [q for q in quais if q not in nomes]
if faltando:
    sys.exit(f"acao desconhecida: {', '.join(faltando)}\\ndisponiveis: {', '.join(nomes)}")

fig, axes = plt.subplots(len(quais), 1, figsize=(11, 1.8 * len(quais)), sharex=True, squeeze=False)
for ax, nome in zip(axes[:, 0], quais):
    laco = np.concatenate([pre, segs[nomes.index(nome)], tail]) / 32767.0
    t = np.arange(len(laco)) / fs
    t0, t1 = len(pre) / fs, (len(pre) + segs.shape[1]) / fs
    ax.axvspan(0, t0, color="#fde8d0", zorder=0)
    ax.axvspan(t1, t[-1], color="#fde8d0", zorder=0)
    ax.plot(t, laco, lw=0.4)
    ax.set_ylim(-1.15, 1.15)
    ax.set_title(f"{nome}  -  amostra de {t0:.3f} s a {t1:.3f} s, "
                 f"zona morta de 0 V nas faixas claras", fontsize=9, loc="left")
axes[-1, 0].set_xlabel("tempo no laco (s)")
fig.tight_layout()
saida = AQUI / "estimulo_plot.png"
fig.savefig(saida, dpi=130)
print(f"-> {saida}")
'''


def leia_me(meta: dict, sessoes: list[Path], capturas: list[Path], cap_origem: str | None) -> str:
    t = meta["table"]
    segs = meta["segments"]
    linhas_seg = "\n".join(
        f"| {i} | {s['name']} | {s['group']} | sub{s['subject']} {s['channel']}, "
        f"t={s['start_s']:.2f} s | {s['snr_db_article_method']:.1f} dB | {i + 1} |"
        for i, s in enumerate(segs))

    if capturas:
        bloco_cap = (f"`capturas/` traz {len(capturas)} arquivo(s) copiados de `{cap_origem}`:\n\n"
                     + "\n".join(f"- `{c.name}` ({c.stat().st_size / 1024:.0f} kB)" for c in capturas))
    else:
        bloco_cap = ("`capturas/` esta **vazia**: as capturas do osciloscopio nao entraram neste\n"
                     "pacote. Para incluir, rode de novo com `--capturas <pasta>`.")

    return f"""# Ensaio de qualidade de sinal sEMG — pacote de entrega

Gerado em {date.today().isoformat()} por `stm32_dac_player/tools/make_entrega.py`.

O ensaio injeta trechos de EMG de um dataset publico nos terminais da placa sEMG
e de um EMG clinico ao mesmo tempo, pelo mesmo circuito de condicionamento, para
comparar os dois contra o sinal de referencia. Este pacote tem **o sinal que foi
injetado** (com precisao de amostra), **a folha da sessao** e **as capturas**.

## O que tem aqui

```
LEIA-ME.md              este arquivo
plot_estimulo.py        plota o sinal injetado (precisa de numpy e matplotlib)
estimulo/
  reference.npz         o sinal injetado, amostra a amostra, em Q15
  stimulus.json         parametros, origem no dataset, SHA-256 e CRC32
  segments.json         qual trecho do dataset virou cada acao, e por que
  stimulus_loop.png     figura do laco e das 8 acoes
  wav/                  um .wav por acao (8 kHz, 16 bits), para inspecao rapida
sessao/                 folha de cada sessao: o que foi disparado, e quando
capturas/               os arquivos do osciloscopio, como sairam do aparelho
analise/                resumo.md e resumo.csv, visao_geral.png, e um PNG por
                        captura com a estrutura do laco sobreposta
firmware/               o binario gravado na Nucleo, e a identificacao dele
```

## O sinal injetado

Taxa de **{t['fs']} S/s**, {t['segment_count']} acoes, laco de **{t['loop_s']} s**:

| trecho | conteudo |
|---|---|
| 0,000 – 0,200 s | silencio (aqui saem os pulsos de sync) |
| 0,200 – 0,230 s | 3 ciclos de 100 Hz — marcador de alinhamento |
| 0,230 – 2,000 s | **zona morta**, 0 V |
| **2,000 – 6,000 s** | **a amostra** do dataset, pico normalizado a ±1,0 |
| 6,000 – 7,500 s | **zona morta**, 0 V |

A amostra abre exatamente **2,000 s** depois do primeiro pulso de sync e fecha em
**6,000 s**. "0 V" e o codigo de meia escala no DAC, que depois do capacitor de
acoplamento vira 0 V nos terminais — entao o inicio e o fim da amostra ficam
entre dois trechos chatos de 0 V.

**Identificacao da acao na captura:** o canal de sync emite **(indice + 1) pulsos
de 10 ms** no inicio de cada laco. Conte os pulsos.

**Alinhamento fino:** por correlacao cruzada com o marcador de 100 Hz em 0,200 s.

| # | Acao | Grupo | Origem no dataset | SNR (metodo do artigo) | Pulsos |
|---|---|---|---|---|---|
{linhas_seg}

Integridade: SHA-256 `{t['sha256']}`, CRC32 `{t['crc32']}`.
O firmware recalcula o CRC da tabela na flash a cada boot e mostra no comando `info`.

## O dataset

{meta['dataset']['name']}.

Tres coisas que nao estao no readme da UCI e foram verificadas nos arquivos:
a taxa e **1000 S/s** (os `.log` tem Start/End, e ~9 800 amostras cobrem ~9 s),
a unidade e µV, e o sinal ceifa em ±4000 µV. O sub2 nao foi usado: a propria UCI
avisa que ele nao foi filtrado.

**O trecho exato da Fig. 5 do artigo nao e recuperavel.** O envelope da figura foi
digitalizado e correlacionado com todas as janelas de 4 s de todos os 80 arquivos
x 8 canais, em quatro escalas de tempo, e nada passou de r = 0,66 — nivel de
acaso. O "Original Signal" publicado passou por um pre-processamento que nao
ficou registrado. Por isso a escolha dos trechos e por **criterio** (ceifamento,
numero de rajadas, e SNR perto dos 10,7 dB publicados), reproduzivel e
versionada em `segments.json`.

Processamento de cada trecho: {meta['processing']}.

## As capturas

{bloco_cap}

## Como refazer a analise

1. `python plot_estimulo.py` desenha o sinal injetado (gera `estimulo_plot.png`).
2. Em cada captura, conte os pulsos de sync para saber a acao, ache o marcador de
   100 Hz e alinhe por correlacao cruzada contra `reference.npz`.
3. A janela de analise e a amostra: de +2,000 s a +6,000 s do primeiro pulso.
   As zonas mortas dao o piso de ruido medido de cada braco.
4. SNR pelo metodo do artigo: normaliza para [-1, 1], envelope RMS em janelas de
   50 ms, ativo onde o envelope passa de 20 % do pico, e
   SNR = 20 log10(RMS_ativo / RMS_repouso).

Para carregar o sinal de referencia:

```python
import numpy as np
d = np.load("estimulo/reference.npz")
fs = int(d["fs"])                      # 8000
nomes = [str(n) for n in d["names"]]   # ordem = indice do segmento
laco = np.concatenate([d["preamble"], d["segments"][nomes.index("Walking")], d["tail"]])
x = laco / 32767.0                     # Q15 -> [-1, 1]
```
"""


def copiar(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    if src.is_dir():
        shutil.copytree(src, dst, dirs_exist_ok=True)
    else:
        shutil.copy2(src, dst)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--capturas", type=Path, help="pasta com os arquivos do osciloscopio (ex.: /mnt/pendrive)")
    ap.add_argument("--padrao", default="*.csv", help="quais arquivos pegar de --capturas (default: *.csv)")
    ap.add_argument("--saida", type=Path, default=ROOT / "entrega", help="onde montar o pacote")
    args = ap.parse_args()

    meta = json.loads((STIM / "stimulus.json").read_text(encoding="utf-8"))
    nome = f"entrega_sinal_{date.today().isoformat()}"
    pkg = args.saida / nome
    if pkg.exists():
        shutil.rmtree(pkg)
    pkg.mkdir(parents=True)

    # --- lado do estimulo: o que define o ensaio ---
    for f in ("reference.npz", "stimulus.json", "segments.json", "stimulus_loop.png"):
        copiar(STIM / f, pkg / "estimulo" / f)
    if (STIM / "wav").is_dir():
        copiar(STIM / "wav", pkg / "estimulo" / "wav")

    # --- folhas de sessao ---
    sessoes = sorted((STIM / "sessoes").glob("*.md")) if (STIM / "sessoes").is_dir() else []
    for s in sessoes:
        copiar(s, pkg / "sessao" / s.name)

    # --- firmware gravado ---
    (pkg / "firmware").mkdir(parents=True, exist_ok=True)
    bin_path = BUILD / "stm32_dac_player_h563zi.bin"
    if bin_path.exists():
        copiar(bin_path, pkg / "firmware" / bin_path.name)
    (pkg / "firmware" / "IDENTIFICACAO.txt").write_text(
        "stm32_dac_player - NUCLEO-H563ZI\n"
        f"tabela SHA-256 {meta['table']['sha256']}\n"
        f"tabela CRC32   {meta['table']['crc32']}\n"
        f"taxa do DAC    {meta['table']['fs']} S/s\n"
        f"laco           {meta['table']['loop_s']} s, {meta['table']['segment_count']} acoes\n"
        "O comando 'info' do firmware mostra os mesmos valores e confere o CRC na flash.\n",
        encoding="utf-8", newline="\n")

    # --- capturas ---
    (pkg / "capturas").mkdir(parents=True, exist_ok=True)
    capturas: list[Path] = []
    if args.capturas:
        if not args.capturas.is_dir():
            raise SystemExit(f"{args.capturas} nao e uma pasta acessivel "
                             "(o pendrive esta montado? veja o README, secao do WSL)")
        capturas = sorted(p for p in args.capturas.rglob(args.padrao) if p.is_file())
        if not capturas:
            print(f"aviso: nenhum arquivo '{args.padrao}' em {args.capturas}")
        for c in capturas:
            copiar(c, pkg / "capturas" / c.name)
    else:
        (pkg / "capturas" / "FALTANDO.txt").write_text(
            "As capturas do osciloscopio nao entraram neste pacote.\n"
            "Rode de novo com:  python tools/make_entrega.py --capturas <pasta>\n",
            encoding="utf-8", newline="\n")

    # --- analise das capturas, se ja foi rodada ---
    ana = ROOT / "analise"
    if ana.is_dir():
        for f in ("resumo.csv", "resumo.md", "visao_geral.png"):
            if (ana / f).exists():
                copiar(ana / f, pkg / "analise" / f)
        if (ana / "plots").is_dir():
            copiar(ana / "plots", pkg / "analise" / "plots")

    (pkg / "plot_estimulo.py").write_text(PLOT_SCRIPT, encoding="utf-8", newline="\n")
    (pkg / "LEIA-ME.md").write_text(
        leia_me(meta, sessoes, capturas, str(args.capturas) if args.capturas else None),
        encoding="utf-8", newline="\n")

    # --- zip ---
    zip_path = args.saida / f"{nome}.zip"
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        for p in sorted(pkg.rglob("*")):
            if p.is_file():
                z.write(p, p.relative_to(pkg.parent))

    total = sum(p.stat().st_size for p in pkg.rglob("*") if p.is_file())
    print(f"pasta  {pkg}")
    print(f"zip    {zip_path}  ({zip_path.stat().st_size / 1024:.0f} kB; "
          f"{total / 1024:.0f} kB descompactado)")
    print(f"       {len(sessoes)} folha(s) de sessao, {len(capturas)} captura(s), "
          f"{meta['table']['segment_count']} acoes de referencia")
    if not capturas:
        print("       !! sem as capturas do osciloscopio - rode com --capturas <pasta>")


if __name__ == "__main__":
    main()
