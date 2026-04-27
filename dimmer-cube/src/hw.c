#include "hw.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "stm32l4xx_hal.h"

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
    uint16_t pin;
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

static uint8_t g_hw_scan_digit = 0U;
static uint8_t g_pwm_phase = 0U;

static const hw_pin_t k_latch_pin = {GPIOB, GPIO_PIN_5}; /* D4 */
static const hw_pin_t k_clock_pin = {GPIOA, GPIO_PIN_8}; /* D7 */
static const hw_pin_t k_data_pin = {GPIOA, GPIO_PIN_9};  /* D8 */

static const hw_pin_t k_led_pins[HW_DIGIT_COUNT] = {
    {GPIOA, GPIO_PIN_5}, /* D13 / LED1 */
    {GPIOA, GPIO_PIN_6}, /* D12 / LED2 */
    {GPIOA, GPIO_PIN_7}, /* D11 / LED3 */
    {GPIOB, GPIO_PIN_6}  /* D10 / LED4 */
};

static hw_button_t g_buttons[3] = {
    {{GPIOA, GPIO_PIN_1}, false, false, 0U, 0U, false, BUTTON_DECREASE},
    {{GPIOA, GPIO_PIN_4}, false, false, 0U, 0U, true, BUTTON_SELECT_SHORT},
    {{GPIOB, GPIO_PIN_0}, false, false, 0U, 0U, false, BUTTON_INCREASE}
};

static const uint8_t k_digit_select[HW_DIGIT_COUNT] = {
    0xF1U,
    0xF2U,
    0xF4U,
    0xF8U
};

/*
 * HAL_Init() enables the SysTick interrupt for the HAL time base. The Cube
 * project must provide this handler; otherwise execution falls into the weak
 * default startup handler on the first tick interrupt.
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
    HAL_SYSTICK_IRQHandler();
}

static void hw_gpio_write(hw_pin_t pin, bool high)
{
    HAL_GPIO_WritePin(pin.port,
                      pin.pin,
                      high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bool hw_gpio_read(hw_pin_t pin)
{
    return HAL_GPIO_ReadPin(pin.port, pin.pin) == GPIO_PIN_SET;
}

static void hw_shift_bit(bool high)
{
    hw_gpio_write(k_data_pin, high);
    hw_gpio_write(k_clock_pin, true);
    hw_gpio_write(k_clock_pin, false);
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
    hw_gpio_write(k_latch_pin, false);
    hw_shift_byte((uint8_t)~segments_active_high);
    hw_shift_byte(digit_bits);
    hw_gpio_write(k_latch_pin, true);
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
    const bool pin_high = hw_gpio_read(button->pin);

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
        hw_gpio_write(pin, !on);
        return;
    }

    hw_gpio_write(pin, on);
}

static bool hw_led_pwm_on(uint8_t level)
{
    const uint32_t duty = ((uint32_t)level * HW_PWM_PHASE_COUNT + LED_PWM_MAX) /
                          (LED_PWM_MAX + 1U);

    return (duty > 0U) && (g_pwm_phase < duty);
}

static void hw_init_gpio(void)
{
    GPIO_InitTypeDef gpio = {0};
    uint32_t index;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = k_latch_pin.pin;
    HAL_GPIO_Init(k_latch_pin.port, &gpio);
    gpio.Pin = k_clock_pin.pin;
    HAL_GPIO_Init(k_clock_pin.port, &gpio);
    gpio.Pin = k_data_pin.pin;
    HAL_GPIO_Init(k_data_pin.port, &gpio);

    for (index = 0U; index < HW_DIGIT_COUNT; ++index) {
        gpio.Pin = k_led_pins[index].pin;
        HAL_GPIO_Init(k_led_pins[index].port, &gpio);
    }

    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;

    for (index = 0U; index < (sizeof(g_buttons) / sizeof(g_buttons[0])); ++index) {
        gpio.Pin = g_buttons[index].pin.pin;
        HAL_GPIO_Init(g_buttons[index].pin.port, &gpio);
    }
}

void hw_init(void)
{
    uint32_t index;

    HAL_Init();
    hw_init_gpio();

    hw_gpio_write(k_latch_pin, false);
    hw_gpio_write(k_clock_pin, false);
    hw_gpio_write(k_data_pin, false);
    hw_write_display_raw(0U, HW_BLANK_DIGITS);

    for (index = 0U; index < HW_DIGIT_COUNT; ++index) {
        hw_led_write(k_led_pins[index], false);
    }
}

uint32_t hw_now_ms(void)
{
    return HAL_GetTick();
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
