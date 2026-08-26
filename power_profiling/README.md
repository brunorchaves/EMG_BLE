# Medição de consumo com a PPK2 (PCA63100)

Este diretório reproduz a **Tabela 2** do artigo (corrente RMS e potência por
estado de operação, a 5,0 V) usando o **Power Profiler Kit II** da Nordic
(PCA63100) no lugar do shunt + osciloscópio usado originalmente.

## O que é a PCA63100

A PCA63100 é o **Power Profiler Kit II (PPK2)**: um instrumento USB da
Nordic Semiconductor para medir a corrente consumida por um circuito
("DUT" — device under test), de ~200 nA até 1 A. Tem dois modos:

- **Source Meter**: a PPK2 *alimenta* o DUT com uma tensão regulada
  (0,8–5,0 V) e mede a corrente que ele consome — substitui uma fonte de
  bancada + amperímetro/shunt em um único instrumento.
- **Ampere Meter**: o DUT é alimentado por **outra** fonte (aqui, a bateria
  + boost do EMG_BLE), e a PPK2 é inserida em série apenas para medir a
  corrente que passa por ali.

A placa se conecta ao PC só por USB (alimentação e controle/leitura). Os
terminais de parafuso `VIN` / `VOUT` / `GND` são para o **circuito medido**,
não para o PC.

## Verificado nesta máquina

- Porta serial: **COM8** (`USB\VID_1915&PID_C00A`, interface CDC ACM).
- No Windows, sem o driver oficial da Nordic instalado, a porta aparece como
  "Dispositivo Serial USB" genérico — a autodetecção da lib por nome de
  driver **não funciona**; use sempre `--port COM8`.
- Pacote `ppk2-api` (PyPI) instalado no Python do sistema
  (`C:\Users\RIBB\AppData\Local\Programs\Python\Python312`).
- Comunicação testada com sucesso: `get_modifiers()` OK, modo source a 5 V,
  início/parada de medição e leitura de amostras OK — sem nada ligado nos
  terminais VIN/VOUT/GND ainda.

## Onde ligar no circuito do EMG_BLE

Seu circuito hoje: bateria Li-Ion 3,7 V → **boost** → trilho de 5 V que
alimenta o front-end analógico + nRF52840.

### Opção recomendada — reproduzir a Tabela 2 (fonte fixa 5,0 V)

A Tabela 2 foi medida com o circuito alimentado por **5,0 V fixos**, não
pela bateria. Para reproduzir isso com a PPK2, use o modo **Source Meter**
como a própria fonte de 5 V, removendo a bateria/boost da jogada:

1. Desligue/retire a bateria (ou desconecte a saída do boost do restante do
   circuito). O boost fica sem função nesta medição.
2. Ligue o fio **VOUT** da PPK2 no ponto onde a saída do boost entrava
   (o trilho de 5 V que vai para o INA317 / ADS112C04 / nRF52840).
3. Ligue o **GND** da PPK2 no GND da placa.
4. No software, use modo *Source Meter*, 5000 mV.

### Opção complementar — consumo real com bateria (autonomia)

Se depois quiser estimar autonomia real com bateria, rompa o fio **entre o
boost e o circuito** (não entre a bateria e o boost) e insira a PPK2 em modo
**Ampere Meter** nesse ponto:

- saída + do boost → **VIN** da PPK2
- **VOUT** da PPK2 → entrada de 5 V da placa
- GND comum entre boost, PPK2 e a placa

Isso mede a corrente real no trilho de 5 V com o boost operando (inclui
ripple/eficiência do boost), ainda comparável à Tabela 2.

### Por que não medir direto no fio da bateria (3,7 V)

Ali a corrente é a que entra no boost a 3,7 V — não é o mesmo número da
Tabela 2 (o boost eleva a tensão e, por eficiência, também muda a corrente).
Só vale a pena se o objetivo for estimar a autonomia da bateria em mAh, e
mesmo assim é melhor medir depois do boost primeiro.

## Software

```
pip install ppk2-api   # já instalado nesta máquina
```

### 1. Teste de comunicação (sem nada ligado no circuito)

```
python power_profiling/ppk2_check.py --port COM8
```

### 2. Captura por estado (reproduz a Tabela 2)

```
python power_profiling/ppk2_capture.py --port COM8 --mode source --voltage-mv 5000
```

O script para em cada estado (`OFF`, `IDLE`, `CONNECTED`, `TRANSMITTING`),
pede para você levar o sensor até lá (app BLE / observando os LEDs) e
pressionar Enter, captura por 10 s (ajustável com `--duration`), e no final
grava:

- `power_profiling/results/<estado>_raw_uA.csv` — amostras brutas por estado
- `power_profiling/results/resumo_tabela2.csv` — corrente RMS (mA) e
  potência (mW) por estado, no formato da Tabela 2

Para o estado `OFF` em modo source, a própria PPK2 corta sua saída
(`toggle_DUT_power`), sem precisar desligar fio nenhum.

Para a medição com bateria/boost (Ampere Meter):

```
python power_profiling/ppk2_capture.py --port COM8 --mode ampere --voltage-mv 5000 --states IDLE,CONNECTED,TRANSMITTING
```

## Alternativa gráfica (sem escrever nada)

Se preferir não rodar script Python, instale o **nRF Connect for Desktop**
(app oficial da Nordic) e dentro dele o app **Power Profiler**. Ele mostra o
gráfico de corrente em tempo real e exporta CSV pela interface, com a mesma
fiação descrita acima. Não estava instalado nesta máquina — avise se quiser
que eu prepare/baixe.

## Correlação com os estados do firmware (opcional, próximo passo)

A PPK2 tem 8 canais digitais (D0–D7) que gravam nível lógico em paralelo com
a corrente — útil para marcar automaticamente onde cada estado
(IDLE/CONNECTED/TRANSMITTING) começa e termina, em vez de cronometrar na
mão. Isso exigiria alterar o firmware para alternar um GPIO livre a cada
transição de estado BLE. Não fiz essa alteração ainda — é uma tarefa
separada; avise se quiser que eu implemente.
