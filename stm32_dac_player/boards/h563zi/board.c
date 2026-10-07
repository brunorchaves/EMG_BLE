/*
 * NUCLEO-H563ZI (MB1404): STM32H563ZIT6, Cortex-M33 a 250 MHz.
 *
 *   PA5  DAC1_OUT2  ESTIMULO         CN7 pino 10 (Zio D13)  <- e este que se mede
 *   PA4  DAC1_OUT1  sync             CN7 pino 17 (Zio D24)
 *        PA4 e VBUS_SENSE nesta placa (SB56, ADC1_INP18). Serve de gatilho de
 *        osciloscopio com o USB de usuario desconectado; abrir SB56 o libera.
 *   PB0  LD1 verde  status
 *   PG4  LD3 vermelho erro
 *   PC13 B1 USER (ativo alto)
 *   PD8/PD9 USART3 = VCP do STLINK-V3EC, 115200 8N1
 *
 * Relogio: HSE em bypass digital no MCO de 8 MHz do ST-LINK, PLL1 =
 * 8/4*250/2 = 250 MHz em VOS0 (o mesmo do SystemClock_Config dos exemplos
 * da ST para esta placa). Se o HSE nao subir, cai para HSI 64 MHz com o
 * mesmo PLL e acende o LED vermelho: a placa toca, mas a base de tempo
 * deixa de ser de cristal.
 *
 * DAC: TIM6 TRGO a 8 kS/s dispara os dois canais; GPDMA1 canal 0 escreve
 * DHR12RD (32 bits = os dois canais) a partir de um pingue-pongue em RAM.
 * O GPDMA so conta blocos de ate 64 kB, entao o modo circular e feito com
 * um item de lista encadeada que aponta para si mesmo e recarrega
 * BNDT e o endereco de origem a cada bloco.
 */
#include "stm32h5xx.h"

#include "board.h"
#include "../board_util.h"

#define SYSCLK_HZ            250000000u
#define TIM6_CLK_HZ          SYSCLK_HZ        /* APB1 /1 */
#define HSE_TIMEOUT          2000000u
#define GPDMA_REQ_DAC1_CH1   2u               /* RM0481, tabela de requisicoes do GPDMA1 */
#define DAC_TSEL_TIM6_TRGO   5u
#define USART_BAUD           115200u

static board_info_t info = {.name = "NUCLEO-H563ZI", .sysclk_hz = SYSCLK_HZ};
static volatile uint32_t ms_ticks;
static rx_ring_t rx;
static board_fill_cb_t fill_cb;
static volatile bool dma_error;

static uint32_t dma_buf[2 * BOARD_DAC_HALF_LEN];
static uint32_t dma_lli[3] __attribute__((aligned(4)));   /* CBR1, CSAR, CLLR */

/* Chamado pelo startup antes de main e antes do .data/.bss. */
void SystemInit(void)
{
    SCB->CPACR |= (3u << 20) | (3u << 22);   /* FPU */
    __DSB();
    __ISB();
}

