# Ensaios do artigo anterior e o que refazer para o novo

Levantamento de **quais ensaios sustentaram o artigo do SEB 2025**, **quais
continuam válidos com o hardware novo do Robert** (versão com supercapacitor) e
**quais materiais cada refação exige**.

> **Documento irmão:** [`BANCADA_ENSAIOS.pdf`](BANCADA_ENSAIOS.pdf) — o *como*.
> Passo a passo de cada ensaio, projeto completo do circuito de condicionamento
> (com esquemático), diagramas de aterramento e de pontos de medição, e a lista
> de compras. Este documento aqui é o *o quê* e o *por quê*.

> **Versões formatadas:** [`ENSAIOS_ARTIGO_2.pdf`](ENSAIOS_ARTIGO_2.pdf) (13 pp.,
> A4 — é a que vai para o Robert) e [`ENSAIOS_ARTIGO_2.html`](ENSAIOS_ARTIGO_2.html).
> **Este `.md` é a fonte de conteúdo**; o `.html` é a fonte de *layout* e as duas
> têm de ser mantidas em sincronia à mão. Para regerar o PDF depois de editar o
> HTML:
>
> ```bash
> "/c/Program Files/Google/Chrome/Application/chrome.exe" \
>   --headless=new --disable-gpu --no-pdf-header-footer \
>   --user-data-dir="$TEMP/chromeprof" --virtual-time-budget=12000 \
>   --print-to-pdf="$PWD/ENSAIOS_ARTIGO_2.pdf" \
>   "file:///$PWD/ENSAIOS_ARTIGO_2.html"
> ```

Fontes lidas: [`663744.pdf`](663744.pdf) (artigo),
[`Protocolo_Experimental_sEMG_Supercapacitor.pdf`](Protocolo_Experimental_sEMG_Supercapacitor.pdf)
(protocolo do Robert, 20 páginas, 8 ensaios),
[`sEMG_Shematic_versao_final.PDF`](sEMG_Shematic_versao_final.PDF) (placa nova,
3 folhas), [`Schematic_EMG-schematic-v2.0_2025-02-19.pdf`](Schematic_EMG-schematic-v2.0_2025-02-19.pdf)
(placa do artigo), [`README.md`](README.md), o código do firmware e os dados em
`data/`, `collectData/`, `emg_clinical/`, `power_profiling/`.

---

## 0. Veredito em uma tabela

| Ensaio do artigo | Refazer? | Por quê | Onde | Esforço |
|---|---|---|---|---|
| **A** — digitalizar o EMG clínico com ADS112C04 externo | **Sim, e trocando o instrumento** | A base de tempo do rig antigo não existia (§2.1) | Laboratório | 1 sessão |
| **B** — dataset UCI × clínico digitalizado | **Só recomputar** | É pós-processamento; nenhum hardware entra | Mesa | horas |
| **C** — dataset → DAC STM32 → condicionamento → sensor | **Sim** | Base de tempo + trilho de 5 V → 3,3 V (§3) | Mesa + Laboratório | 2 dias + 1 sessão |
| **D** — consumo por estado (Tabela 2) | **Sim, mas já está 80% pronto** | Bancada PPK2 existe e roda; falta a placa nova | Mesa | 1–2 dias |

E os ensaios do protocolo do Robert que **não existiam** no artigo — supercapacitor
(carga, descarga, autonomia) — são trabalho novo integral, e são justamente a
novidade do artigo 2.

> **A conclusão que mais importa:** o que obriga a refazer os ensaios de sinal
> **não é** principalmente o hardware novo. É que os dados antigos **não têm
> base de tempo válida** (§2.1). O eixo de frequência da Fig. 6 do artigo está
> errado por um fator entre 1,1 e 1,8, e **diferente para cada braço** da
> comparação. Isso não se corrige em pós-processamento.

---

## 1. O que foi feito no artigo (SEB 2025)

O artigo não mediu sEMG de voluntário. Ele **reproduziu fisicamente** um trecho
de *sidekicking* de um dataset público e gravou essa reprodução em três
caminhos, comparando os três. A Fig. 1 do artigo mostra a cadeia:
`PC (dataset) → DAC → eletrodos → pré-amplificação → ...`.

### Os três braços de sinal

**Braço A — EMG clínico digitalizado externamente.**
A saída analógica do EMG clínico do laboratório foi condicionada por um
amplificador operacional (para caber na janela de entrada do conversor) e
digitalizada por um **ADS112C04 em placa externa**, controlado por um ESP32
(`emg_clinical/`, ESP-IDF). Objetivo: pôr o sinal do clínico e o do protótipo no
mesmo formato, mesma resolução e — em teoria — mesma taxa, para que a comparação
fosse justa. Saída: CSVs em `data/csvs_clinical/`.

