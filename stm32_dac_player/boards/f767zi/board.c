/*
 * NUCLEO-F767ZI (MB1137): STM32F767ZIT6, Cortex-M7 a 216 MHz.
 *
 *   PA4  DAC_OUT1  estimulo          CN7 pino 17 (D24)
 *   PA5  DAC_OUT2  sincronismo       CN7 pino 10 (D13)
 *   PB0  LD1 verde  status
 *   PB14 LD3 vermelho erro
 *   PC13 B1 USER (ativo alto)
 *   PD8/PD9 USART3 = VCP do ST-LINK/V2-1, 115200 8N1
 *
 * Relogio: HSE em bypass no MCO de 8 MHz do ST-LINK, PLL = 8/8*432/2 =
 * 216 MHz com overdrive (o mesmo do SystemClock_Config dos exemplos da ST
 * para esta placa). APB1 = /4 = 54 MHz, entao os timers do APB1 correm a
 * 108 MHz. Se o HSE nao subir, cai para HSI 16 MHz com M = 16 e acende o
 * LED vermelho.
 *
 * DAC: TIM6 TRGO a 8 kS/s dispara os dois canais; DMA1 Stream5 Channel7
 * escreve DHR12RD (32 bits) em modo circular sobre o pingue-pongue. Sem
 * D-cache, para o buffer de DMA nao precisar de manutencao de cache.
 */
#include "stm32f7xx.h"

#include "board.h"
#include "../board_util.h"

#define SYSCLK_HZ        216000000u
#define PCLK1_HZ         (SYSCLK_HZ / 4u)
#define TIM6_CLK_HZ      (2u * PCLK1_HZ)    /* APB1 com prescaler != 1 dobra o clock dos timers */
#define HSE_TIMEOUT      2000000u
#define DMA_CH_DAC1      7u
#define DAC_TSEL_TIM6    0u
#define USART_BAUD       115200u

static board_info_t info = {.name = "NUCLEO-F767ZI", .sysclk_hz = SYSCLK_HZ};
static volatile uint32_t ms_ticks;
static rx_ring_t rx;
static board_fill_cb_t fill_cb;
static volatile bool dma_error;

static uint32_t dma_buf[2 * BOARD_DAC_HALF_LEN];

void SystemInit(void)
{
    SCB->CPACR |= (3u << 20) | (3u << 22);   /* FPU */
    __DSB();
    __ISB();
}

static void clock_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;

    RCC->CR |= RCC_CR_HSEBYP;
    RCC->CR |= RCC_CR_HSEON;
    uint32_t t = HSE_TIMEOUT;
    while (!(RCC->CR & RCC_CR_HSERDY) && --t) {}
    info.hse_ok = (RCC->CR & RCC_CR_HSERDY) != 0;

    uint32_t src, m;
    if (info.hse_ok) {
        src = RCC_PLLCFGR_PLLSRC_HSE;
        m = 8u;
    } else {
        RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_HSEBYP);
        while (!(RCC->CR & RCC_CR_HSIRDY)) {}
        src = RCC_PLLCFGR_PLLSRC_HSI;
        m = 16u;
    }
    /* entrada do PLL = 1 MHz, N = 432, P = 2 (216 MHz), Q = 9, R = 7 */
    RCC->PLLCFGR = src | (m << RCC_PLLCFGR_PLLM_Pos) | (432u << RCC_PLLCFGR_PLLN_Pos) |
                   (0u << RCC_PLLCFGR_PLLP_Pos) | (9u << RCC_PLLCFGR_PLLQ_Pos) | (7u << RCC_PLLCFGR_PLLR_Pos);
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) {}

    /* overdrive, exigido acima de 180 MHz */
    PWR->CR1 |= PWR_CR1_ODEN;
    while (!(PWR->CSR1 & PWR_CSR1_ODRDY)) {}
    PWR->CR1 |= PWR_CR1_ODSWEN;
    while (!(PWR->CSR1 & PWR_CSR1_ODSWRDY)) {}

    FLASH->ACR = FLASH_ACR_LATENCY_7WS | FLASH_ACR_PRFTEN | FLASH_ACR_ARTEN;
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_7WS) {}

    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 | RCC_CFGR_HPRE)) |
                RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}

    SCB_EnableICache();
}

static void gpio_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN;
    (void)RCC->AHB1ENR;

    gpio_mode(GPIOA, 4, GPIO_ANALOG);
    gpio_mode(GPIOA, 5, GPIO_ANALOG);
    gpio_mode(GPIOB, 0, GPIO_OUT);
    gpio_mode(GPIOB, 14, GPIO_OUT);
    gpio_mode(GPIOC, 13, GPIO_IN);
    gpio_af(GPIOD, 8, 7);
    gpio_af(GPIOD, 9, 7);
}