static void clock_init(void)
{
    /* VOS0, exigido para 250 MHz */
    PWR->VOSCR = PWR_VOSCR_VOS;
    while (!(PWR->VOSSR & PWR_VOSSR_VOSRDY)) {}

    RCC->CR |= RCC_CR_HSEBYP | RCC_CR_HSEEXT;
    RCC->CR |= RCC_CR_HSEON;
    uint32_t t = HSE_TIMEOUT;
    while (!(RCC->CR & RCC_CR_HSERDY) && --t) {}
    info.hse_ok = (RCC->CR & RCC_CR_HSERDY) != 0;

    uint32_t src, m;
    if (info.hse_ok) {
        src = 3u;   /* HSE 8 MHz */
        m = 4u;
    } else {
        RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_HSEBYP | RCC_CR_HSEEXT);
        RCC->CR &= ~RCC_CR_HSIDIV;   /* HSI 64 MHz */
        while (!(RCC->CR & RCC_CR_HSIRDY)) {}
        src = 1u;
        m = 32u;
    }
    /* entrada do PLL = 2 MHz (faixa 2-4 MHz), VCO largo, N = 250, P = 2 */
    RCC->PLL1CFGR = (src << RCC_PLL1CFGR_PLL1SRC_Pos) | (1u << RCC_PLL1CFGR_PLL1RGE_Pos) |
                    (m << RCC_PLL1CFGR_PLL1M_Pos) | RCC_PLL1CFGR_PLL1PEN;
    RCC->PLL1DIVR = ((250u - 1u) << RCC_PLL1DIVR_PLL1N_Pos) | ((2u - 1u) << RCC_PLL1DIVR_PLL1P_Pos) |
                    ((2u - 1u) << RCC_PLL1DIVR_PLL1Q_Pos) | ((2u - 1u) << RCC_PLL1DIVR_PLL1R_Pos);
    RCC->CR |= RCC_CR_PLL1ON;
    while (!(RCC->CR & RCC_CR_PLL1RDY)) {}

    /* 5 wait states + WRHIGHFREQ = 2 para 250 MHz em VOS0 */
    FLASH->ACR = (5u << FLASH_ACR_LATENCY_Pos) | (2u << FLASH_ACR_WRHIGHFREQ_Pos) | FLASH_ACR_PRFTEN;
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != 5u) {}

    RCC->CFGR2 = 0;   /* AHB, APB1, APB2, APB3 = /1 */
    RCC->CFGR1 = (RCC->CFGR1 & ~RCC_CFGR1_SW) | (3u << RCC_CFGR1_SW_Pos);
    while (((RCC->CFGR1 & RCC_CFGR1_SWS) >> RCC_CFGR1_SWS_Pos) != 3u) {}

    ICACHE->CR |= ICACHE_CR_EN;
}

static void gpio_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_GPIOCEN |
                    RCC_AHB2ENR_GPIODEN | RCC_AHB2ENR_GPIOGEN;
    (void)RCC->AHB2ENR;

    gpio_mode(GPIOA, 4, GPIO_ANALOG);
    gpio_mode(GPIOA, 5, GPIO_ANALOG);
    gpio_mode(GPIOB, 0, GPIO_OUT);
    gpio_mode(GPIOG, 4, GPIO_OUT);
    gpio_mode(GPIOC, 13, GPIO_IN);
    gpio_af(GPIOD, 8, 7);
    gpio_af(GPIOD, 9, 7);
}

static void uart_init(void)
{
    RCC->APB1LENR |= RCC_APB1LENR_USART3EN;
    (void)RCC->APB1LENR;
    USART3->CR1 = 0;
    USART3->BRR = (SYSCLK_HZ + USART_BAUD / 2u) / USART_BAUD;   /* kernel = PCLK1 */
    USART3->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE_RXFNEIE | USART_CR1_UE;
    NVIC_SetPriority(USART3_IRQn, 2);
    NVIC_EnableIRQ(USART3_IRQn);
}

void board_init(void)
{
    clock_init();
    gpio_init();
    SysTick_Config(SYSCLK_HZ / 1000u);
    NVIC_SetPriority(SysTick_IRQn, 3);
    uart_init();
}

const board_info_t *board_info(void) { return &info; }

