# stm32_dac_player — estímulo sEMG por DAC

Firmware para **NUCLEO-H563ZI** e **NUCLEO-F767ZI** que toca no DAC trechos do
dataset do artigo (8 ações, de Sidekicking a Standing), para os ensaios de
qualidade de sinal ([TRES_ENSAIOS_SINAL.pdf](../docs/TRES_ENSAIOS_SINAL.pdf) §3).
A saída vai para o
[circuito de condicionamento](../docs/CIRCUITO_CONDICIONAMENTO.pdf) (LM358 → 1:501 → 5,59 mV pp)
e de lá para a placa sEMG e para o EMG clínico.

A placa **liga e já toca o laço**. A CLI pela serial do ST-LINK e o botão azul
trocam de modo; no laboratório não precisa de PC.

## Ligação

| Sinal | Pino | Nucleo-144 (Zio) | Para onde |
|---|---|---|---|
| **Estímulo** (DAC canal 2) | **PA5** | CN7 pino 10 (**D13**) | J1 "DAC" do condicionamento — **é este que se mede** |
| Sincronismo (DAC canal 1) | PA4 | CN7 pino 17 (D24) | gatilho do osciloscópio (opcional) |
| Terra | GND | qualquer GND | GND comum da bancada |

> **O estímulo é o PA5, não o PA4.** Na NUCLEO-H563ZI o PA4 está amarrado ao
> **VBUS_SENSE** pela ponte de solda **SB56** (ele é o ADC1_INP18 do sense de
> USB) — o divisor carrega a saída e aparecem ~660 mV parasitas quando o USB de
> usuário está ligado. O suporte desta placa no Zephyr usa `dac1_out2_pa5` pelo
> mesmo motivo. Sobrou para o PA4 o pulso de sync, que é só gatilho e aguenta a
> carga, **desde que o USB de usuário fique desconectado** — o que a bancada do
> ensaio já exige. Abrir o SB56 libera o PA4 por completo.

O LM358 do condicionamento é alimentado em **5 V** (o pino 5V da Nucleo serve),
não em 3,3 V. Confira a serigrafia antes de soldar (UM3115 para a H563ZI,
UM1974 para a F767ZI).

O **sync** sai pelo outro canal do DAC, na mesma palavra de DMA do estímulo,
então cai na amostra exata — sem o jitter de um GPIO acionado por interrupção.
São **(índice do segmento + 1) pulsos de 10 ms** no início de cada laço, então a
própria gravação do osciloscópio diz qual ação estava tocando, sem depender de
contar laços desde o início da sessão. O alinhamento fino entre os três braços
continua vindo do marcador de 100 Hz, que está dentro do próprio sinal.

## Uso rápido

```powershell
python stm32_dac_player\tools\fetch_dataset.py     # baixa o dataset da UCI e confere o SHA-256
python stm32_dac_player\tools\make_stimulus.py     # (opcional) regera a tabela; a versionada já está pronta
powershell -File stm32_dac_player\tools\flash.ps1 -Board h563zi
```

O `flash.ps1` compila e copia o `.bin` para o drive USB que o ST-LINK monta
(`NOD_H563ZI` / `NODE_F767ZI`). Não é preciso instalar nada da ST. Se o
`STM32_Programmer_CLI` estiver no PATH, `-Programmer` usa ele no lugar.

Para só compilar: `powershell -File stm32_dac_player\tools\build.ps1 -Board f767zi`.

### Gravação via WSL (quando o drive do ST-LINK não monta)

Em máquina com política corporativa de bloqueio de armazenamento removível, o
drive USB do ST-LINK aparece no Windows mas dá "acesso negado" (confirmado
nesta máquina) — `flash.ps1` não funciona. Alternativa sem instalar nada da
ST, pela interface de debug (SWD) do ST-LINK via WSL:

```powershell
# uma vez: compartilha o ST-LINK com o WSL (pede UAC)
usbipd bind --busid <busid-do-ST-LINK>      # usbipd list mostra o busid (VID 0483:374e)
usbipd attach --wsl --busid <busid>         # sem admin; repetir depois de reboot/replug
# ou: powershell -File stm32_dac_player\tools\wsl_attach_stlink.ps1
```
```bash
# dentro do WSL (uma vez): pyocd + o pacote CMSIS do alvo
pip3 install --user --break-system-packages pyocd
pyocd pack install stm32h563zitx      # ou stm32f767zitx

# a cada gravação
sudo chmod 666 /dev/bus/usb/001/00X   # ache o numero com `lsusb` (STMicroelectronics STLINK-V3); so dura até replugar
pyocd flash -t stm32h563zitx build/h563zi/stm32_dac_player_h563zi.bin --base-address 0x08000000
pyocd reset -t stm32h563zitx
```

A serial também passa para o WSL (`/dev/ttyACM0`), com o mesmo `chmod 666` ou
`sudo` se o usuário não estiver no grupo `dialout`. `usbipd bind` fica
persistido; só o `attach` precisa ser refeito depois de um reboot, sleep ou
desconectar o cabo (sem UAC).

**Pré-requisitos** (uma vez só):
- [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) 14.x para mingw-w64, extraída em `%USERPROFILE%\tools\arm-gnu-toolchain\`. Outro lugar funciona com `ARM_TOOLCHAIN_DIR`.
- `pip install --user cmake ninja`. Os scripts usam numpy, scipy, matplotlib e pyserial.

## Modos e comandos

Serial: porta do ST-LINK, 115200 8N1. `help` lista tudo.

| Comando | O que faz | Uso no ensaio |
|---|---|---|
| `loop` | laço de estímulo, recomeçando da amostra 0 (**modo de boot**) | E3 |
| `once` | toca o laço uma vez e para em silêncio | disparo sob comando |
| `sweep` | degraus de 1/3 de oitava, 10,0 → 794,3 Hz (20 degraus, IEC 61260 base 10), 2 s cada | E1 passo 6, E3 passo 7 |
| `sine <Hz>` | senoide fixa | E1 passos 2–4 |
| `silence` | meia escala, parado | E1 passo 5, E3 passo 3 |
| `code <n>` | código DC fixo | conferir o DAC no multímetro |
| `amp <n>` / `amp 1400mv` / `amp -20db` | amplitude de pico em códigos, mV na saída do DAC ou dBFS (0 dB = 2047) | E1 passos 2 e 4 |
| `mid <n>` | código do centro (padrão 2048) | |
| `seg` | lista as 8 ações com origem e SNR | |
| `seg <n\|nome>` | escolhe a ação (`seg 4`, `seg Walking`) | E3 |
| `seg all` | cicla pelas 8, uma por laço (60 s por volta) | E3 — uma gravação pega tudo |
| `status` / `info` | modo, ação e amplitude / placa, relógio, tabela, SHA-256 e CRC | caderno de bancada |
| `events on` | imprime cada início de laço ou degrau | |

A amplitude padrão é **1738 códigos = ±1,400 V** (85 % dos códigos, TRES_ENSAIOS §2).
Trocar a amplitude não recomeça o laço; trocar de modo recomeça.

- **Botão azul:** loop → sweep → sine 100 Hz → silence → loop.
- **LED verde:** aceso = loop, 2 Hz = sweep, 1 Hz = sine, 4 Hz = once tocando, apagado = silêncio.
- **LED vermelho:** algo errado. Ou o HSE não subiu (caiu no HSI, base de tempo ±1 %, não serve para ensaio), ou o CRC da tabela não confere, ou houve underrun de DMA. O `info` diz qual.

`tools/player_cli.py` manda comandos por script. `--linearity` faz o passo 4
do E1: dez amplitudes de −40 a −3 dBFS, com o instante de cada troca.

## O laço de estímulo (7,5 s @ 8 kS/s)

Os tempos são redondos de propósito — dá para cravar cursor no osciloscópio e
para fatiar em script sem procurar borda:

| Intervalo | Conteúdo | Para quê |
|---|---|---|
| 0,000–0,200 s | silêncio | aqui vive o trem de pulsos de sync |
| 0,200–0,230 s | 3 ciclos de 100 Hz | marcador de alinhamento por correlação cruzada |
| 0,230–2,000 s | **zona morta (0 V)** | guarda antes da amostra, e o piso de ruído medido |
| **2,000–6,000 s** | **a amostra**, pico normalizado a ±1,0 | o sinal do ensaio |
| 6,000–7,500 s | **zona morta (0 V)**, 1,5 s | guarda depois da amostra → repete |

A amostra abre exatamente **2,000 s** depois do primeiro pulso de sync e fecha
exatamente em **6,000 s**, com 1,5 s de zona morta de cada lado. "Silêncio" é o
código de meia escala no DAC, que depois do capacitor de acoplamento vira **0 V
nos terminais** — então na gravação dos dois sistemas o início e o fim da
amostra ficam entre dois trechos chatos e inconfundíveis de 0 V.

> **O chirp saiu.** O layout do [TRES_ENSAIOS_SINAL](../docs/TRES_ENSAIOS_SINAL.pdf) §3
> tinha também uma varredura de 20→500 Hz em 1,3–1,8 s, como "verificação rápida
> de banda". Era redundante com o modo `sweep` (degraus de 1/3 de oitava, 2 s por
> frequência), que é a medida de resposta em frequência de verdade dos ensaios E1
> passo 6 e E3 passo 7 — e comia ~1 s de cada laço. Sem ela a amostra passou de
> 62 % para 53 % do laço, mas em troca ganhou 1,5 s de zona morta de cada lado.

![laço](stimulus/stimulus_loop.png)

A tabela é gerada offline por [`tools/make_stimulus.py`](tools/make_stimulus.py):
- a média é removida;
- reamostragem de 1 kS/s para 8 kS/s com `scipy.signal.resample_poly`;
- rampas de 10 ms nas bordas;
- o pico é normalizado depois de reamostrar.

O preâmbulo (0–2,000 s) é igual para todas as ações, então vai para a flash uma
vez só; a zona morta de saída o firmware gera na hora. Assim 8 segmentos custam
**531 kB** em vez de 960 kB — 28 % da flash de 2 MB.

O script grava a tabela em [`common/stimulus_table.c`](common/stimulus_table.c), mais a
referência para a análise ([`stimulus/reference.npz`](stimulus/reference.npz) com todos os
arrays, e um `.wav` por ação em [`stimulus/wav/`](stimulus/wav/)) e os parâmetros com o
hash em [`stimulus/stimulus.json`](stimulus/stimulus.json).

**Tabela atual:** SHA-256 `e9625fae6b306603f6e4c4097cab93c2ae003a7e6422762006ea6d9783d84094`, CRC32 `0x16ed33e5`.
O `info` mostra os dois e confere o CRC na flash a cada boot. Anote o SHA no
caderno de bancada. Regerar a tabela invalida a comparação entre sessões.

## Qual trecho do dataset, e por quê

**Dataset:** UCI *EMG Physical Action Data Set* (Theodoridis 2011, DOI 10.24432/C53W49, ref. [22] do artigo).
É um Delsys de 8 canais. *Não* é Trigno: o Trigno aparece no artigo só como referência de consumo.

Três fatos não estão no readme da UCI; foram verificados nos arquivos:
- a taxa é **1000 S/s** (os `.log` têm Start/End: cerca de 9 800 amostras em 9 s);
- a unidade é µV;
- o sinal ceifa em ±4000 µV.

**O trecho exato da Fig. 5 não é recuperável.** O envelope da figura foi
digitalizado e correlacionado com todas as janelas de 4 s de todos os 80
arquivos × 8 canais, em quatro escalas de tempo. Nada passou de r = 0,66, que é
nível de acaso: o "Original Signal" do artigo foi pré-processado de um jeito que
não ficou registrado. Por isso a escolha é por critério, reprodutível em
[`tools/pick_segment.py`](tools/pick_segment.py), com o resultado versionado em
[`stimulus/segments.json`](stimulus/segments.json):

1. No máximo 0,5 % de amostras ceifadas (±4000 µV).
2. Entre 8 e 18 rajadas pelo método do artigo.
3. Maior SNR, ou — para o Sidekicking — a SNR mais perto dos **10,7 dB**
   publicados para o sinal original.
4. O **sub2 fica fora**: a UCI avisa que ele não foi filtrado.

**19 das 20 ações** do dataset têm trecho que passa. A única fora é **Punching**,
cuja janela mais limpa tem 0,60 % de ceifamento.

### As 8 ações escolhidas

O artigo testou **uma** só (Sidekicking, agressivo, 10,7 dB). O conjunto aqui
cobre de propósito a faixa dinâmica que ficou sem testar — de 7,6 dB (sinal
fraco, caso difícil para o piso de ruído) a 21,1 dB:

| # | Ação | Grupo | Origem | SNR | Ceifado |
|---|---|---|---|---|---|
| 0 | Sidekicking | Aggressive | sub4 R-Thi, t=4,90 s | 10,7 dB | 0,15 % |
| 1 | Elbowing | Aggressive | sub4 R-Tri, t=5,50 s | 11,1 dB | 0,10 % |
| 2 | Frontkicking | Aggressive | sub1 R-Bic, t=5,50 s | 13,9 dB | 0 |
| 3 | Slapping | Aggressive | sub1 L-Tri, t=5,00 s | 14,5 dB | 0 |
| 4 | Walking | Normal | sub4 R-Bic, t=1,50 s | 12,1 dB | 0 |
| 5 | Handshaking | Normal | sub4 R-Ham, t=0,75 s | 21,1 dB | 0 |
| 6 | Clapping | Normal | sub4 R-Bic, t=1,50 s | 14,9 dB | 0 |
| 7 | Standing | Normal | sub4 R-Tri, t=0,25 s | 7,6 dB | 0 |

O índice **0 é o Sidekicking** de propósito: 1 pulso de sync, igual ao
comportamento de antes de existirem vários segmentos.

Para trocar o conjunto, edite [`stimulus/segments.json`](stimulus/segments.json)
e rode `make_stimulus.py` de novo (o SHA-256 muda — anote no caderno). Os
candidatos de Sidekicking que ficaram de reserva estão em
[`stimulus/candidates.png`](stimulus/candidates.png).

Se as gravações brutas da sessão de 2025 aparecerem
(ver [ENSAIOS_ARTIGO_2.md](../docs/ENSAIOS_ARTIGO_2.md)), dá para reabrir esta escolha.

## Como funciona

```
TIM6 TRGO 8 kHz ──► DAC canal 2 (PA5, estímulo) ◄── DHR12RD ◄── DMA circular ◄── pingue-pongue 2 × 256 palavras
                └─► DAC canal 1 (PA4, sync)                      (meia janela = 32 ms)   ▲
                                                                                         │ player_fill()
                                                      interrupção HT/TC ─────────────────┘ preâmbulo + segmento,
                                                                                            ou senoide / varredura
