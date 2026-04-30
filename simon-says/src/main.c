#include "game.h"

#include <stdbool.h>
#include <stdint.h>

#include "stm32l476xx.h"

enum {
    HW_DIGIT_COUNT = 4U,
    HW_SHIFT_BYTE_BITS = 8U,
    HW_BUTTON_DEBOUNCE_MS = 180U,
    HW_BLANK_DIGITS = 0xFFU,
    HW_APP_TICK_MS = 1U,
    HW_DISPLAY_REFRESH_MS = 2U
};

typedef struct {
    GPIO_TypeDef *port;
    uint8_t pin_no;
    uint16_t pin_mask;
} HwPin;

static volatile uint32_t g_ms = 0U;
static volatile uint8_t g_scan_digit = 0U;
static volatile uint32_t g_last_button_ms[3] = {0U, 0U, 0U};
static Game g_game;

static const HwPin k_latch_pin = {GPIOB, 5U, 1U << 5}; /* D4 */
static const HwPin k_clock_pin = {GPIOA, 8U, 1U << 8}; /* D7 */
static const HwPin k_data_pin = {GPIOA, 9U, 1U << 9};  /* D8 */

static const HwPin k_button_pins[3] = {
    {GPIOA, 1U, 1U << 1}, /* A1 / S1 -> 1 */
    {GPIOA, 4U, 1U << 4}, /* A2 / S2 -> 2 */
    {GPIOB, 0U, 1U << 0}  /* A3 / S3 -> 3 */
};

static const uint8_t k_digit_select[HW_DIGIT_COUNT] = {
    0xF1U,
    0xF2U,
    0xF4U,
    0xF8U
};

static void hw_gpio_out(HwPin pin)
{
    const uint32_t shift = (uint32_t)pin.pin_no * 2U;
    const uint32_t mask = 0x3UL << shift;

    pin.port->MODER = (pin.port->MODER & ~mask) | (0x1UL << shift);
    pin.port->OTYPER &= ~(0x1UL << pin.pin_no);
    pin.port->OSPEEDR &= ~mask;
    pin.port->PUPDR &= ~mask;
}

static void hw_gpio_in_pullup(HwPin pin)
{
    const uint32_t shift = (uint32_t)pin.pin_no * 2U;
    const uint32_t mask = 0x3UL << shift;

    pin.port->MODER &= ~mask;
    pin.port->PUPDR = (pin.port->PUPDR & ~mask) | (0x1UL << shift);
}

static void hw_pin_write(HwPin pin, bool high)
{
    pin.port->BSRR = high ? (uint32_t)pin.pin_mask
                          : ((uint32_t)pin.pin_mask << 16U);
}

static bool hw_pin_is_high(HwPin pin)
{
    return (pin.port->IDR & pin.pin_mask) != 0U;
}

static void hw_shift_bit(bool high)
{
    hw_pin_write(k_data_pin, high);
    hw_pin_write(k_clock_pin, true);
    hw_pin_write(k_clock_pin, false);
}

static void hw_shift_byte(uint8_t value)
{
    uint32_t index;

    for (index = 0U; index < HW_SHIFT_BYTE_BITS; ++index) {
        const uint8_t bit = (uint8_t)(1U << (7U - index));

        hw_shift_bit((value & bit) != 0U);
    }
}

static void hw_write_display_raw(uint8_t segments_active_high, uint8_t digit_bits)
{
    hw_pin_write(k_latch_pin, false);
    hw_shift_byte((uint8_t)~segments_active_high);
    hw_shift_byte(digit_bits);
    hw_pin_write(k_latch_pin, true);
}

static uint8_t hw_encode_char(char c)
{
    switch (c) {
    case '0':
    case 'O':
        return 0x3FU;
    case '1':
        return 0x06U;
    case '2':
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
        return 0x7FU;
    case '9':
        return 0x6FU;
    case 'A':
        return 0x77U;
    case 'E':
        return 0x79U;
    case 'K':
        return 0x76U;
    case '*':
        return 0x63U;
    case '_':
        return 0x08U;
    case '-':
        return 0x40U;
    case ' ':
    default:
        return 0x00U;
    }
}

static void hw_refresh_display(void)
{
    uint8_t segments;
    const uint8_t digit = g_scan_digit;

    if (g_game.display.raw_override) {
        segments = g_game.display.raw_segments;
    } else {
        segments = hw_encode_char(g_game.display.text[digit]);
    }

    hw_write_display_raw(segments, k_digit_select[digit]);
    g_scan_digit = (uint8_t)((g_scan_digit + 1U) % HW_DIGIT_COUNT);
}