void board_dac_start(board_fill_cb_t cb)
{
    fill_cb = cb;
    cb(dma_buf, 2 * BOARD_DAC_HALF_LEN);

    RCC->AHB1ENR |= RCC_AHB1ENR_GPDMA1EN;
    RCC->AHB2ENR |= RCC_AHB2ENR_DAC1EN;
    RCC->APB1LENR |= RCC_APB1LENR_TIM6EN;
    (void)RCC->APB1LENR;

    /* TIM6: atualizacao a 8 kHz -> TRGO */
    TIM6->CR1 = 0;
    TIM6->PSC = 0;
    TIM6->ARR = TIM6_CLK_HZ / BOARD_DAC_FS_HZ - 1u;
    TIM6->CR2 = 2u << TIM_CR2_MMS_Pos;
    TIM6->EGR = TIM_EGR_UG;

    /* DAC: modo normal com buffer, interface de alta frequencia (AHB > 160 MHz) */
    DAC1->CR = 0;
    DAC1->MCR = 2u << DAC_MCR_HFSEL_Pos;
    DAC1->DHR12RD = BOARD_DAC_WORD(2048u, 0u);
    DAC1->CR = (DAC_TSEL_TIM6_TRGO << DAC_CR_TSEL1_Pos) | DAC_CR_TEN1 | DAC_CR_DMAEN1 |
               (DAC_TSEL_TIM6_TRGO << DAC_CR_TSEL2_Pos) | DAC_CR_TEN2;
    DAC1->CR |= DAC_CR_EN1 | DAC_CR_EN2;
    while ((DAC1->SR & (DAC_SR_DAC1RDY | DAC_SR_DAC2RDY)) != (DAC_SR_DAC1RDY | DAC_SR_DAC2RDY)) {}

    /* GPDMA1 canal 0: memoria (palavra, incrementa) -> DHR12RD (palavra, fixo) */
    DMA_Channel_TypeDef *ch = GPDMA1_Channel0;
    ch->CCR = DMA_CCR_RESET;
    dma_lli[0] = sizeof(dma_buf);
    dma_lli[1] = (uint32_t)dma_buf;
    dma_lli[2] = DMA_CLLR_UB1 | DMA_CLLR_USA | DMA_CLLR_ULL | ((uint32_t)dma_lli & DMA_CLLR_LA);
    ch->CLBAR = (uint32_t)dma_lli & 0xFFFF0000u;
    ch->CTR1 = (2u << DMA_CTR1_SDW_LOG2_Pos) | DMA_CTR1_SINC | (2u << DMA_CTR1_DDW_LOG2_Pos);
    ch->CTR2 = (GPDMA_REQ_DAC1_CH1 << DMA_CTR2_REQSEL_Pos) | DMA_CTR2_DREQ;
    ch->CBR1 = sizeof(dma_buf);
    ch->CSAR = (uint32_t)dma_buf;
    ch->CDAR = (uint32_t)&DAC1->DHR12RD;
    ch->CLLR = dma_lli[2];
    ch->CFCR = 0x7F00u;   /* limpa todas as flags */
    ch->CCR = DMA_CCR_HTIE | DMA_CCR_TCIE | DMA_CCR_DTEIE | DMA_CCR_ULEIE | DMA_CCR_USEIE |
              (3u << DMA_CCR_PRIO_Pos);
    NVIC_SetPriority(GPDMA1_Channel0_IRQn, 0);
    NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);
    ch->CCR |= DMA_CCR_EN;

    TIM6->CR1 = TIM_CR1_CEN;
}

void GPDMA1_Channel0_IRQHandler(void)
{
    DMA_Channel_TypeDef *ch = GPDMA1_Channel0;
    uint32_t sr = ch->CSR;
    if (sr & (DMA_CSR_DTEF | DMA_CSR_ULEF | DMA_CSR_USEF)) {
        ch->CFCR = DMA_CFCR_DTEF | DMA_CFCR_ULEF | DMA_CFCR_USEF;
        dma_error = true;
    }
    if (sr & DMA_CSR_HTF) {
        ch->CFCR = DMA_CFCR_HTF;
        fill_cb(&dma_buf[0], BOARD_DAC_HALF_LEN);
    }
    if (sr & DMA_CSR_TCF) {
        ch->CFCR = DMA_CFCR_TCF;
        fill_cb(&dma_buf[BOARD_DAC_HALF_LEN], BOARD_DAC_HALF_LEN);
    }
}

bool board_dac_underrun(void)
{
    bool u = dma_error || (DAC1->SR & DAC_SR_DMAUDR1);
    if (u) {
        DAC1->SR = DAC_SR_DMAUDR1;
        dma_error = false;
    }
    return u;
}

void board_led(board_led_t led, bool on)
{
    if (led == BOARD_LED_STATUS) gpio_write(GPIOB, 0, on);
    else gpio_write(GPIOG, 4, on);
}

bool board_button_raw(void) { return gpio_read(GPIOC, 13); }

void SysTick_Handler(void) { ms_ticks++; }
uint32_t board_millis(void) { return ms_ticks; }

void USART3_IRQHandler(void)
{
    usart_isr_rx(USART3, &rx, USART_ISR_RXNE_RXFNE, USART_ISR_ORE, USART_ICR_ORECF);
}

void board_uart_putc(char c)
{
    while (!(USART3->ISR & USART_ISR_TXE_TXFNF)) {}
    USART3->TDR = (uint8_t)c;
}

int board_uart_getc(void) { return rx_ring_pop(&rx); }

uint32_t board_irq_save(void)
{
    uint32_t p = __get_PRIMASK();
    __disable_irq();
    return p;
}

void board_irq_restore(uint32_t state) { __set_PRIMASK(state); }

void board_wait_for_interrupt(void) { __WFI(); }