```

- A temporização é toda de hardware. A CPU só preenche a metade do buffer que o DMA acabou de esvaziar.
- A CLI e o botão postam pedidos que entram na próxima meia janela, sem trava e sem cortar amostra.
- No H5, o GPDMA conta blocos de no máximo 64 kB. Por isso o modo circular é um item de lista encadeada que aponta para si mesmo.
- **Relógio:** HSE em bypass no MCO de 8 MHz do ST-LINK. H563 a 250 MHz (TIM6 com ARR = 31249); F767 a 216 MHz (timers do APB1 a 108 MHz, ARR = 13499). As duas contas são exatas para 8 kS/s.

| Pasta | Conteúdo |
|---|---|
| `common/` | portável: `player.c` (modos), `cli.c`, `main.c`, `board.h` (interface), tabela gerada |
| `boards/h563zi/`, `boards/f767zi/` | `board.c` por registrador sobre os headers CMSIS, startup e linker script da ST |
| `vendor/` | subconjunto dos headers CMSIS da ST (cmsis-core, cmsis-device-h5 `884b8dc`, cmsis-device-f7 `2352e88`), licenças junto |
| `tools/` | dataset, escolha do trecho, geração da tabela, build/flash, CLI por script |
| `tests/` | teste de host do player e da CLI: `python stm32_dac_player/tests/run_host_tests.py` (precisa de `pip install ziglang`) |

O teste de host confere amostra a amostra, contra a tabela, dois laços inteiros
de **cada uma das 8 ações** — estímulo e sync. Também cobre a contagem de pulsos
de sync por segmento (e que ela termina antes do marcador), o `seg all` ciclando
e voltando ao 0, o `once`, a frequência de 12 degraus da varredura, a amplitude
da senoide e o parser da CLI.

## Verificação na bancada (H563ZI)

1. `info`: o SHA bate com `stimulus/stimulus.json`, o CRC dá `OK` e o relógio aparece como `HSE`.
2. `silence` com multímetro em **PA5**: cerca de 1,65 V.
3. `sine 100`, multímetro em AC na saída do seguidor. Ajuste `amp` até ±1,400 V de pico (≈ 0,990 V RMS) e anote o código. Esperado: perto de 1738.
4. Sync em PA4 no osciloscópio: período de **7,500 s** no `loop` e de 2,000 s no `sweep`. Conte os pulsos — devem ser (índice do segmento + 1).
5. Com o cursor no primeiro pulso de sync: a amostra abre em **+2,000 s** e fecha em **+6,000 s**, com 0 V antes e depois.
6. Botão cicla os modos, e o LED verde acompanha.

### Estado da bancada

> **Pendente:** a placa ainda está com o firmware de 2026-10-06 (tabela
> `b145c17c…`, laço de 6,5 s com o chirp, estímulo no PA4). O firmware atual —
> 8 ações, laço de 7,5 s com zona morta, estímulo no **PA5** — compila e passa
> os testes de host, mas **ainda não foi gravado**: a Nucleo saiu do USB. Grave
> pela via WSL/pyocd antes da sessão e confira o SHA no `info`.

O que a sessão de 2026-10-06 já confirmou na placa, e continua valendo
(gravada pela via WSL/pyocd acima, porque o drive do ST-LINK não monta nesta
máquina):

```
board    NUCLEO-H563ZI  SYSCLK 250 MHz  clock HSE (MCO 8 MHz do ST-LINK)
crc32    esperado cb4abf10 calculado cb4abf10  OK
```

O relógio sobe em HSE (cristal do ST-LINK, não o HSI de reserva), o CRC da
tabela gravada confere, e `sine 100` e `sweep` responderam certo (o degrau
avançou 10,00 → 12,59 Hz nos 2 s esperados). **Os passos 2–5 acima (amplitude
no multímetro, período e pulsos no osciloscópio) ainda não foram feitos** —
exigem instrumento físico na bancada.

Nota: o contador de `loops`/`uptime` do `status` zera a cada reset da MCU —
reparado que um `usbipd detach`/`attach` no host chega a pulsar o reset do
alvo pelo ST-LINK. Não é problema (a tabela na flash não muda, só reinicia o
laço do zero), mas não estranhe o contador voltar a 0 depois de manipular o
USB pelo WSL.

**Para a sessão E3 (dataset × clínico × placa, osciloscópio):** checklist
completo em [TRES_ENSAIOS_SINAL.pdf](../docs/TRES_ENSAIOS_SINAL.pdf) §E3.
Resumo do que falta montar: circuito de condicionamento
([CIRCUITO_CONDICIONAMENTO.pdf](../docs/CIRCUITO_CONDICIONAMENTO.pdf)) entre
**PA5** e os terminais de injeção dos dois sistemas, LM358 em 5 V, osciloscópio
como único terra da bancada (notebook na bateria, USB da placa sEMG
desconectado — e **o USB de usuário da Nucleo também, por causa do SB56**), e o
sync do PA4 no segundo canal do osciloscópio para disparo. Antes de gravar:
`silence` nos dois braços de hardware por 30 s e comparar com o piso medido em
casa (E1 passo 5) — se estiver pior, é laço de terra do laboratório, resolver
antes de gravar.

Com `seg all`, uma gravação contínua de ~10 min pega as 8 ações várias vezes
(60 s por volta completa), e a contagem de pulsos de sync diz qual é qual na
hora da análise.
