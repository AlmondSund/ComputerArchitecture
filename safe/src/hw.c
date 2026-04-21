#include "hw.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "stm32l476xx.h"

enum {
    HW_DIGIT_COUNT = 4U,
    HW_SHIFT_BYTE_BITS = 8U,
    HW_BLANK_SEGMENTS = 0xFFU,
    HW_BLANK_DIGITS = 0xFFU,
    HW_LED_ACTIVE_LOW = 1U,
    HW_BUTTON_ACTIVE_LOW = 1U,
    HW_BUZZER_KEYPRESS_ON_MS = 20U,
    HW_BUZZER_SUCCESS_ON_MS = 90U,
    HW_BUZZER_ERROR_ON_MS = 140U,
    HW_BUZZER_LOCKOUT_PERIOD_MS = 1000U,
    HW_BUZZER_LOCKOUT_ON_MS = 60U
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
    char mapped_key;
} hw_button_t;

typedef struct {
    char stable;
    char sample;
    uint32_t edge_ms;
} hw_key_filter_t;

static volatile uint32_t g_hw_ms = 0U;
static uint8_t g_hw_scan_digit = 0U;
static BuzzerPattern g_last_buzzer_pattern = BUZZER_SILENT;
static uint32_t g_buzzer_until_ms = 0U;

/*
 * Multi-Function Shield pins, reusing the verified mapping already used by
 * sibling projects in this repository.
 */
static const hw_pin_t k_latch_pin = {GPIOB, 5U, 1U << 5};  /* D4  */
static const hw_pin_t k_clock_pin = {GPIOA, 8U, 1U << 8};  /* D7  */
static const hw_pin_t k_data_pin = {GPIOA, 9U, 1U << 9};   /* D8  */
static const hw_pin_t k_buzzer_pin = {GPIOB, 3U, 1U << 3}; /* D3  */

static const hw_pin_t k_led_pins[HW_DIGIT_COUNT] = {
    {GPIOA, 5U, 1U << 5}, /* D13 / LED1 */
    {GPIOA, 6U, 1U << 6}, /* D12 / LED2 */
    {GPIOA, 7U, 1U << 7}, /* D11 / LED3 */
    {GPIOB, 6U, 1U << 6}  /* D10 / LED4 */
};

static hw_button_t g_buttons[3] = {
    {{GPIOA, 1U, 1U << 1}, false, false, 0U, 'A'}, /* A1 / S1 */
    {{GPIOA, 4U, 1U << 4}, false, false, 0U, '#'}, /* A2 / S2 */
    {{GPIOB, 0U, 1U << 0}, false, false, 0U, '*'}  /* A3 / S3 */
};

/*
 * Concrete keypad wiring for the prototype on ST morpho headers:
 * rows  -> PC10, PC11, PC12, PD2
 * cols  -> PC8,  PC6,  PC5,  PC4
 */
static const hw_pin_t k_keypad_rows[4] = {
    {GPIOC, 10U, 1U << 10},
    {GPIOC, 11U, 1U << 11},
    {GPIOC, 12U, 1U << 12},
    {GPIOD, 2U, 1U << 2}
};

static const hw_pin_t k_keypad_cols[4] = {
    {GPIOC, 8U, 1U << 8},
    {GPIOC, 6U, 1U << 6},
    {GPIOC, 5U, 1U << 5},
    {GPIOC, 4U, 1U << 4}
};