**Braço B — dataset como referência absoluta.**
*EMG Physical Action Data Set* (UCI, Theodoridis 2011; adquirido originalmente
com sistema **Delsys** de 8 canais). Nenhum hardware envolvido: é o arquivo de
referência contra o qual A e C são comparados. No artigo, é o traço "Original
Signal" da Fig. 5 e a SNR de 10,7 dB.

**Braço C — dataset reproduzido para dentro do sensor próprio.**
O trecho do dataset foi tocado por um **DAC de um STM32**, passado por um
**circuito de condicionamento** e injetado na entrada do sensor sEMG
desenvolvido. O condicionamento tinha de resolver três coisas ao mesmo tempo:
levar o sinal do domínio do DAC (0–3,3 V, unipolar) para uma janela simétrica em
torno da referência analógica da placa, atenuá-lo até a faixa de sEMG (0,01–6 mV
pp) e apresentá-lo de forma diferencial aos pads dos eletrodos. Saída: CSVs em
`data/csvs_proprietary/`.

### O quarto ensaio: consumo

A Tabela 2 do artigo (OFF / IDLE / CONNECTED / TRANSMITTING, 5,0 V) veio de uma
medição de corrente por estado. Hoje essa medição está **reconstruída e muito
melhor** com a PPK2 — ver [`power_profiling/`](power_profiling/) e a seção
*Consumo e desempenho medidos* do README.

### O que cada braço produziu

| Braço | SNR publicada |
|---|---|
| B — dataset original | 10,7 dB |
| C — sistema proposto | 9,0 dB |
| A — sistema clínico | 12,7 dB |

Mais a Fig. 5 (três sinais no tempo, com envelope RMS de 50 ms e segmentos de
contração detectados a 20 % do pico) e a Fig. 6 (resposta em frequência dos dois
sistemas).

---

## 2. Três problemas nos dados antigos

Isto não é pessimismo: é o que decide o que dá para reaproveitar.

### 2.1 A base de tempo não existia — o problema grave

Os dois braços de aquisição gravavam **texto ASCII por porta serial a 115200
baud**, e o script do PC simplesmente coletava o que chegasse durante N segundos.
Não havia timestamp, nem controle de taxa, nem relógio.

- [`collectData/colectData.py`](collectData/colectData.py) — laço
  `while (time.time() - t0) < TEMPO_COLETA: ser.readline()`, e grava o que veio.
- [`emg_clinical/collectData.py`](emg_clinical/collectData.py) — fixa
  `EXPECTED_SAMPLE_RATE_HZ = 1000` e para em exatamente 20 000 amostras,
  **independentemente do tempo decorrido**.
