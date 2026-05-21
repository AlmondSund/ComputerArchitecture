#include "mfs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32f4xx.h"

enum {
    MFS_DIGIT_COUNT = DSPLINK_DISPLAY_WIDTH,
    MFS_SHIFT_BITS = 8U,
    MFS_BUTTON_COUNT = 3U,
    MFS_BUTTON_DEBOUNCE_MS = 40U,
    MFS_DISPLAY_REFRESH_MS = 2U,
    MFS_BLANK_DIGITS = 0xFFU,
    MFS_ADC_TIMEOUT_SPINS = 100000U,
    MFS_ADC_AVERAGE_SAMPLES = 8U,
    MFS_BUZZER_ACTIVE_LOW = 1U
};

typedef struct {
    GPIO_TypeDef *port;
    uint8_t pin_no;
    uint16_t pin_mask;
} mfs_pin_t;

typedef struct {
    mfs_pin_t pin;
    bool stable_pressed;
    bool sample_pressed;
    uint32_t sample_since_ms;
    dsplink_ui_event_t event;
} mfs_button_t;

/*
 * NUCLEO-F429ZI Arduino header mapping used by the current hardware contract:
 * D13 PA5, D12 PA6, D11 PA7, D10 PD14, D8 PF12, D7 PF13, D4 PF14,
 * D3 PE13, A0 PA3, A1 PC0, A2 PC3, A3 PF3.
 */
static const mfs_pin_t k_latch_pin = {GPIOF, 14U, 1U << 14}; /* D4 */
static const mfs_pin_t k_clock_pin = {GPIOF, 13U, 1U << 13}; /* D7 */
static const mfs_pin_t k_data_pin = {GPIOF, 12U, 1U << 12};  /* D8 */
static const mfs_pin_t k_buzzer_pin = {GPIOE, 13U, 1U << 13}; /* D3 */
static const mfs_pin_t k_pot_pin = {GPIOA, 3U, 1U << 3};     /* A0 / ADC1_IN3 */

static const mfs_pin_t k_led_pins[MFS_DIGIT_COUNT] = {
    {GPIOA, 5U, 1U << 5},  /* D13 / LED1 */
    {GPIOA, 6U, 1U << 6},  /* D12 / LED2 */
    {GPIOA, 7U, 1U << 7},  /* D11 / LED3 */
    {GPIOD, 14U, 1U << 14} /* D10 / LED4 */
};

static mfs_button_t g_buttons[MFS_BUTTON_COUNT] = {
    {{GPIOC, 0U, 1U << 0}, false, false, 0U, DSPLINK_UI_EVENT_PREVIOUS},
    {{GPIOC, 3U, 1U << 3}, false, false, 0U, DSPLINK_UI_EVENT_NEXT},
    {{GPIOF, 3U, 1U << 3}, false, false, 0U, DSPLINK_UI_EVENT_TOGGLE_BYPASS}
};

static const uint8_t k_digit_select[MFS_DIGIT_COUNT] = {
    0xF1U,
    0xF2U,
    0xF4U,
    0xF8U
};

static char g_display_text[MFS_DIGIT_COUNT + 1U] = {' ', ' ', ' ', ' ', '\0'};
static uint8_t g_scan_digit = 0U;
static uint32_t g_last_refresh_ms = 0U;
static uint32_t g_buzzer_off_ms = 0U;

static void mfs_gpio_out(mfs_pin_t pin)
{
    const uint32_t shift = (uint32_t)pin.pin_no * 2U;
    const uint32_t mask = 0x3UL << shift;

    pin.port->MODER = (pin.port->MODER & ~mask) | (0x1UL << shift);
    pin.port->OTYPER &= ~(1UL << pin.pin_no);
    pin.port->OSPEEDR = (pin.port->OSPEEDR & ~mask) | (0x1UL << shift);
    pin.port->PUPDR &= ~mask;
}

