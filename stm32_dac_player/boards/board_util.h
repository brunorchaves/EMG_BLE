/*
 * Pedacos iguais nas duas familias (H5 e F7 tem o mesmo bloco de GPIO e a
 * mesma USART v2). Incluir depois do header CMSIS do dispositivo.
 */
#ifndef BOARD_UTIL_H
#define BOARD_UTIL_H

#include <stdint.h>

enum { GPIO_IN = 0, GPIO_OUT = 1, GPIO_AF = 2, GPIO_ANALOG = 3 };

static inline void gpio_mode(GPIO_TypeDef *port, uint32_t pin, uint32_t mode)
{
    port->MODER = (port->MODER & ~(3u << (2 * pin))) | (mode << (2 * pin));
}

static inline void gpio_af(GPIO_TypeDef *port, uint32_t pin, uint32_t af)
{
    volatile uint32_t *afr = &port->AFR[pin >> 3];
    uint32_t sh = 4 * (pin & 7u);
    *afr = (*afr & ~(0xFu << sh)) | (af << sh);
    port->OSPEEDR |= 2u << (2 * pin);
    gpio_mode(port, pin, GPIO_AF);
}

static inline void gpio_write(GPIO_TypeDef *port, uint32_t pin, int on)
{
    port->BSRR = on ? (1u << pin) : (1u << (pin + 16));
}

static inline int gpio_read(GPIO_TypeDef *port, uint32_t pin)
{
    return (port->IDR >> pin) & 1u;
}

/* Ring buffer de recepcao da UART: escrito so na interrupcao, lido so no laco. */
#define RX_RING_LEN 128u
typedef struct {
    volatile uint8_t buf[RX_RING_LEN];
    volatile uint32_t head, tail;
} rx_ring_t;

static inline void rx_ring_push(rx_ring_t *r, uint8_t c)
{
    uint32_t next = (r->head + 1u) % RX_RING_LEN;
    if (next != r->tail) {
        r->buf[r->head] = c;
        r->head = next;
    }
}

static inline int rx_ring_pop(rx_ring_t *r)
{
    if (r->tail == r->head) return -1;
    int c = r->buf[r->tail];
    r->tail = (r->tail + 1u) % RX_RING_LEN;
    return c;
}

/* USART v2 (registradores ISR/RDR/TDR, iguais em H5 e F7) */
static inline void usart_isr_rx(USART_TypeDef *u, rx_ring_t *r, uint32_t rxne, uint32_t ore, uint32_t orecf)
{
    uint32_t isr = u->ISR;
    if (isr & ore) u->ICR = orecf;
    while (u->ISR & rxne) rx_ring_push(r, (uint8_t)u->RDR);
}

#endif /* BOARD_UTIL_H */