static void uart_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
    (void)RCC->APB1ENR;
    USART3->CR1 = 0;
    USART3->BRR = (PCLK1_HZ + USART_BAUD / 2u) / USART_BAUD;
    USART3->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;
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

    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    RCC->APB1ENR |= RCC_APB1ENR_DACEN | RCC_APB1ENR_TIM6EN;
    (void)RCC->APB1ENR;

    TIM6->CR1 = 0;
    TIM6->PSC = 0;
    TIM6->ARR = TIM6_CLK_HZ / BOARD_DAC_FS_HZ - 1u;
    TIM6->CR2 = 2u << TIM_CR2_MMS_Pos;   /* update -> TRGO */
    TIM6->EGR = TIM_EGR_UG;

    DAC->CR = 0;
    DAC->DHR12RD = BOARD_DAC_WORD(2048u, 0u);
    DAC->CR = (DAC_TSEL_TIM6 << DAC_CR_TSEL1_Pos) | DAC_CR_TEN1 | DAC_CR_DMAEN1 |
              (DAC_TSEL_TIM6 << DAC_CR_TSEL2_Pos) | DAC_CR_TEN2;
    DAC->CR |= DAC_CR_EN1 | DAC_CR_EN2;

    DMA_Stream_TypeDef *s = DMA1_Stream5;
    s->CR = 0;
    while (s->CR & DMA_SxCR_EN) {}
    DMA1->HIFCR = DMA_HIFCR_CTCIF5 | DMA_HIFCR_CHTIF5 | DMA_HIFCR_CTEIF5 | DMA_HIFCR_CDMEIF5 | DMA_HIFCR_CFEIF5;
    s->PAR = (uint32_t)&DAC->DHR12RD;
    s->M0AR = (uint32_t)dma_buf;
    s->NDTR = 2 * BOARD_DAC_HALF_LEN;
    s->FCR = 0;   /* modo direto */
    s->CR = (DMA_CH_DAC1 << DMA_SxCR_CHSEL_Pos) | (3u << DMA_SxCR_PL_Pos) | (2u << DMA_SxCR_MSIZE_Pos) |
            (2u << DMA_SxCR_PSIZE_Pos) | DMA_SxCR_MINC | DMA_SxCR_CIRC | DMA_SxCR_DIR_0 | DMA_SxCR_HTIE |
            DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
    NVIC_SetPriority(DMA1_Stream5_IRQn, 0);
    NVIC_EnableIRQ(DMA1_Stream5_IRQn);
    s->CR |= DMA_SxCR_EN;

    TIM6->CR1 = TIM_CR1_CEN;
}

void DMA1_Stream5_IRQHandler(void)
{
    uint32_t isr = DMA1->HISR;
    if (isr & (DMA_HISR_TEIF5 | DMA_HISR_DMEIF5)) {
        DMA1->HIFCR = DMA_HIFCR_CTEIF5 | DMA_HIFCR_CDMEIF5;
        dma_error = true;
    }
    if (isr & DMA_HISR_HTIF5) {
        DMA1->HIFCR = DMA_HIFCR_CHTIF5;
        fill_cb(&dma_buf[0], BOARD_DAC_HALF_LEN);
    }
    if (isr & DMA_HISR_TCIF5) {
        DMA1->HIFCR = DMA_HIFCR_CTCIF5;
        fill_cb(&dma_buf[BOARD_DAC_HALF_LEN], BOARD_DAC_HALF_LEN);
    }
}

bool board_dac_underrun(void)
{
    bool u = dma_error || (DAC->SR & DAC_SR_DMAUDR1);
    if (u) {
        DAC->SR = DAC_SR_DMAUDR1;
        dma_error = false;
    }
    return u;
}

void board_led(board_led_t led, bool on)
{
    gpio_write(GPIOB, led == BOARD_LED_STATUS ? 0 : 14, on);
}

bool board_button_raw(void) { return gpio_read(GPIOC, 13); }

void SysTick_Handler(void) { ms_ticks++; }
uint32_t board_millis(void) { return ms_ticks; }

void USART3_IRQHandler(void)
{
    usart_isr_rx(USART3, &rx, USART_ISR_RXNE, USART_ISR_ORE, USART_ICR_ORECF);
}

void board_uart_putc(char c)
{
    while (!(USART3->ISR & USART_ISR_TXE)) {}
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