static void mfs_gpio_in_pullup(mfs_pin_t pin)
{
    const uint32_t shift = (uint32_t)pin.pin_no * 2U;
    const uint32_t mask = 0x3UL << shift;

    pin.port->MODER &= ~mask;
    pin.port->PUPDR = (pin.port->PUPDR & ~mask) | (0x1UL << shift);
}

static void mfs_gpio_analog(mfs_pin_t pin)
{
    const uint32_t shift = (uint32_t)pin.pin_no * 2U;
    const uint32_t mask = 0x3UL << shift;

    pin.port->MODER = (pin.port->MODER & ~mask) | (0x3UL << shift);
    pin.port->PUPDR &= ~mask;
}

static void mfs_pin_write(mfs_pin_t pin, bool high)
{
    pin.port->BSRR = high ? (uint32_t)pin.pin_mask
                          : ((uint32_t)pin.pin_mask << 16U);
}

static bool mfs_pin_is_high(mfs_pin_t pin)
{
    return (pin.port->IDR & pin.pin_mask) != 0U;
}

static void mfs_buzzer_write(bool on)
{
    if (!on) {
        /*
         * The MFS buzzer circuit can keep sounding if D3 is driven to the
         * wrong inactive level. Match the proven safe-project behavior: leave
         * the pin high-impedance when silent and only drive it for a beep.
         */
        mfs_gpio_analog(k_buzzer_pin);
        return;
    }

    mfs_gpio_out(k_buzzer_pin);
    mfs_pin_write(k_buzzer_pin, MFS_BUZZER_ACTIVE_LOW == 0U);
}

static void mfs_shift_bit(bool high)
{
    mfs_pin_write(k_data_pin, high);
    mfs_pin_write(k_clock_pin, true);
    mfs_pin_write(k_clock_pin, false);
}

static void mfs_shift_byte(uint8_t value)
{
    uint32_t index;

    for (index = 0U; index < MFS_SHIFT_BITS; ++index) {
        const uint8_t bit = (uint8_t)(1U << (7U - index));

        mfs_shift_bit((value & bit) != 0U);
    }
}

static void mfs_write_display_raw(uint8_t segments_active_high, uint8_t digit_bits)
{
    mfs_pin_write(k_latch_pin, false);
    mfs_shift_byte((uint8_t)~segments_active_high);
    mfs_shift_byte(digit_bits);
    mfs_pin_write(k_latch_pin, true);
}

static uint8_t mfs_encode_char(char c)
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
    case '-':
        return 0x40U;
    case '_':
        return 0x08U;
    case ' ':
    default:
        return 0x00U;
    }
}

static void mfs_init_adc(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    (void)RCC->APB2ENR;

    ADC1->CR1 = 0U;
    ADC1->CR2 = 0U;
    ADC1->SMPR2 |= ADC_SMPR2_SMP3;
    ADC1->SQR1 = 0U;
    ADC1->SQR3 = 3U;
    ADC1->CR2 = ADC_CR2_ADON;
}

void mfs_init(void)
{
    uint32_t index;

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN
                    | RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOEEN
                    | RCC_AHB1ENR_GPIOFEN;
    (void)RCC->AHB1ENR;

    mfs_gpio_out(k_latch_pin);
    mfs_gpio_out(k_clock_pin);
    mfs_gpio_out(k_data_pin);
    mfs_gpio_analog(k_buzzer_pin);
    mfs_gpio_analog(k_pot_pin);

    for (index = 0U; index < MFS_DIGIT_COUNT; ++index) {
        mfs_gpio_out(k_led_pins[index]);
    }

    for (index = 0U; index < MFS_BUTTON_COUNT; ++index) {
        mfs_gpio_in_pullup(g_buttons[index].pin);
        g_buttons[index].stable_pressed = false;
        g_buttons[index].sample_pressed = false;
        g_buttons[index].sample_since_ms = 0U;
    }

    mfs_pin_write(k_latch_pin, true);
    mfs_pin_write(k_clock_pin, false);
    mfs_pin_write(k_data_pin, false);
    mfs_buzzer_write(false);
    mfs_set_leds(0U);
    mfs_write_display_raw(0U, MFS_BLANK_DIGITS);
    mfs_init_adc();
}