static const char k_keymap[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

static const uint8_t k_digit_select[HW_DIGIT_COUNT] = {
    0xF1U,
    0xF2U,
    0xF4U,
    0xF8U
};

static hw_key_filter_t g_keypad_filter = {0, 0, 0U};

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

static void hw_gpio_analog(hw_pin_t pin)
{
    const uint32_t shift = (uint32_t)pin.pin_no * 2U;
    const uint32_t mask = 0x3UL << shift;

    pin.port->MODER = (pin.port->MODER & ~mask) | (0x3UL << shift);
    pin.port->PUPDR &= ~mask;
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
    case 'G':
        return 0x3DU;
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
    case 'W':
        return 0x2AU;
    case 'Y':
        return 0x6EU;
    case '-':
        return 0x40U;
    case '_':
        return 0x08U;
    case '*':
        return 0x63U;
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

static bool hw_button_edge(hw_button_t *button, uint32_t now_ms)
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
    return button->stable;
}

static char hw_scan_keypad_matrix(void)
{
    uint32_t row_index;
    uint32_t found_count = 0U;
    char found_key = '\0';

    for (row_index = 0U; row_index < 4U; ++row_index) {
        uint32_t col_index;

        hw_pin_write(k_keypad_rows[0], true);
        hw_pin_write(k_keypad_rows[1], true);
        hw_pin_write(k_keypad_rows[2], true);
        hw_pin_write(k_keypad_rows[3], true);
        hw_pin_write(k_keypad_rows[row_index], false);

        __NOP();
        __NOP();

        for (col_index = 0U; col_index < 4U; ++col_index) {
            const bool col_high = hw_pin_is_high(k_keypad_cols[col_index]);

            if (!col_high) {
                found_key = k_keymap[row_index][col_index];
                found_count++;
            }
        }
    }

    hw_pin_write(k_keypad_rows[0], true);
    hw_pin_write(k_keypad_rows[1], true);
    hw_pin_write(k_keypad_rows[2], true);
    hw_pin_write(k_keypad_rows[3], true);

    return (found_count == 1U) ? found_key : '\0';
}

static bool hw_keypad_edge(uint32_t now_ms, char *pressed_key)
{
    const char sample = hw_scan_keypad_matrix();

    if (sample != g_keypad_filter.sample) {
        g_keypad_filter.sample = sample;
        g_keypad_filter.edge_ms = now_ms;
    }

    if ((uint32_t)(now_ms - g_keypad_filter.edge_ms) < KEYPAD_DEBOUNCE_MS) {
        return false;
    }

    if (g_keypad_filter.stable == g_keypad_filter.sample) {
        return false;
    }

    g_keypad_filter.stable = g_keypad_filter.sample;
    if (g_keypad_filter.stable == '\0') {
        return false;
    }

    *pressed_key = g_keypad_filter.stable;
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

static void hw_buzzer_write(bool on)
{
    if (!on) {
        /*
         * Park the buzzer pin in analog mode when silent so the shield sees a
         * true high-impedance input instead of a guessed inactive level.
         */
        hw_gpio_analog(k_buzzer_pin);
        return;
    }

    hw_gpio_out(k_buzzer_pin);
    if (BUZZER_ACTIVE_LOW != 0U) {
        hw_pin_write(k_buzzer_pin, false);
        return;
    }

    hw_pin_write(k_buzzer_pin, true);
}

static uint32_t hw_buzzer_duration_ms(BuzzerPattern pattern)
{
    switch (pattern) {
    case BUZZER_KEYPRESS:
        return HW_BUZZER_KEYPRESS_ON_MS;
    case BUZZER_SUCCESS:
        return HW_BUZZER_SUCCESS_ON_MS;
    case BUZZER_ERROR:
        return HW_BUZZER_ERROR_ON_MS;
    case BUZZER_LOCKOUT:
        return HW_BUZZER_LOCKOUT_ON_MS;
    case BUZZER_SILENT:
    default:
        return 0U;
    }
}

void hw_init(void)
{
    uint32_t index;

    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN |
                    RCC_AHB2ENR_GPIOBEN |
                    RCC_AHB2ENR_GPIOCEN |
                    RCC_AHB2ENR_GPIODEN;

    hw_gpio_out(k_latch_pin);
    hw_gpio_out(k_clock_pin);
    hw_gpio_out(k_data_pin);
    hw_gpio_analog(k_buzzer_pin);

    for (index = 0U; index < HW_DIGIT_COUNT; ++index) {
        hw_gpio_out(k_led_pins[index]);
        hw_gpio_out(k_keypad_rows[index]);
        hw_gpio_in_pullup(k_keypad_cols[index]);
        hw_pin_write(k_keypad_rows[index], true);
    }

    for (index = 0U; index < 3U; ++index) {
        hw_gpio_in_pullup(g_buttons[index].pin);
    }

    hw_pin_write(k_latch_pin, true);
    hw_pin_write(k_clock_pin, false);
    hw_pin_write(k_data_pin, false);
    hw_buzzer_write(false);

    for (index = 0U; index < HW_DIGIT_COUNT; ++index) {
        hw_led_write(k_led_pins[index], false);
    }

    hw_write_display_raw(0U, HW_BLANK_DIGITS);

    SystemCoreClockUpdate();
    (void)SysTick_Config(SystemCoreClock / 1000U);
}

uint32_t hw_now_ms(void)
{
    return g_hw_ms;
}

void hw_poll_inputs(App *app, uint32_t now_ms)
{
    uint32_t index;
    char key;

    if (app == NULL) {
        return;
    }

    if (hw_keypad_edge(now_ms, &key)) {
        (void)app_inject_key(app, key);
    }

    for (index = 0U; index < 3U; ++index) {
        if (hw_button_edge(&g_buttons[index], now_ms)) {
            (void)app_inject_key(app, g_buttons[index].mapped_key);
        }
    }
}

void hw_apply_outputs(const App *app, uint32_t now_ms)
{
    uint32_t index;
    bool buzzer_on;
    const BuzzerPattern requested_buzzer = app->indicators.sound_enabled
                                               ? app->indicators.buzzer
                                               : BUZZER_SILENT;

    if (app == NULL) {
        return;
    }

    hw_write_display_raw(
        hw_encode_char(app->display.text[g_hw_scan_digit]),
        k_digit_select[g_hw_scan_digit]);
    g_hw_scan_digit = (uint8_t)((g_hw_scan_digit + 1U) % HW_DIGIT_COUNT);

    switch (app->indicators.led) {
    case LED_SUCCESS:
        hw_led_write(k_led_pins[0], false);
        hw_led_write(k_led_pins[1], false);
        hw_led_write(k_led_pins[2], false);
        hw_led_write(k_led_pins[3], true);
        break;
    case LED_ERROR:
        hw_led_write(k_led_pins[0], true);
        hw_led_write(k_led_pins[1], false);
        hw_led_write(k_led_pins[2], false);
        hw_led_write(k_led_pins[3], false);
        break;
    case LED_LOCKOUT:
        for (index = 0U; index < HW_DIGIT_COUNT; ++index) {
            hw_led_write(k_led_pins[index], ((now_ms / 200U) % 2U) == 0U);
        }
        break;
    case LED_ADMIN:
        hw_led_write(k_led_pins[0], false);
        hw_led_write(k_led_pins[1], true);
        hw_led_write(k_led_pins[2], true);
        hw_led_write(k_led_pins[3], false);
        break;
    case LED_ARMED:
        hw_led_write(k_led_pins[0], false);
        hw_led_write(k_led_pins[1], true);
        hw_led_write(k_led_pins[2], false);
        hw_led_write(k_led_pins[3], false);
        break;
    case LED_OFF:
    default:
        for (index = 0U; index < HW_DIGIT_COUNT; ++index) {
            hw_led_write(k_led_pins[index], false);
        }
        break;
    }

    if (requested_buzzer == BUZZER_SILENT) {
        g_last_buzzer_pattern = BUZZER_SILENT;
        g_buzzer_until_ms = 0U;
    } else if (requested_buzzer != g_last_buzzer_pattern) {
        g_last_buzzer_pattern = requested_buzzer;
        g_buzzer_until_ms = now_ms + hw_buzzer_duration_ms(requested_buzzer);
    }

    buzzer_on = (g_buzzer_until_ms != 0U) &&
                ((int32_t)(g_buzzer_until_ms - now_ms) > 0);
    hw_buzzer_write(buzzer_on);
}

void hw_wait(void)
{
    __WFI();
}
