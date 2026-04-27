#include "hw.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "stm32l476xx.h"

enum {
    HW_DIGIT_COUNT = 4U,
    HW_SHIFT_BYTE_BITS = 8U,
    HW_BLANK_DIGITS = 0xFFU,
    HW_LED_ACTIVE_LOW = 1U,
    HW_BUTTON_ACTIVE_LOW = 1U,
    HW_PWM_PHASE_COUNT = 16U
};

typedef struct {
    GPIO_TypeDef *port;
    uint8_t pin_no;
    uint16_t pin_mask;
} hw_pin_t;

typedef struct {
    hw_pin_t pin;
    bool stable;
    bool sample;
    uint32_t edge_ms;
    uint32_t pressed_ms;
    bool classify_on_release;
    ButtonEvent press_event;
} hw_button_t;

static volatile uint32_t g_hw_ms = 0U;
static uint8_t g_hw_scan_digit = 0U;
static uint8_t g_pwm_phase = 0U;

static const hw_pin_t k_latch_pin = {GPIOB, 5U, 1U << 5}; /* D4 */
static const hw_pin_t k_clock_pin = {GPIOA, 8U, 1U << 8}; /* D7 */
static const hw_pin_t k_data_pin = {GPIOA, 9U, 1U << 9};  /* D8 */

static const hw_pin_t k_led_pins[HW_DIGIT_COUNT] = {
    {GPIOA, 5U, 1U << 5}, /* D13 / LED1 */
    {GPIOA, 6U, 1U << 6}, /* D12 / LED2 */
    {GPIOA, 7U, 1U << 7}, /* D11 / LED3 */
    {GPIOB, 6U, 1U << 6}  /* D10 / LED4 */
};

static hw_button_t g_buttons[3] = {
    {{GPIOA, 1U, 1U << 1}, false, false, 0U, 0U, false, BUTTON_DECREASE},
    {{GPIOA, 4U, 1U << 4}, false, false, 0U, 0U, true, BUTTON_SELECT_SHORT},
    {{GPIOB, 0U, 1U << 0}, false, false, 0U, 0U, false, BUTTON_INCREASE}
};

static const uint8_t k_digit_select[HW_DIGIT_COUNT] = {
    0xF1U,
    0xF2U,
    0xF4U,
    0xF8U
};

void SysTick_Handler(void)
{
    ++g_hw_ms;
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

static void hw_gpio_in_pullup(hw_pin_t pin)
{
    const uint32_t shift = (uint32_t)pin.pin_no * 2U;
    const uint32_t mask = 0x3UL << shift;

    pin.port->MODER &= ~mask;
    pin.port->PUPDR = (pin.port->PUPDR & ~mask) | (0x1UL << shift);
}

static void hw_pin_write(hw_pin_t pin, bool high)
{
    pin.port->BSRR = high ? (uint32_t)pin.pin_mask
                          : ((uint32_t)pin.pin_mask << 16U);
}

static bool hw_pin_is_high(hw_pin_t pin)
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
    uint32_t bit_index;

    for (bit_index = 0U; bit_index < HW_SHIFT_BYTE_BITS; ++bit_index) {
        const uint8_t bit = (uint8_t)(1U << (7U - bit_index));

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
    if ((c >= 'a') && (c <= 'z')) {
        c = (char)(c - ('a' - 'A'));
    }

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
    case 'L':
        return 0x38U;
    case 'U':
        return 0x3EU;
    case '-':
        return 0x40U;
    case '_':
        return 0x08U;
    case ' ':
    default:
        return 0x00U;
    }
}

static bool hw_button_pressed(const hw_button_t *button)
{
    const bool pin_high = hw_pin_is_high(button->pin);

    return (HW_BUTTON_ACTIVE_LOW != 0U) ? !pin_high : pin_high;
}

static bool hw_poll_button_event(hw_button_t *button, uint32_t now_ms, ButtonEvent *event)
{
    const bool sample = hw_button_pressed(button);

    if (sample != button->sample) {
        button->sample = sample;
        button->edge_ms = now_ms;
    }

    if ((uint32_t)(now_ms - button->edge_ms) < BUTTON_DEBOUNCE_MS) {
        return false;
    }

    if (button->stable == button->sample) {
        return false;
    }

    button->stable = button->sample;

    if (button->stable) {
        button->pressed_ms = now_ms;

        if (button->classify_on_release) {
            return false;
        }

        *event = button->press_event;
        return true;
    }

    if (!button->classify_on_release) {
        return false;
    }

    *event = ((uint32_t)(now_ms - button->pressed_ms) >= SELECT_HOLD_MS)
                 ? BUTTON_SELECT_LONG
                 : BUTTON_SELECT_SHORT;
    return true;
}

static void hw_led_write(hw_pin_t pin, bool on)
{
    if (HW_LED_ACTIVE_LOW != 0U) {
        hw_pin_write(pin, !on);
        return;
    }

    hw_pin_write(pin, on);
}

static bool hw_led_pwm_on(uint8_t level)
{
    const uint32_t duty = ((uint32_t)level * HW_PWM_PHASE_COUNT + LED_PWM_MAX) /
                          (LED_PWM_MAX + 1U);

    return (duty > 0U) && (g_pwm_phase < duty);
}

void hw_init(void)
{
    uint32_t index;

    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN |
                    RCC_AHB2ENR_GPIOBEN;

    hw_gpio_out(k_latch_pin);
    hw_gpio_out(k_clock_pin);
    hw_gpio_out(k_data_pin);

    for (index = 0U; index < HW_DIGIT_COUNT; ++index) {
        hw_gpio_out(k_led_pins[index]);
        hw_led_write(k_led_pins[index], false);
    }

    for (index = 0U; index < (sizeof(g_buttons) / sizeof(g_buttons[0])); ++index) {
        hw_gpio_in_pullup(g_buttons[index].pin);
    }

    hw_write_display_raw(0U, HW_BLANK_DIGITS);

    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000U);
}

uint32_t hw_now_ms(void)
{
    return g_hw_ms;
}

void hw_poll_inputs(App *app, uint32_t now_ms)
{
    uint32_t index;
    ButtonEvent event;

    if (app == NULL) {
        return;
    }

    for (index = 0U; index < (sizeof(g_buttons) / sizeof(g_buttons[0])); ++index) {
        if (hw_poll_button_event(&g_buttons[index], now_ms, &event)) {
            (void)app_inject_button(app, event);
        }
    }
}

void hw_apply_outputs(const App *app, uint32_t now_ms)
{
    uint32_t index;

    (void)now_ms;

    if (app == NULL) {
        hw_write_display_raw(0U, HW_BLANK_DIGITS);
        return;
    }

    hw_write_display_raw(hw_encode_char(app->display.text[g_hw_scan_digit]),
                         k_digit_select[g_hw_scan_digit]);
    g_hw_scan_digit = (g_hw_scan_digit + 1U) % HW_DIGIT_COUNT;
    g_pwm_phase = (g_pwm_phase + 1U) % HW_PWM_PHASE_COUNT;

    for (index = 0U; index < HW_DIGIT_COUNT; ++index) {
        hw_led_write(k_led_pins[index], hw_led_pwm_on(app->led_pwm_levels[index]));
    }
}

void hw_wait(void)
{
    __WFI();
}
