#include "uart_debug.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32f4xx.h"

enum {
    UART_TIMEOUT_SPINS = 100000U
};

static uint32_t uart_apb1_clock_hz(void)
{
    const uint32_t ppre1 = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    uint32_t divisor = 1U;

    if (ppre1 >= 4U) {
        divisor = 2U << (ppre1 - 4U);
    }

    return SystemCoreClock / divisor;
}

static void uart_gpio_alt(GPIO_TypeDef *port, uint8_t pin_no, uint8_t af)
{
    const uint32_t moder_shift = (uint32_t)pin_no * 2U;
    const uint32_t moder_mask = 0x3UL << moder_shift;
    const uint32_t afr_shift = (uint32_t)(pin_no % 8U) * 4U;
    const uint32_t afr_mask = 0xFUL << afr_shift;
    volatile uint32_t *afr = &port->AFR[pin_no / 8U];

    port->MODER = (port->MODER & ~moder_mask) | (0x2UL << moder_shift);
    port->OTYPER &= ~(1UL << pin_no);
    port->OSPEEDR = (port->OSPEEDR & ~moder_mask) | (0x2UL << moder_shift);
    port->PUPDR = (port->PUPDR & ~moder_mask) | (0x1UL << moder_shift);
    *afr = (*afr & ~afr_mask) | ((uint32_t)af << afr_shift);
}

void uart_debug_init(uint32_t baud)
{
    uint32_t apb_clock;

    if (baud == 0U) {
        baud = 115200U;
    }

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
    (void)RCC->AHB1ENR;
    (void)RCC->APB1ENR;

    uart_gpio_alt(GPIOD, 8U, 7U); /* ST-LINK VCP USART3 TX */
    uart_gpio_alt(GPIOD, 9U, 7U); /* ST-LINK VCP USART3 RX */

    USART3->CR1 = 0U;
    USART3->CR2 = 0U;
    USART3->CR3 = 0U;

    apb_clock = uart_apb1_clock_hz();
    USART3->BRR = (apb_clock + (baud / 2U)) / baud;
    USART3->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

void uart_debug_putc(char c)
{
    uint32_t spins = 0U;

    while (((USART3->SR & USART_SR_TXE) == 0U) && (spins < UART_TIMEOUT_SPINS)) {
        spins++;
    }

    if (spins < UART_TIMEOUT_SPINS) {
        USART3->DR = (uint16_t)(uint8_t)c;
    }
}

void uart_debug_write(const char *text)
{
    if (text == NULL) {
        return;
    }

    while (*text != '\0') {
        if (*text == '\n') {
            uart_debug_putc('\r');
        }
        uart_debug_putc(*text);
        text++;
    }
}

void uart_debug_write_u32(uint32_t value)
{
    char buffer[11];
    size_t index = sizeof(buffer);

    buffer[--index] = '\0';
    do {
        buffer[--index] = (char)('0' + (value % 10U));
        value /= 10U;
    } while ((value != 0U) && (index > 0U));

    uart_debug_write(&buffer[index]);
}
