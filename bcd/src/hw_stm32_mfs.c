#include <stdbool.h>
#include <stdint.h>

#include "stm32l476xx.h"
#include "bcd_hw.h"

enum {
    HW_SHIFT_BYTE_BITS = 8U,
    HW_BLANK_SEGMENTS = 0xFFU,
    HW_BLANK_DIGIT_SELECT = 0xFFU,
};

typedef struct {
    GPIO_TypeDef *port;
    uint8_t pin_no;
    uint16_t pin_mask;
} hw_pin_t;

static volatile uint32_t g_ms = 0U;

static const hw_pin_t k_latch_pin = {GPIOB, 5U, 1U << 5}; // D4
static const hw_pin_t k_clock_pin = {GPIOA, 8U, 1U << 8}; // D7
static const hw_pin_t k_data_pin = {GPIOA, 9U, 1U << 9}; // D8

static const uint8_t k_digit_select[4] = {
    0xF1U,
    0xF2U,
    0xF4U,
    0xF8U,
};

void SysTick_Handler(void)
{
    ++g_ms;
}

static void hw_gpio_out(hw_pin_t pin)
{
    const uint32_t shift = (uint32_t)pin.pin_no * 2U;
    const uint32_t mask = 0x3UL << shift;

    pin.port->MODER = (pin.port->MODER & ~mask) | (0x1UL << shift);
    pin.port->OTYPER &= ~(0x1UL << pin.pin_no);
    pin.port->OSPEEDR &= ~mask;
    pin.port->PUPDR &= ~mask;
}

static void hw_pin_write(hw_pin_t pin, bool high)
{
    pin.port->BSRR = high ? (uint32_t)pin.pin_mask
                          : ((uint32_t)pin.pin_mask << 16U);
}

static void hw_shift_bit(bool high)
{
    hw_pin_write(k_data_pin, high);
    hw_pin_write(k_clock_pin, true);
    hw_pin_write(k_clock_pin, false);
}

static void hw_shift_byte(uint8_t value)
{
    uint32_t i;

    for (i = 0U; i < HW_SHIFT_BYTE_BITS; ++i) {
        const uint8_t bit = (uint8_t)(1U << (7U - i));

        hw_shift_bit((value & bit) != 0U);
    }
}

static void hw_write_display(uint8_t segment_bits, uint8_t digit_bits)
{
    hw_pin_write(k_latch_pin, false);
    hw_shift_byte(segment_bits);
    hw_shift_byte(digit_bits);
    hw_pin_write(k_latch_pin, true);
}

void bcd_hw_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;

    hw_gpio_out(k_latch_pin);
    hw_gpio_out(k_clock_pin);
    hw_gpio_out(k_data_pin);

    hw_pin_write(k_latch_pin, true);
    hw_pin_write(k_clock_pin, false);
    hw_pin_write(k_data_pin, false);
    hw_write_display(HW_BLANK_SEGMENTS, HW_BLANK_DIGIT_SELECT);

    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000U);
}

uint32_t bcd_hw_now_ms(void)
{
    return g_ms;
}

void bcd_hw_show_digit(uint8_t digit_index, uint8_t segment_mask)
{
    uint8_t digit_bits;
    const uint8_t segment_bits = (uint8_t)~segment_mask;

    if (digit_index >= 4U) {
        hw_write_display(HW_BLANK_SEGMENTS, HW_BLANK_DIGIT_SELECT);
        return;
    }

    digit_bits = k_digit_select[digit_index];
    hw_write_display(segment_bits, digit_bits);
}

void bcd_hw_wait(void)
{
    __WFI();
}