- [`emg_clinical/main/main.c:40-44`](emg_clinical/main/main.c#L40-L44) — o rig do
  clínico faz `printf("%d\n", adc_raw)` seguido de
  `vTaskDelay(pdMS_TO_TICKS(1))`. Com o tick padrão do ESP-IDF em 100 Hz, esse
  delay arredonda para **zero**: o laço corre tão rápido quanto o `printf`
  permitir, lendo o ADC de forma assíncrona às conversões (repetindo e perdendo
  amostras sem aviso).

A 115200 baud, uma linha `"12345\n"` custa ~7 bytes → **teto de ~1600 linhas/s**.
E é exatamente onde os arquivos caem:

| Arquivo | Amostras | Duração nominal | Taxa real implícita |
|---|---|---|---|
| `biceps_10s_4_proprietary.csv` | 11 119 | 10 s | **~1112 S/s** |
| `thigh_5s_2_1x_proprietary.csv` | 9 095 | 5 s | **~1819 S/s** |
| `biceps_20s_2_clinical.csv` | 20 000 | "20 s" | **desconhecida** (parou na contagem) |

E [`data/processdata/processdata.py:31`](data/processdata/processdata.py#L31)
assume `fs = 2000` para **todos**.

Consequências:

- **A resposta em frequência (Fig. 6) não é recuperável.** Cada braço tem um
  erro de escala de frequência diferente. Não há como desfazer sem conhecer a
  taxa real de cada arquivo, e ela não foi registrada.
- **A janela de envelope de "50 ms" era de 90 ms** no braço do bíceps
  proprietário (50 ms × 2000/1112).
- **A SNR sobrevive melhor**, porque é razão de amplitudes RMS e não depende
  fortemente da escala de tempo. Os 9,0 / 10,7 / 12,7 dB provavelmente estão na
  ordem certa. Mas a detecção de segmentos ativos, que usa a janela deslizante,
  fica deslocada.

### 2.2 Os dados antigos não são simultâneos

O artigo descreve gravação **simultânea** pelo protótipo e pelo clínico. Os CSVs
que sobraram no repositório não são isso: são contrações de bíceps e coxa, de
durações diferentes (10 s vs 20 s), com nomes que não se pareiam. O material da
sessão de *sidekicking* que gerou as Fig. 5 e 6 **não está neste repositório** —
é preciso pedir ao Robert.

### 2.3 O firmware do artigo não amostrava a 2 kS/s

Já documentado no README: no firmware da época **não havia fonte de tempo
governando a amostragem**, e o ADC era lido cerca de 1 vez por segundo. A Tabela 1
do artigo reivindica 2 kS/s. Isso hoje está corrigido — 1015 S/s medidos no modo
padrão, 2042 S/s em turbo, com 0 % de conversões perdidas e taxa verificável por
contadores em RAM (`fw_counters.py`).

---

## 3. O que mudou no hardware do Robert

Comparando o esquemático do artigo (`Schematic_EMG-schematic-v2.0`) com a versão
final (`sEMG_Shematic_versao_final`).

### 3.1 O que NÃO mudou — e é boa notícia

A cadeia analógica é **idêntica, componente por componente e valor por valor**:

| Estágio | Artigo | Versão final |
|---|---|---|
| Instrumentação | INA317, R_G = 3 kΩ → A_v = 34,33 | idem |
| Passa-alta | Sallen-Key, 220 nF / 36 kΩ, f_c 20 Hz, A_v 1,55 | idem |
| Passa-baixa | Sallen-Key, 1,5 kΩ / 220 nF, f_c 482 Hz, A_v 1,55 | idem |
| Ganho ajustável | DS3502 + MCP609 inversor, 2×–11× | idem |
| ADC | ADS112C04, AIN vs AVSS, ref = AVDD | idem |
| Proteção de entrada | BAT54S, R 1 MΩ/56 kΩ/2,2 kΩ, C 1 nF/10 pF/100 pF | idem |

**A função de transferência (banda 30–400 Hz, ganho total 166×–914×) não mudou.**
Isso é importante: significa que o *formato* do resultado do artigo deve se
reproduzir. O que muda é o piso de ruído e o teto de saturação.

### 3.2 O que mudou

| Item | Artigo (v2.0) | Versão final | Consequência |
|---|---|---|---|
| **Trilho analógico VCC** | 5,0 V | **3,3 V** (TPS63031 buck-boost) | teto de saturação −20 %, LSB −34 % |
| **Referência analógica** | **MAX6106**, 2,048 V de precisão | divisor resistivo **VCC/2 = 1,65 V** + buffer MCP609 | ratiométrica com o ADC (bom); sem PSRR de referência (ruim) |
| **Fonte** | conector externo; bateria Li-Ion 400 mAh @ 3,7 V + step-up (fora da placa) | **USB → BQ25173 → supercapacitor 1 F/5 V (KVR-5R0C105-R) → chave de carga → TPS63031 → 3V3** | ensaio de autonomia inteiramente novo |
| **Chave de carga** | não existia | P-FET DMP2035U + NMOS 2N7002 + botão SW1 + sinal `PWR_OFF` do MCU (trava de software) | novo comportamento de liga/desliga |
| **Telemetria de tensão** | não existia | divisor R22/R26 10 k/10 k → net `VCAP` → pino D1 do módulo (**P0.03**) | o firmware pode medir a própria carga |
| **LEDs** | 2, ânodo no trilho de 5 V | 2 (vermelho/verde), ânodo em VCC = 3,3 V | o brilho basal parasita **desaparece** |
| **Pull-ups I²C** | 4,7 kΩ | **10 kΩ** | risco de tempo de subida a 400 kHz |
| **Capacitância de bulk** | maior capacitor = 220 nF | ~20 µF na saída do TPS63031 | melhor, ainda abaixo do ideal |
| **Pontos de medição** | — | **jumpers 0 Ω R27 / R28 / R32** e test points MP4 (VCAP), MP5 (VIN), MP7 (+3V3) | permite separar consumo por subsistema |

### 3.3 Os três efeitos que realmente decidem o refazer

**(a) O teto de saturação caiu 20 %.** O MCP609 não é rail-to-rail; sobra cerca
de 0,1 V para cada trilho.

| | Artigo | Versão final |
|---|---|---|
| Excursão útil no último estágio | 2,048 − 0,1 = **±1,95 V** (assimétrica: 2,85 V do lado positivo) | 1,65 − 0,1 = **±1,55 V** (simétrica) |
| Entrada máx. antes de ceifar, ganho 914× | 2,13 mV | **1,70 mV** |
| Entrada máx. antes de ceifar, ganho 166× | 11,7 mV | **9,3 mV** |

sEMG vai de 0,01 a 6 mV pp (RMS < 1,5 mV, pelo próprio artigo). Ou seja: **o
ajuste de ganho usado nos ensaios antigos provavelmente ceifa agora.** Antes de
qualquer coisa, o DS3502 tem de ser re-trimado. Em compensação, o ceifamento
passou a ser **simétrico**, que é mais honesto — o README já havia registrado
que a 3,3 V a referência de 2,048 V ceifava o pico positivo em ~16 %, e a placa
nova corrige exatamente isso.

**(b) A resolução referida à entrada melhorou 34 %.** Com ref = AVDD e leitura
single-ended, o LSB é AVDD/32768:

| | Artigo (5,0 V) | Versão final (3,3 V) |
|---|---|---|
| LSB no ADC | 152,6 µV | **100,7 µV** |
| LSB referido ao eletrodo (ganho 914×) | 167 nV | **110 nV** |

Efeito de segunda ordem — a SNR de 9,0 dB é limitada por ruído analógico, não
por quantização — mas é a favor.

**(c) O risco novo: a referência do ADC agora é a saída de um conversor
comutado.** Antes, o ponto médio analógico vinha de um MAX6106 de precisão. Agora
vem de um divisor resistivo pendurado no trilho de 3,3 V do TPS63031 — **o mesmo
trilho que é a referência do ADC**. Duas coisas se seguem:

- *A favor:* o arranjo é **ratiométrico**. Se VCC deriva devagar, o ponto médio e
  o fundo de escala derivam juntos e o erro se cancela. Melhor estabilidade
  térmica e de longo prazo do que com o MAX6106.
- *Contra:* o `PS_SYNC` do TPS63031 está **aterrado** (pino 7 → GND), o que
  habilita o modo *power-save*. Com carga leve (~2,5 mA) o conversor entra em
  PFM: rajadas de comutação de frequência baixa e **variável**. O ADC amostra a
  1 kS/s de forma assíncrona a essas rajadas — o que pode dobrar tons de
  comutação para dentro de 30–400 Hz por aliasing. O cancelamento ratiométrico
  não protege contra isso, porque o passa-alta (C9/C10) desacopla o caminho de
  DC do sinal e do da referência de formas diferentes.

**É este item (c) que não se resolve por análise — só medindo.** E é a razão
técnica mais forte para refazer os ensaios de qualidade de sinal.

*Mitigação, se der problema:* amarrar `PS_SYNC` em nível alto força PWM fixo a
2,4 MHz (bem acima da banda), ao custo de algumas dezenas de µA de quiescente.
Vale a troca. Medir os dois casos.

---

## 4. Riscos de bring-up da placa nova

Antes de qualquer ensaio. Estes são achados de revisão do esquemático, não
problemas confirmados.

### 4.1 `PWR_OFF` está funcionando por acidente — e derruba a placa em qualquer reset

A trava: `R29` (10 kΩ) mantém o gate do P-FET `Q1` em V_CAP (desligado). O botão
`SW1` **ou** o NMOS `Q2` puxam o pé de `R30` para GND, colocando o gate em
V_CAP/2 e ligando `Q1`. O gate de `Q2` é a net `PWR_OFF`, com `R31` (10 kΩ) para
GND. Ou seja: apesar do nome, **`PWR_OFF` em nível alto = manter ligado**. É uma
trava de *keep-alive*.

`PWR_OFF` é o pino D6 do módulo = **P1.11**. E o firmware atual já escreve 1 em
P1.11 no boot — mas por outro motivo: era o TX da UART, forçado alto para matar
o brilho parasita do LED2 ([`main.c:674-675`](emg_nrf_ses/project/ble_peripheral/ble_app_blinky/main.c#L674-L675)).
Funciona, mas por coincidência.

Duas consequências reais:

1. **Janela de boot.** Entre soltar o botão e o `main()` chegar naquela linha, a
   trava depende só do botão. É preciso segurar o botão até o boot terminar, ou
   medir esse tempo e documentá-lo.
2. **Qualquer reset desliga a placa.** No reset, os GPIOs vão para alta
   impedância, `R31` puxa `PWR_OFF` para baixo, `Q2` abre, `R29` desliga `Q1` — e
   a alimentação do módulo cai antes de o reset completar. **Watchdog, brownout
   ou reset por software viram desligamento definitivo**, até alguém apertar o
   botão. *Correção sugerida:* um capacitor em paralelo com `R31` para sustentar
   a trava durante o reset (100 kΩ + 1 µF dá ~100 ms), ou `R31` mais alto.

### 4.2 O módulo pode estar sendo alimentado pelo pino errado

`R32` (0 Ω) liga VCC = 3,3 V ao **pino 14 (`5V`)** do módulo XIAO nRF52840 — o
pino de VBUS, que no módulo alimenta o regulador interno. Alimentar 3,3 V ali
deixa o MCU em ~3,0–3,25 V (queda do LDO, ou de um diodo de bloqueio), enquanto
os pull-ups de I²C estão em 3,3 V — ou seja, VDD + 0,3 V no limite do
absoluto máximo dos pinos.

O esquemático também traz `R37` (0 Ω) no **pino 12 (`3V3`)**. Pela extração da
netlist não consegui resolver com certeza onde a outra ponta de `R37` vai.
**Pergunta para o Robert:** `R32` e `R37` são opções mutuamente exclusivas de
alimentação do módulo? Alimentar direto no pino `3V3` (só `R37`) é o caminho
correto num sistema de 3,3 V, e evita perder ~10 % da energia do supercapacitor
num LDO redundante.

### 4.3 Pull-ups de 10 kΩ podem não fechar 400 kHz

Fast mode exige tempo de subida < 300 ns. Com `t_r ≈ 0,85·R·C`, 10 kΩ tolera no
máximo **~35 pF** de capacitância de barramento. Com 4,7 kΩ eram 75 pF. Numa
placa pequena com dois escravos é plausível, mas apertado.
*Se o I²C falhar:* baixar para 100 kHz (custa taxa de amostragem — o README
registra que a 100 kHz se perdiam 11 % das conversões) ou popular 4,7 kΩ.

### 4.4 A tensão de corte real é definida pela chave de carga, não pelo conversor

O TPS63031 opera de 1,8 V a 5,5 V de entrada. Mas o gate de `Q1` fica em
V_CAP/2, logo **V_gs = −V_CAP/2**. Com V_CAP em 2,0 V, V_gs = −1,0 V — em cima
do limiar do DMP2035U. Conforme o supercapacitor descarrega, o P-FET vai saindo
de condução e a R_DS(on) sobe. A carga é pequena (poucos mA), então isso perdoa
bastante, mas o corte real deve cair em **V_CAP ≈ 2,0–2,5 V, não 1,8 V** —
custando 10 a 25 % da energia útil.

*Correção sugerida:* `R30` não é necessária para proteção (o DMP2035U aguenta
V_gs de ±20 V, e `R29` já limita a corrente de gate). Trocar `R30` por 0 Ω leva
o gate direto a GND, dando V_gs = −V_CAP em toda a faixa. Mas **medir antes de
mexer** — é exatamente o que o Ensaio 4 apura.

### 4.5 Falta capacitor de bulk

O README já mediu: os picos de corrente são **um por conversão do ADC** (~1000/s),
20 µs de largura, movendo ~0,4 µC cada, com p99,9 de 18,7 mA. Os ~20 µF na saída
do TPS63031 seguram isso em ~20 mV de excursão. Com 100 µF ficaria em ~5 mV.
Isso importa mais agora do que antes, porque **esse trilho é a referência do
ADC** (§3.3c).

---

## 5. Contas de guardanapo: a autonomia do supercapacitor

Este é o número que define o artigo 2, então vale fazer a conta antes de ir para
a bancada — para saber se o resultado medido faz sentido.

### 5.1 Energia disponível

O protocolo do Robert traz `E = C·V²` e `E_disp = C·(V²_max − V²_min)`.
**Falta o fator ½** nas duas — as fórmulas corretas são:

```
E       = ½ · C · V²
E_disp  = ½ · C · (V²max − V²min)
```

Não é preciosismo: o erro **dobra** a energia disponível e, com ela, a autonomia
declarada. Vale corrigir no documento antes que vire número publicado.

Com C = 1 F e V_max = 5,0 V (o divisor `BQ_FB` R21/R24 = 634 k/200 k dá
1,2 V × (1 + 3,17) ≈ **5,0 V** — coerente com a tensão nominal do
KVR-5R0C105-R):

| Cenário de corte | E_disp | Após o TPS63031 (η ≈ 0,85) |
|---|---|---|
| V_off = 1,8 V (limite do conversor) | 10,88 J | 9,25 J |
| V_off = 2,5 V (limite provável da chave, §4.4) | 9,38 J | **7,97 J** |

### 5.2 Autonomia estimada

Usando as correntes **já medidas** com a PPK2 a 3,3 V e 1 kSPS
(`power_profiling/PENDENTE_ENSAIO_2.md`), no cenário conservador de 7,97 J:

| Estado | Corrente medida | Potência | Autonomia estimada |
|---|---|---|---|
| Streaming contínuo | 2,734 mA | 9,02 mW | **~15 min** |
| Conectado + streaming | 2,647 mA | 8,73 mW | ~15 min |
| Advertising (ADC em power-down) | 1,54 mA | 5,08 mW | **~26 min** |
| Mistura 94 % adv / 3 % conn / 3 % stream | 1,60 mA | 5,28 mW | ~25 min |

Com a chave de carga corrigida (corte em 1,8 V), some ~15 %: ~17 min de
streaming, ~29 min na mistura.

> **Ressalva:** essas correntes foram medidas com a PPK2 alimentando VCC
> **diretamente** a 3,3 V, sem o TPS63031 no caminho. Na placa nova falta somar
> o quiescente do conversor (dezenas de µA) e a perda de conversão. O η = 0,85
> acima já é uma tentativa de contabilizar isso, mas o número real sai do
> Ensaio 4.

### 5.3 Tempo de carga e o argumento do artigo

`t = C·ΔV / I_chg`. Para 1 F de 0 a 5 V:

| Corrente de carga | Tempo |
|---|---|
| 50 mA | 100 s |
| 100 mA | 50 s |
| 200 mA | 25 s |

A corrente é fixada por `R25` = 10 kΩ no pino ISET do BQ25173 — **confirmar a
constante no datasheet** (e é medível direto no Ensaio 3).

Duas observações que o protocolo não cobre e que valem virar resultado:

- **O BQ25173 é linear.** Carregar um capacitor de 0 a V_in por um elemento série
  dissipa tanta energia quanto armazena: a eficiência de carga é intrinsecamente
  **~50 %**. Somando com o TPS63031, a eficiência de ida e volta fica em torno de
  **~32 %** (25 J da USB → ~8 J entregues à carga). É um número honesto e
  interessante de publicar, e é o preço de não ter bateria.
- **Com V_in = 5,0 V da USB e alvo de 5,0 V, o carregador termina em dropout.** O
  capacitor provavelmente para em ~4,7–4,8 V, não em 5,0 V. Isso custa ~9 % da
  energia — e, por outro lado, **prolonga a vida do supercapacitor**, que
  envelhece rápido no limite da tensão nominal. Medir a tensão final de fato
  alcançada; não assumir 5,0 V.

### 5.4 O enquadramento para o artigo 2

Para calibrar a expectativa: a bateria Li-Ion de 400 mAh @ 3,7 V do artigo
armazena 5328 J. O supercapacitor armazena 12,5 J — **1/426**. A autonomia cai
de 238 h para ~25 min.

Ou seja, o argumento do artigo 2 **não pode ser autonomia**. Tem de ser:
carga em menos de um minuto, ausência de química (sem envelhecimento por ciclo,
sem risco térmico, sem descarte especial), centenas de milhares de ciclos, e
**isolamento galvânico durante a medição** — com a USB desconectada, a placa
flutua, o que resolve de graça o problema de laço de terra que o próprio
protocolo do Robert pede para tratar com "módulos isolados" (§9.2).

---

## 6. O protocolo do Robert: mapeamento e correções

### 6.1 Como os 8 ensaios dele se mapeiam no que existe

| # | Ensaio do protocolo | Situação |
|---|---|---|
| 1 | Consumo alimentado por bateria | **80 % feito.** Bancada PPK2 completa em `power_profiling/`, 9 estados, relatório em PDF. Falta rodar na placa nova e separar por subsistema |
| 2 | Desenvolvimento e validação do circuito de simulação de sinal sEMG | **Parcial.** O rig existe (DAC do STM32 + condicionamento), mas **nunca foi caracterizado como instrumento** — sem erro de amplitude, offset, RMS, resposta em frequência, SNR ou distorção |
| 3 | Sistema de alimentação por supercapacitor | **Novo integral** |
| 4 | Consumo alimentado por supercapacitor | **Novo integral** |
| 5 | Comparação com equipamento clínico de referência | **Refazer.** É o braço A + B + C do artigo, agora sobre a placa nova e com base de tempo válida |
| 6 | PCB integrada | Trabalho do Robert |
| 7 | Case (manufatura aditiva) | Trabalho do Robert |
| 8 | Validação final do sistema integrado | Depois de 6 e 7 |

### 6.2 Quatro coisas para acertar no documento dele

**(1) As fórmulas de energia estão sem o fator ½** (§5.1). Dobram a autonomia.

**(2) A arquitetura da §7.2 não é a da placa.** O diagrama de blocos mostra
`Storage → PMU (BQ25570) → LDO → BLE/uC`, e o texto fala em "BQ25570 ou
BQ25173". A placa final usa **BQ25173** (carregador linear) e, sobretudo,
**TPS63031 (buck-boost)**, não um LDO. A distinção não é cosmética: com um
supercapacitor caindo de 5 V para 2 V, **um LDO jamais sustentaria 3,3 V** — a
topologia buck-boost é o que torna o projeto viável. E o BQ25570 é um colhedor de
energia com MPPT, função completamente diferente de um carregador linear.
Atualizar o diagrama antes de virar figura de artigo.

**(3) O Ensaio 5 (§9.2) pede caracterização individual por subsistema, e o
firmware não permitia.** O README registra: *"o firmware nunca para a aquisição,
então não existe um estado 'só rádio' ou 'só ADC'"*. Isso **melhorou por dois
lados**:

- O power-down do ADS112C04 quando desconectado já foi implementado e validado
  em hardware. Isso dá o estado "só rádio" (ADVERTISING, 1,54 mA) contra "rádio +
  ADC" (CONNECTED, 2,49 mA) — e é assim que a aquisição foi atribuída em
  0,95 mA, medida direta em vez de inferida.
- A placa nova tem **jumpers de 0 Ω** que fecham o resto: remover `R32` e inserir
  o amperímetro isola o **módulo MCU**; remover `R28` mede **MCU + AFE**; a
  diferença é o **front-end analógico**. `R27` mede o total drenado do
  supercapacitor. Pela primeira vez a atribuição por subsistema fica direta.

**(4) Falta o ensaio mais importante do hardware novo:** a interação
alimentação → qualidade de sinal (§3.3c). Proponho acrescentar um **Ensaio 2b**:
espectro de ruído do trilho de 3,3 V e seu acoplamento na banda 30–400 Hz, com
`PS_SYNC` aterrado (power-save) *versus* em nível alto (PWM forçado), e em três
pontos da descarga do supercapacitor (5,0 V / 3,5 V / 2,5 V). É a medição que
decide se a placa nova preserva a SNR de 9,0 dB — e, se não preservar, ela já
aponta a correção.

---

## 7. Materiais por ensaio

### 7.1 O que já existe

| Item | Onde |
|---|---|
| Nordic Power Profiler Kit II (PCA63100) | em mãos |
| J-Link + SEGGER Embedded Studio | em mãos |
| Placa sEMG v2.0 (a do artigo) | em mãos |
| Rig ESP32 + ADS112C04 externo | `emg_clinical/` |
| STM32 com DAC + circuito de condicionamento | em mãos |
| PC com BLE funcionando via `bleak` | em mãos |
| Bancada de medição em Python (9 estados, relatório em PDF) | `power_profiling/` |
| Cliente BLE que decodifica os blocos de 60 amostras | `power_profiling/ble_client.py` |
| Leitor de contadores do firmware por J-Link | `power_profiling/fw_counters.py` |

### 7.2 O que falta comprar/pegar

| Item | Para qual ensaio | Observação |
|---|---|---|
| **Placa nova do Robert, montada** | todos | com o KVR-5R0C105-R populado |
| Supercapacitor de reserva (1–2 un.) | 3, 4 | morrem se abusados na tensão nominal |
| Ferro de solda ponta fina, pinça, fluxo, malha dessoldadora | 1, 4 | para os jumpers 0 Ω R27/R28/R32 |
| Headers de 2 pinos ou fio esmaltado | 1, 4 | para inserir a PPK2 nos jumpers |
| Resistores 0805 (4,7 k, 0 Ω, kit de divisor) | 2, 4 | re-trim do atenuador e das correções de §4 |
| Osciloscópio com memória longa | 2, 2b, 3, 5 | **provavelmente do laboratório** |
| Ponta diferencial (ou 2 canais + math) | 2b | ripple do 3V3 sem laço de terra |
| Fonte de bancada 5 V ou USB com medição | 3 | a PPK2 em modo ampere também serve (até 1 A) |
| Multímetro | 1, 3, 4 | |
| Sistema EMG clínico de referência | 5 | **laboratório** |
| Cabos blindados, jacarés, protoboard | 2, 5 | |
| Isolador USB (tipo ADuM3160) | 5 | se aparecer laço de terra na sessão simultânea |
| Eletrodos Ag/AgCl + gel | — | **só se houver braço com voluntário** — o protocolo do artigo usa reprodução por DAC, então não há comitê de ética envolvido |

### 7.3 Trabalho de firmware necessário

Não é opcional: alguns ensaios não são executáveis sem isso.

| # | O quê | Habilita | Prioridade |
|---|---|---|---|
| 1 | Tornar o keep-alive de P1.11 **explícito** (hoje é herança do workaround da UART) + capacitor em `R31` (§4.1) | operar a placa sem ela se desligar em resets | **bloqueante** |
| 2 | Ler `VCAP` (P0.03) pelo SAADC e expor por BLE | Ensaios 3 e 4 **sem instrumento externo** — e é feature de produto | **alta** |
| 3 | Corrigir o no-op da escrita de ganho (`gain_level = new_gain` em `ble_emg_service.c`) | varredura de ganho nos Ensaios 2 e 5 | **alta** |
| 4 | Número de sequência no pacote | "perda de dados" que o protocolo pede (§12.4) | média |
| 5 | Marcadores de estado em GPIO para a porta lógica da PPK2 (pads livres: P0.02, P1.14, P1.15) | fronteiras de banda com precisão de µs no mesmo stream | média |
| 6 | Habilitar o LED verde (P1.12, hoje forçado alto pelo workaround) | indicação de estado; o brilho parasita já não existe a 3,3 V | baixa |
| 7 | Reavaliar o DC/DC interno do nRF (`DCDCEN`) | ~10 % de consumo, e agora a energia é escassa | baixa — confirmar antes se o módulo XIAO tem o indutor no pino DCC |

E o mais importante para o Ensaio 5: **substituir a coleta ASCII por serial** do
braço do clínico (§2.1). Três opções, em ordem de preferência:

1. **Osciloscópio com memória longa** digitalizando a saída analógica do clínico
   com Fs conhecida e registrada. É a opção mais defensável — nenhuma dúvida sobre
   base de tempo.
2. **Saída digital do próprio clínico**, se ele tiver. Elimina o problema.
3. **Reescrever o rig do ESP32** para amostragem governada pela interrupção de
   DRDY# e transporte binário com contador de sequência — a mesma arquitetura que
   consertou o firmware do nRF. Mais trabalho, e ainda seria o elo mais fraco.

---

## 8. Ordem de execução

Ordenada para que cada fase reduza o risco da seguinte, e para deixar a única
fase que depende do laboratório para o fim — quando a placa já estiver estável.

| Fase | O quê | Onde | Duração | Depende de |
|---|---|---|---|---|
| **0** | Bring-up: checklist da §4, gravar firmware, confirmar I²C a 400 kHz, medir a janela de boot da trava | mesa | 1 dia | placa do Robert |
| **1** | Ensaio 1 refeito: PPK2 na placa nova a 3,3 V, 9 estados, **e a separação por subsistema pelos jumpers R28/R32** | mesa | 1–2 dias | fase 0 |
| **2** | Ensaio 3: carga do supercapacitor — I_chg real, tempo de carga, tensão final alcançada, dissipação do BQ25173 | mesa | 1 dia | fase 0 + firmware #2 |
| **3** | Ensaio 4: descarga e autonomia — curva V_CAP(t) por estado, **tensão de corte real** (§4.4), autonomia medida contra a estimada da §5.2 | mesa | 1 dia | fase 2 |
| **4** | **Ensaio 2b (novo)**: ruído do trilho e acoplamento na banda, `PS_SYNC` aterrado vs. alto, em 3 pontos da descarga | mesa | 1 dia | fase 1, osciloscópio |
| **5** | Ensaio 2: caracterizar o simulador como instrumento + **resposta em frequência do sensor** (não precisa do clínico: gerador de sinal basta) + re-trim do DS3502 para o novo teto (§3.3a) | mesa | 2 dias | fase 4, firmware #3 |
| **6** | **Ensaio 5: sessão no laboratório** — reprodução do dataset gravada simultaneamente pelo clínico e pela placa nova, com base de tempo registrada nos dois braços | **laboratório** | 1 sessão (meio dia) | fases 0–5 |
| **7** | Recomputar A × B × C: SNR, envelope, resposta em frequência, com o eixo de tempo correto | mesa | 1–2 dias | fase 6 |
| **8** | Ensaios 6, 7, 8 (PCB integrada, case, validação final) | Robert | — | — |

**Só a fase 6 precisa do laboratório.** Tudo o mais é bancada de mesa — inclusive
a resposta em frequência do sensor, que no artigo saiu junto da comparação
clínica mas na verdade só precisa de um gerador de sinal.

### Uma economia possível

Se o Robert tiver guardadas as **gravações brutas da sessão de *sidekicking***
(as que geraram as Fig. 5 e 6) **com a taxa de amostragem registrada**, dá para
fazer um pré-teste sem ir ao laboratório: reproduzir o mesmo trecho pelo mesmo
DAC para dentro da placa nova, na mesa, e comparar contra a gravação antiga do
clínico. Não é simultâneo, e isso tem de ficar declarado — mas derrisca a sessão
de laboratório inteira por algumas horas de bancada.

**Vale perguntar a ele antes de agendar o laboratório.**

---

## 9. Perguntas para o Robert

1. Existem as **gravações brutas do *sidekicking*** (os três braços das Fig. 5 e
   6)? E foi registrada a **taxa de amostragem real** de cada braço?
2. `R32` (VCC → pino `5V`) e `R37` (pino `3V3`) são **opções mutuamente
   exclusivas** de alimentação do módulo? (§4.2)
3. O comportamento de **desligar em qualquer reset** (§4.1) é intencional?
4. O `KVR-5R0C105-R` é mesmo **1 F / 5,0 V**? E o `R25` = 10 kΩ do ISET
   corresponde a que corrente de carga no BQ25173?
5. O diagrama da §7.2 do protocolo (BQ25570 + LDO) vai ser atualizado para o que
   a placa realmente é (BQ25173 + TPS63031)? (§6.2-2)
6. O `PS_SYNC` aterrado foi escolha deliberada, ou default? (§3.3c)
7. O equipamento clínico do laboratório tem **saída digital**, ou o único acesso
   é a saída analógica?

---

## Referências internas

- [`README.md`](README.md) — firmware atual, divergências com o artigo, consumo medido
- [`power_profiling/PLANO.md`](power_profiling/PLANO.md) — metodologia da bancada PPK2
- [`power_profiling/PENDENTE_ENSAIO_2.md`](power_profiling/PENDENTE_ENSAIO_2.md) — resultados e pendências do Ensaio 2
- [`power_profiling/relatorio_consumo.pdf`](power_profiling/relatorio_consumo.pdf) — caracterização completa
- `.claude/skills/ppk2-power-profiling/` — fiação e scripts de medição
- `.claude/skills/build-flash-nrf52/` — build e gravação pela CLI da SEGGER