dsplink_ui_event_t mfs_poll_buttons(uint32_t now_ms)
{
    uint32_t index;
    dsplink_ui_event_t events = DSPLINK_UI_EVENT_NONE;

    for (index = 0U; index < MFS_BUTTON_COUNT; ++index) {
        mfs_button_t *button = &g_buttons[index];
        const bool pressed = !mfs_pin_is_high(button->pin);

        if (pressed != button->sample_pressed) {
            button->sample_pressed = pressed;
            button->sample_since_ms = now_ms;
            continue;
        }

        if (((uint32_t)(now_ms - button->sample_since_ms) >= MFS_BUTTON_DEBOUNCE_MS)
                && (button->stable_pressed != button->sample_pressed)) {
            button->stable_pressed = button->sample_pressed;
            if (button->stable_pressed) {
                events = (dsplink_ui_event_t)((uint32_t)events | (uint32_t)button->event);
            }
        }
    }

    return events;
}

uint16_t mfs_read_potentiometer(void)
{
    uint32_t sample;
    uint32_t sum = 0U;

    for (sample = 0U; sample < MFS_ADC_AVERAGE_SAMPLES; ++sample) {
        uint32_t spins = 0U;

        ADC1->CR2 |= ADC_CR2_SWSTART;
        while (((ADC1->SR & ADC_SR_EOC) == 0U) && (spins < MFS_ADC_TIMEOUT_SPINS)) {
            spins++;
        }

        if (spins >= MFS_ADC_TIMEOUT_SPINS) {
            return 0U;
        }

        sum += (uint16_t)(ADC1->DR & DSPLINK_POT_MAX);
    }

    return (uint16_t)((sum + (MFS_ADC_AVERAGE_SAMPLES / 2U))
                      / MFS_ADC_AVERAGE_SAMPLES);
}

void mfs_set_leds(uint8_t led_mask)
{
    uint32_t index;

    for (index = 0U; index < MFS_DIGIT_COUNT; ++index) {
        const bool on = ((led_mask & (1U << index)) != 0U);

        mfs_pin_write(k_led_pins[index], !on);
    }
}

void mfs_display_text(const char text[DSPLINK_DISPLAY_WIDTH + 1U])
{
    uint32_t index;

    if (text == NULL) {
        return;
    }

    for (index = 0U; index < MFS_DIGIT_COUNT; ++index) {
        g_display_text[index] = text[index];
        if (text[index] == '\0') {
            while (index < MFS_DIGIT_COUNT) {
                g_display_text[index] = ' ';
                index++;
            }
            break;
        }
    }

    g_display_text[MFS_DIGIT_COUNT] = '\0';
}

void mfs_refresh_display(uint32_t now_ms)
{
    const uint8_t digit = g_scan_digit;
    const uint8_t segments = mfs_encode_char(g_display_text[digit]);

    if ((uint32_t)(now_ms - g_last_refresh_ms) < MFS_DISPLAY_REFRESH_MS) {
        return;
    }

    g_last_refresh_ms = now_ms;
    mfs_write_display_raw(segments, k_digit_select[digit]);
    g_scan_digit = (uint8_t)((g_scan_digit + 1U) % MFS_DIGIT_COUNT);
}

void mfs_beep(uint32_t now_ms, uint32_t duration_ms)
{
    mfs_buzzer_write(true);
    g_buzzer_off_ms = now_ms + duration_ms;
}

void mfs_service_buzzer(uint32_t now_ms)
{
    if ((g_buzzer_off_ms != 0U) && ((int32_t)(now_ms - g_buzzer_off_ms) >= 0)) {
        mfs_buzzer_write(false);
        g_buzzer_off_ms = 0U;
    }
}
