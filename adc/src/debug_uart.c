#include "debug_uart.h"

#include <stddef.h>
#include <stdint.h>

#include "stm32l476xx.h"

typedef struct {
    GPIO_TypeDef *port;
    uint8_t pin_no;
} pin_t;

static const pin_t k_usart2_tx_pin = {GPIOA, 2U}; /* ST-LINK VCP TX */

static void gpio_alternate(pin_t pin, uint8_t alternate)
{
    const uint32_t shift = (uint32_t)pin.pin_no * 2U;
    const uint32_t mode_mask = 0x3UL << shift;
    const uint32_t afr_index = (uint32_t)pin.pin_no / 8U;
    const uint32_t afr_shift = ((uint32_t)pin.pin_no % 8U) * 4U;
    const uint32_t afr_mask = 0xFUL << afr_shift;

    pin.port->MODER = (pin.port->MODER & ~mode_mask) | (0x2UL << shift);
    pin.port->OTYPER &= ~(0x1UL << pin.pin_no);
    pin.port->OSPEEDR = (pin.port->OSPEEDR & ~mode_mask) | (0x2UL << shift);
    pin.port->PUPDR &= ~mode_mask;
    pin.port->AFR[afr_index] =
            (pin.port->AFR[afr_index] & ~afr_mask)
            | ((uint32_t)alternate << afr_shift);
}

void debug_uart_init(uint32_t baud)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;
    (void)RCC->AHB2ENR;
    (void)RCC->APB1ENR1;

    gpio_alternate(k_usart2_tx_pin, 7U);

    USART2->CR1 &= ~USART_CR1_UE;
    USART2->BRR = SystemCoreClock / baud;
    USART2->CR2 = 0U;
    USART2->CR3 = 0U;
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

void debug_uart_putc(char c)
{
    while ((USART2->ISR & USART_ISR_TXE) == 0U) {
    }

    USART2->TDR = (uint32_t)(uint8_t)c;
}

void debug_uart_write(const char *text)
{
    while ((text != NULL) && (*text != '\0')) {
        if (*text == '\n') {
            debug_uart_putc('\r');
        }
        debug_uart_putc(*text);
        ++text;
    }
}

void debug_uart_write_u32(uint32_t value)
{
    char buffer[10];
    size_t count = 0U;

    do {
        buffer[count] = (char)('0' + (value % 10U));
        value /= 10U;
        ++count;
    } while ((value != 0U) && (count < sizeof(buffer)));

    while (count > 0U) {
        --count;
        debug_uart_putc(buffer[count]);
    }
}

void debug_uart_write_i32(int32_t value)
{
    if (value < 0) {
        debug_uart_putc('-');
        value = -value;
    }

    debug_uart_write_u32((uint32_t)value);
}
