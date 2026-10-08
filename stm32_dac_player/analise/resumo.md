# Capturas do ensaio de qualidade de sinal

36 arquivos, 35 com dado utilizavel.

## Taxa de amostragem

- faixa: 200 a 370 S/s (mediana 200)
- para a banda de 20-400 Hz o minimo teorico e 800 S/s
- capturas que atendem: **0 de 35**
- com 200 S/s o Nyquist e 100 Hz, e 79 % da banda cai acima dele
- **se cada arquivo tem ~2000 pontos, a taxa baixa e do SALVAMENTO, nao da aquisicao**: 2000 e o registro de tela do InfiniiVision. Nos prints da sessao de 2026-10-07 o Agilent DSO-X 2012A estava adquirindo a 2,5 kSa/s. Ao salvar em CSV, por o comprimento em Max dentro de Definicoes traz a memoria inteira, e o arquivo passa a ter ~18 mil linhas para os 7,5 s do laco.

## Periodo do laco

- medido em 3 captura(s) com dois marcadores: **7.5000 s** (desvio 0.0000 s), nominal 7.500 s


## Ganho do clinico, inferido dos dados

- atenuacao informada: 1:213.8
- razao mediana entre os canais (n=32): 1.4372
- **ganho implicito 307x**
- serve de conferencia contra o ajuste do aparelho: se nao casar com nenhum valor da tabela de ganhos, ou a atenuacao ou o ajuste esta diferente do anotado


## Nivel nos terminais (ganho do clinico = 300x)

- mediana **10.44 mV pp**, faixa 0.40 a 34.04
- projeto do atenuador 1:501 (CIRCUITO_CONDICIONAMENTO): **5,59 mV pp**
- faixa fisiologica de sEMG citada no artigo: pp abaixo de 6 mV
- razao medido/projeto: **1.87x**

## Problemas por captura

- truncadas (< 50 pontos): scope_17.csv
- sem marcador na janela: 18 de 35
- com ceifamento: scope_0.csv, scope_1.csv, scope_5.csv, scope_6.csv, scope_7.csv, scope_27.csv, scope_28.csv, scope_30.csv

## Tabela

| arquivo | S/s | janela (s) | estim. | repouso V | marcador(es) | pp estim. | SNR outro | ceifa |
|---|---|---|---|---|---|---|---|---|
| scope_0.csv | 370 | -2.70..2.70 | nao identificado | — | — | 0.158 | — | sim |
| scope_1.csv | 370 | -2.70..2.70 | nao identificado | — | — | 0.161 | — | sim |
| scope_4.csv | 312 | 0.15..6.55 | nao identificado | — | — | 0.021 | — | nao |
| scope_5.csv | 200 | -4.98..5.01 | ch2 | 1.493 | — | 2.200 | — | sim |
| scope_6.csv | 200 | -4.98..-0.04 | ch2 | 1.573 | — | 2.312 | — | sim |
| scope_7.csv | 200 | -4.98..2.90 | ch2 | 1.573 | — | 2.324 | — | sim |
| scope_8.csv | 200 | 1.50..11.49 | ch2 | 1.613 | 2.860;10.360 | 2.182 | 16.03 | nao |
| scope_9.csv | 200 | 1.50..10.47 | ch2 | 1.613 | 8.195 | 2.211 | — | nao |
| scope_10.csv | 200 | 1.50..1.80 | ch2 | 1.613 | — | 0.040 | — | nao |
| scope_11.csv | 200 | 1.50..11.35 | ch2 | 1.613 | — | 2.322 | — | nao |
| scope_12.csv | 200 | 1.50..10.27 | ch2 | 1.613 | 1.700;9.200 | 2.273 | 16.53 | nao |
| scope_13.csv | 200 | 1.50..10.43 | ch2 | 1.613 | — | 2.312 | — | nao |
| scope_14.csv | 200 | 1.50..11.49 | ch2 | 1.613 | 6.015 | 2.313 | — | nao |
| scope_15.csv | 200 | 1.50..8.71 | ch2 | 1.613 | 8.085 | 2.332 | — | nao |
| scope_16.csv | 200 | 1.50..8.45 | ch2 | 1.513 | 7.500 | 2.313 | — | nao |
| scope_17.csv | — | — | — | — | — | — | — | captura truncada (5 pontos) |
| scope_18.csv | 200 | 1.50..11.09 | ch2 | 1.603 | 2.715 | 2.303 | 15.37 | nao |
| scope_19.csv | 200 | 1.50..9.96 | ch2 | 1.593 | 2.225 | 2.372 | 4.65 | nao |
| scope_20.csv | 200 | 1.50..11.49 | ch2 | 1.593 | 5.505 | 2.371 | — | nao |
| scope_21.csv | 200 | 1.50..6.79 | ch2 | 1.593 | — | 2.151 | — | nao |
| scope_22.csv | 200 | 1.50..7.32 | ch2 | 1.593 | — | 2.109 | — | nao |
| scope_23.csv | 200 | 1.50..6.87 | ch2 | 1.593 | — | 2.121 | — | nao |
| scope_24.csv | 200 | 1.50..8.49 | ch2 | 1.593 | — | 2.122 | — | nao |
| scope_25.csv | 200 | 1.50..11.49 | ch2 | 1.593 | 6.135 | 2.362 | — | nao |
| scope_26.csv | 200 | 1.50..6.78 | ch2 | 1.593 | — | 2.271 | — | nao |
| scope_27.csv | 200 | 1.50..10.87 | ch2 | 1.593 | 10.170 | 2.352 | — | sim |
| scope_28.csv | 200 | 1.50..10.35 | ch2 | 1.593 | 1.515;9.015 | 2.352 | 17.07 | sim |
| scope_29.csv | 200 | 1.50..10.59 | ch2 | 1.613 | — | 2.332 | — | nao |
| scope_30.csv | 200 | 1.50..9.18 | ch2 | 1.613 | 8.370 | 2.313 | — | sim |
| scope_31.csv | 200 | 1.50..8.72 | ch2 | 1.613 | — | 2.131 | — | nao |
| scope_32.csv | 200 | 1.50..8.46 | ch2 | 1.613 | 7.890 | 2.392 | — | nao |
| scope_33.csv | 200 | 1.50..7.05 | ch2 | 1.613 | — | 1.750 | — | nao |
| scope_34.csv | 200 | 1.50..9.43 | ch2 | 1.613 | 8.760 | 2.030 | — | nao |
| scope_35.csv | 200 | 1.50..9.52 | ch2 | 1.613 | 8.150 | 1.839 | — | nao |
| scope_36.csv | 200 | 1.50..7.94 | ch2 | 1.613 | 7.705 | 2.382 | — | nao |
| scope_37.csv | 200 | 1.50..8.40 | ch2 | 1.613 | — | 2.301 | — | nao |
