# Caderno de bancada — ensaios de qualidade de sinal

Um registro por coleta. Acrescentar no topo. Os campos obrigatórios vêm do
passo 10 do Ensaio 3 em [`TRES_ENSAIOS_SINAL.pdf`](docs/TRES_ENSAIOS_SINAL.pdf).

---

> **Atenção ao comparar os runs:** o divisor mudou entre eles. O Run 001 usou
> `R3` = 10 kΩ (1:11, a posição de conferência) e o Run 002 usou `R3` = 470 Ω
> (1:213,8). Níveis e SNR dos dois **não são comparáveis** sem refazer a conta
> pela atenuação de cada um.

## Run 002 — 2026-10-07 · as 8 ações, 36 capturas

Mesma bancada do Run 001, com o divisor na posição de trabalho e o firmware já
com as 8 ações. **A folha detalhada está em
[`stm32_dac_player/stimulus/sessoes/2026-10-07_disparos.md`](stm32_dac_player/stimulus/sessoes/2026-10-07_disparos.md)**
— aqui só o resumo.

| Campo | Valor |
|---|---|
| Arquivos | `scope_5.csv` a `scope_37.csv` em [`capturas/2026-10-07/`](stm32_dac_player/capturas/2026-10-07/) |
| Atenuação | **1:213,8** (`R2` 100 kΩ / `R3` 470 Ω, pelas faixas dos resistores) |
| Ganho do clínico | anotado 150× (chaves 1+2); **inferido dos dados: ~307×**, que bate com o 300 da tabela — **reconferir a chave** |
| CH1 / CH2 | saída do clínico / **saída do DAC** (no Run 001 o CH2 era o nó de injeção) |
| Osciloscópio | Agilent **DSO-X 2012A**, adquirindo a **2,5 kSa/s** |
| Nível nos terminais | ~11,8 mV pp, **RMS 0,950 mV** |

### O que mudou de entendimento em relação ao Run 001

| | Run 001 concluiu | Run 002 mostra |
|---|---|---|
| Taxa de amostragem | "Fs 370 Hz inviabiliza medida espectral" | o aparelho **adquiria a 2,5 kSa/s**; os 2000 pontos são o registro de tela. É ajuste de salvamento, não limite do osciloscópio |
| Amplitude do DAC | "abaixo do projeto, ±0,871 V" | é artefato de decimação: um DAC perfeito a 200 S/s mediria 2,227 V pp em vez de 2,741. **O DAC está correto** |
| Nível injetado | "28× o fisiológico" (com 1:11) | com 1:213,8, **RMS 0,950 mV — dentro** do teto de 1,5 mV do artigo. Só o pp fica 2× acima, e esse teste não vale para sinal de crista ~7 |
| Período do laço | "discrepância de 1,04 s a resolver" | medido **7,5000 s, desvio 0,000** em 3 capturas com dois marcadores |

O item do período se explica: no Run 001 a janela de 5,4 s não cabia o laço de
7,5 s, então o que parecia silêncio curto era borda de janela.

## Run 001 — 2026-10-07 · primeira injeção no EMG clínico

### Cadeia

```
NUCLEO-H563ZI (DAC PA4) → circuito de simulação → EMG System do Brasil
                                  │                        │
                                  └── CH2 do osciloscópio   └── saída analógica → CH1
```

### Registro

| Campo | Valor |
|---|---|
| Equipamento clínico | **EMG System do Brasil** (modelo a confirmar) |
| Ganho / filtros do clínico | **não registrado** — pegar no painel |
| Fonte de estímulo | NUCLEO-H563ZI, DAC PA4, tabela em flash |
| Tabela de estímulo (SHA-256) | `e9625fae6b306603f6e4c4097cab93c2ae003a7e6422762006ea6d9783d84094` |
| Segmento | laço de 7,5 s · 8 segmentos · marcador 100 Hz, 3 ciclos, em 0,2 s |
| Atenuação do simulador | **1 : 11** (`R3` = 10 kΩ — a posição de conferência, não a de trabalho) |
| CH1 | saída analógica do clínico |
| CH2 | nó de injeção, o mesmo que entra no clínico |
| Arquivos | [`scope_0.csv`](stm32_dac_player/capturas/2026-10-07/scope_0.csv), [`scope_1.csv`](stm32_dac_player/capturas/2026-10-07/scope_1.csv) |
| Osciloscópio | 2000 pontos · 5,3973 s · dt 2,7 ms · **Fs 370,37 Hz** |

### Medidas

