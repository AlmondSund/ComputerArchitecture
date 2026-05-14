#include "mfs_display.h"

#include <stdbool.h>
#include <stdint.h>

#include "stm32l476xx.h"

typedef struct {
    GPIO_TypeDef *port;
    uint8_t pin_no;
    uint16_t pin_mask;
} pin_t;

static const pin_t k_latch_pin = {GPIOB, 5U, 1U << 5}; /* MFS D4 */
static const pin_t k_clock_pin = {GPIOA, 8U, 1U << 8}; /* MFS D7 */
static const pin_t k_data_pin = {GPIOA, 9U, 1U << 9};  /* MFS D8 */

static const uint8_t k_digit_select[MFS_DISPLAY_DIGITS] = {
    0xF1U,
    0xF2U,
    0xF4U,
    0xF8U,
};

static void pin_write(pin_t pin, bool high)
{
    pin.port->BSRR = high ? (uint32_t)pin.pin_mask
                          : ((uint32_t)pin.pin_mask << 16U);
}

static void gpio_output(pin_t pin)
{
    const uint32_t shift = (uint32_t)pin.pin_no * 2U;
    const uint32_t mask = 0x3UL << shift;

    pin.port->MODER = (pin.port->MODER & ~mask) | (0x1UL << shift);
    pin.port->OTYPER &= ~(0x1UL << pin.pin_no);
    pin.port->OSPEEDR &= ~mask;
    pin.port->PUPDR &= ~mask;
}

static void shift_bit(bool high)
{
    pin_write(k_data_pin, high);
    pin_write(k_clock_pin, true);
    pin_write(k_clock_pin, false);
}

static void shift_byte(uint8_t value)
{
    uint32_t bit_index;

    for (bit_index = 0U; bit_index < 8U; ++bit_index) {
        const uint8_t bit = (uint8_t)(1U << (7U - bit_index));

        shift_bit((value & bit) != 0U);
    }
}

static void write_raw(uint8_t segment_bits, uint8_t digit_bits)
{
    pin_write(k_latch_pin, false);
    shift_byte(segment_bits);
    shift_byte(digit_bits);
    pin_write(k_latch_pin, true);
}

static uint8_t encode_char(char c)
{
    switch (c) {
    case '0':
    case 'O':
        return 0x3FU;
    case '1':
    case 'I':
        return 0x06U;
    case '2':
    case 'Z':
        return 0x5BU;
    case '3':
        return 0x4FU;
    case '4':
        return 0x66U;
    case '5':
    case 'S':
        return 0x6DU;
    case '6':
        return 0x7DU;
    case '7':
        return 0x07U;
    case '8':
    case 'B':
        return 0x7FU;
    case '9':
        return 0x6FU;
    case 'A':
        return 0x77U;
    case 'C':
        return 0x39U;
    case 'D':
        return 0x5EU;
    case 'E':
        return 0x79U;
    case 'F':
        return 0x71U;
    case 'H':
        return 0x76U;
    case 'L':
        return 0x38U;
    case 'N':
        return 0x54U;
    case 'P':
        return 0x73U;
    case 'R':
        return 0x50U;
    case 'T':
        return 0x78U;
    case 'U':
        return 0x3EU;
    case 'Y':
        return 0x6EU;
    case '-':
        return 0x40U;
    case ' ':
    default:
        return 0x00U;
    }
}

void mfs_display_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;
    (void)RCC->AHB2ENR;

    gpio_output(k_latch_pin);
    gpio_output(k_clock_pin);
    gpio_output(k_data_pin);

    pin_write(k_latch_pin, true);
    pin_write(k_clock_pin, false);
    pin_write(k_data_pin, false);
    mfs_display_blank();
}

void mfs_display_show_digit(uint8_t digit, char c)
{
    if (digit >= MFS_DISPLAY_DIGITS) {
        mfs_display_blank();
        return;
    }

    write_raw((uint8_t)~encode_char(c), k_digit_select[digit]);
}

void mfs_display_blank(void)
{
    write_raw(0xFFU, 0xFFU);
}