static void hw_try_button(uint8_t index)
{
    const uint32_t now = g_ms;

    if (index >= 3U) {
        return;
    }

    if (hw_pin_is_high(k_button_pins[index])) {
        return;
    }

    if ((uint32_t)(now - g_last_button_ms[index]) < HW_BUTTON_DEBOUNCE_MS) {
        return;
    }

    g_last_button_ms[index] = now;
    (void)game_press(&g_game, (uint8_t)(index + 1U));
}

static void hw_configure_button_exti(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    SYSCFG->EXTICR[0] &= ~(SYSCFG_EXTICR1_EXTI0 |
                           SYSCFG_EXTICR1_EXTI1);
    SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI0_PB;

    SYSCFG->EXTICR[1] &= ~SYSCFG_EXTICR2_EXTI4;

    EXTI->IMR1 |= EXTI_IMR1_IM0 | EXTI_IMR1_IM1 | EXTI_IMR1_IM4;
    EXTI->RTSR1 &= ~(EXTI_RTSR1_RT0 | EXTI_RTSR1_RT1 | EXTI_RTSR1_RT4);
    EXTI->FTSR1 |= EXTI_FTSR1_FT0 | EXTI_FTSR1_FT1 | EXTI_FTSR1_FT4;
    EXTI->PR1 = EXTI_PR1_PIF0 | EXTI_PR1_PIF1 | EXTI_PR1_PIF4;

    NVIC_SetPriority(EXTI0_IRQn, 2U);
    NVIC_SetPriority(EXTI1_IRQn, 2U);
    NVIC_SetPriority(EXTI4_IRQn, 2U);
    NVIC_EnableIRQ(EXTI0_IRQn);
    NVIC_EnableIRQ(EXTI1_IRQn);
    NVIC_EnableIRQ(EXTI4_IRQn);
}

static uint32_t hw_seed(void)
{
    const volatile uint32_t *uid = (const volatile uint32_t *)UID_BASE;

    return uid[0] ^ uid[1] ^ uid[2] ^ 0xACE1U;
}

static void hw_init(void)
{
    uint32_t index;

    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;

    hw_gpio_out(k_latch_pin);
    hw_gpio_out(k_clock_pin);
    hw_gpio_out(k_data_pin);

    for (index = 0U; index < 3U; ++index) {
        hw_gpio_in_pullup(k_button_pins[index]);
    }

    hw_pin_write(k_latch_pin, true);
    hw_pin_write(k_clock_pin, false);
    hw_pin_write(k_data_pin, false);
    hw_write_display_raw(0U, HW_BLANK_DIGITS);

    hw_configure_button_exti();

    SystemCoreClockUpdate();
    (void)SysTick_Config(SystemCoreClock / 1000U);
    NVIC_SetPriority(SysTick_IRQn, 2U);
}

void SysTick_Handler(void)
{
    static uint32_t app_elapsed_ms = 0U;
    static uint32_t refresh_elapsed_ms = 0U;

    ++g_ms;
    app_elapsed_ms++;
    refresh_elapsed_ms++;

    if (app_elapsed_ms >= HW_APP_TICK_MS) {
        game_tick(&g_game, app_elapsed_ms);
        app_elapsed_ms = 0U;
    }

    if (refresh_elapsed_ms >= HW_DISPLAY_REFRESH_MS) {
        hw_refresh_display();
        refresh_elapsed_ms = 0U;
    }
}

void EXTI0_IRQHandler(void)
{
    if ((EXTI->PR1 & EXTI_PR1_PIF0) != 0U) {
        EXTI->PR1 = EXTI_PR1_PIF0;
        hw_try_button(2U);
    }
}

void EXTI1_IRQHandler(void)
{
    if ((EXTI->PR1 & EXTI_PR1_PIF1) != 0U) {
        EXTI->PR1 = EXTI_PR1_PIF1;
        hw_try_button(0U);
    }
}

void EXTI4_IRQHandler(void)
{
    if ((EXTI->PR1 & EXTI_PR1_PIF4) != 0U) {
        EXTI->PR1 = EXTI_PR1_PIF4;
        hw_try_button(1U);
    }
}

int main(void)
{
    game_init(&g_game, hw_seed());
    hw_init();

    for (;;) {
        __WFI();
    }
}