| | scope_0 | scope_1 |
|---|---|---|
| CH1 pp | 5,261 V | 4,382 V |
| CH1 RMS (AC) | 312,2 mV | 306,4 mV |
| CH1 min / max | −3,156 / +2,106 V | −2,171 / +2,211 V |
| CH2 pp | 158,3 mV | 160,8 mV |
| CH2 RMS (AC) | 14,7 mV | 15,0 mV |
| Razão pp CH1/CH2 | 33,2 | 27,2 |
| corr(CH1, CH2) | −0,642 | −0,620 |

Contraste ativo/repouso, janelas de 0,6 s em `scope_0`:

| | ativo | repouso | contraste |
|---|---|---|---|
| CH2 (injetado) | 24,68 mV RMS | 1,302 mV RMS | **25,6 dB** |
| CH1 (saída do clínico) | 531,6 mV RMS | 60,1 mV RMS | **18,9 dB** |

### Achados

**1 · O player de estímulo está correto.** O envelope de CH2 dá **4,010 s** de
rajadas de EMG, contra 4,000 s da especificação (`segment_len` = 32000 @ 8 kS/s)
— casamento em 10 ms. E o **marcador de 100 Hz está lá**: 3 ciclos, 30 ms, em
t = +2,228 s, com 2 amostras por semiciclo (o esperado a 370 S/s). O mecanismo
de alinhamento funciona.

**2 · O clínico está sendo sobrecarregado.** Com 1:11 estão entrando
**158 mV pp**, que é **28× o fisiológico** (o alvo é 5,59 mV pp). Três
evidências de compressão:

- a saída é **assimétrica**: −3,156 V contra +2,106 V;
- o contraste ativo/repouso **cai de 25,6 dB na entrada para 18,9 dB na saída**
  — um amplificador linear preserva essa razão; perder 6,7 dB é compressão;
- `corr(CH1, CH2)` = −0,64, quando um caminho linear daria ~0,99.

Trocar para a atenuação de projeto (~1:400 a 1:501) não é refinamento, é
condição para a medida valer.

**3 · A taxa de amostragem inviabiliza qualquer medida espectral.**
Fs = 370,37 Hz dá Nyquist de **185 Hz**, e o sinal tem conteúdo até 400–500 Hz.
Tudo acima de 185 Hz dobra para dentro da banda — o espectro medido não
decai ao se aproximar de 185 Hz, que é a assinatura do rebatimento. **Não se
pode tirar SNR, espectro nem resposta em frequência destes arquivos.** O
envelope no tempo sobrevive; a forma de onda não.

**4 · A amplitude do DAC está abaixo do projeto.** 158,3 mV pp × 11 = 1,741 V pp
na entrada do divisor, ou **±0,871 V** — contra os ±1,400 V do projeto. Falta o
passo de calibração de amplitude.

**5 · Discrepância de cronometragem a resolver.** Entre o fim do segmento de EMG
(t = +1,569 s) e o marcador (t = +2,228 s) há **0,659 s** de silêncio. Pela
especificação deveriam ser **1,70 s** (tail de 1,5 s + 0,2 s de preâmbulo).
Diferença de 1,04 s. Resolver com o canal de sync antes de confiar no laço de
7,5 s para alinhamento.

### Para o próximo run

| | |
|---|---|
| 1 | **Exportar a memória de aquisição, não a tela.** Precisa de `dt ≤ 200 µs` (Fs ≥ 5 kS/s); vieram 2,7 ms |
| 2 | Trocar `R3` para a atenuação de trabalho (200 Ω → 1:501, ou 250 Ω → 1:401) |
| 3 | Calibrar a amplitude do DAC em ±1,400 V no `TP1` |
| 4 | **Probar PA5 (sync) num terceiro canal** — ele emite (índice+1) pulsos de 10 ms no início de cada laço: dá a fronteira do laço, o índice do segmento, e o trigger estável |
| 5 | Registrar ganho e filtros do clínico, e o modelo exato |
| 6 | Conferir o ceifamento na saída do clínico com a amplitude nova |

### Análise

Exploração deste run, junto das capturas que ela analisa:
[`run001/scope_analise.py`](stm32_dac_player/capturas/2026-10-07/run001/scope_analise.py) e
[`scope_analise_detalhe.py`](stm32_dac_player/capturas/2026-10-07/run001/scope_analise_detalhe.py), que geram
`scope_overview.png` e `scope_detail.png`.

A análise das **36 capturas da sessão inteira** (não só deste run) é
[`stm32_dac_player/tools/analisa_capturas.py`](stm32_dac_player/tools/analisa_capturas.py),
com o resultado em [`stm32_dac_player/analise/`](stm32_dac_player/analise/).
