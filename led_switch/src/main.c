#include "stm32l476xx.h"
#include <stdbool.h>
#include <stdint.h>

/*
this program controls the LEDs on the Multifunction Shield for STM32L476RG 
- D1: Does a heartbeat effect controlled by heartbeat_t configuration (ON_MS and OFF_MS)
- D2, D3, D4: Toggle whenever the corresponding button (S1-A1, S2-A2, S3-A3) is pressed.
*/

volatile uint32_t msTicks = 0U;

typedef enum {
    BUTTON_IDLE = 0U,
    BUTTON_DEBOUNCE_PRESS,
    BUTTON_PRESSED,
    BUTTON_DEBOUNCE_RELEASE
} ButtonState;

// Heartbeat FSM
typedef enum {
    LED_HEARTBEAT_OFF = 0U,
    LED_HEARTBEAT_ON
} LedState;

typedef struct {
    GPIO_TypeDef *port;
    uint8_t pin_idx;
    bool active_low;
    bool is_on;
} led_t;

typedef struct {
    GPIO_TypeDef *port;
    uint8_t pin_idx;
    bool active_low;
    uint32_t last_transition_ms;
    ButtonState state;
} button_t;

void SysTick_Handler(void) {
    msTicks++;
}

typedef struct {
    led_t *led;
    LedState state;
    uint32_t on_ms;
    uint32_t off_ms;
    uint32_t last_transition_ms;
} heartbeat_t;

static uint32_t pin_mask(
    uint8_t pin_idx
) {
    return (1UL << pin_idx);
}

static bool elapsed_ms(
    uint32_t now_ms,
    uint32_t from_ms,
    uint32_t period_ms
) {
    return ((uint32_t)(now_ms - from_ms) >= period_ms);
}

static void gpio_config_output_push_pull(
    GPIO_TypeDef *port,
    uint8_t pin_idx
) {
    const uint32_t shift = ((uint32_t)pin_idx * 2U);

    // Configure as general-purpose output, push-pull, low-speed, no pull.
    port->MODER = (port->MODER & ~(0x3UL << shift)) | (0x1UL << shift);
    port->OTYPER &= ~pin_mask(pin_idx);
    port->OSPEEDR &= ~(0x3UL << shift);
    port->PUPDR &= ~(0x3UL << shift);
}

static void gpio_config_input(
    GPIO_TypeDef *port,
    uint8_t pin_idx,
    bool pull_up
) {
    const uint32_t shift = ((uint32_t)pin_idx * 2U);
    const uint32_t pull_cfg = (pull_up ? 0x1UL : 0x2UL);

    // Configure as input and select pull-up or pull-down according to wiring.
    port->MODER &= ~(0x3UL << shift);
    port->PUPDR = (port->PUPDR & ~(0x3UL << shift)) | (pull_cfg << shift);
}

static void led_write(
    led_t *led,
    bool turn_on
) {
    const bool drive_high = (led->active_low ? !turn_on : turn_on);
    const uint32_t mask = pin_mask(led->pin_idx);

    // Use BSRR for atomic set/reset writes.
    led->port->BSRR = (drive_high ? mask : (mask << 16U));
    led->is_on = turn_on;
}

static void led_toggle(
    led_t *led
) {
    led_write(led, !led->is_on);
}

static bool button_is_pressed_raw(
    const button_t *button
) {
    const bool level_high = ((button->port->IDR & pin_mask(button->pin_idx)) != 0U);

    return (button->active_low ? !level_high : level_high);
}

static bool button_fsm_update(
    button_t *button,
    uint32_t now_ms,
    uint32_t debounce_ms
) {
    const bool pressed_raw = button_is_pressed_raw(button);

    // Debounce and edge-detect using a 4-state FSM.
    switch (button->state) {
    case BUTTON_IDLE:
        if (pressed_raw) {
            button->state = BUTTON_DEBOUNCE_PRESS;
            button->last_transition_ms = now_ms;
        }
        break;

    case BUTTON_DEBOUNCE_PRESS:
        if (!pressed_raw) {
            button->state = BUTTON_IDLE;
        } else if (elapsed_ms(now_ms, button->last_transition_ms, debounce_ms)) {
            button->state = BUTTON_PRESSED;
            return true; // Emit a single event on confirmed press.
        }
        break;

    case BUTTON_PRESSED:
        if (!pressed_raw) {
            button->state = BUTTON_DEBOUNCE_RELEASE;
            button->last_transition_ms = now_ms;
        }
        break;

    case BUTTON_DEBOUNCE_RELEASE:
        if (pressed_raw) {
            button->state = BUTTON_PRESSED;
        } else if (elapsed_ms(now_ms, button->last_transition_ms, debounce_ms)) {
            button->state = BUTTON_IDLE;
        }
        break;

    default:
        button->state = BUTTON_IDLE;
        break;
    }

    return false;
}

static void heartbeat_fsm_update(
    heartbeat_t *heartbeat,
    uint32_t now_ms
) {
    switch (heartbeat->state) {
    case LED_HEARTBEAT_OFF:
        if (elapsed_ms(now_ms, heartbeat->last_transition_ms, heartbeat->off_ms)) {
            led_write(heartbeat->led, true);
            heartbeat->state = LED_HEARTBEAT_ON;
            heartbeat->last_transition_ms = now_ms;
        }
        break;

    case LED_HEARTBEAT_ON:
        if (elapsed_ms(now_ms, heartbeat->last_transition_ms, heartbeat->on_ms)) {
            led_write(heartbeat->led, false);
            heartbeat->state = LED_HEARTBEAT_OFF;
            heartbeat->last_transition_ms = now_ms;
        }
        break;

    default:
        heartbeat->state = LED_HEARTBEAT_OFF;
        led_write(heartbeat->led, false);
        heartbeat->last_transition_ms = now_ms;
        break;
    }
}

static void board_gpio_init(void) {
    // Shield mapping on NUCLEO-L476RG:
    // LEDs D1..D4 -> PA5, PA6, PA7, PB6. Buttons S1..S3 -> PA1, PA4, PB0.
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;
    (void)RCC->AHB2ENR;

    gpio_config_output_push_pull(GPIOA, 5U);
    gpio_config_output_push_pull(GPIOA, 6U);
    gpio_config_output_push_pull(GPIOA, 7U);
    gpio_config_output_push_pull(GPIOB, 6U);

    // Most Multifunction Shields wire buttons as active-low; keep pull-ups enabled.
    gpio_config_input(GPIOA, 1U, true);
    gpio_config_input(GPIOA, 4U, true);
    gpio_config_input(GPIOB, 0U, true);
}

int main(void) {
    // Configure SysTick to generate an interrupt every 1 ms
    SysTick_Config(SystemCoreClock / 1000U);

    board_gpio_init();

    // Multifunction Shield LEDs are usually active-low (sinking current).
    led_t led_d1 = { GPIOA, 5U, true, false };
    led_t led_d2 = { GPIOA, 6U, true, false };
    led_t led_d3 = { GPIOA, 7U, true, false };
    led_t led_d4 = { GPIOB, 6U, true, false };

    led_write(&led_d1, false);
    led_write(&led_d2, false);
    led_write(&led_d3, false);
    led_write(&led_d4, false);

    button_t button_s1 = { GPIOA, 1U, true, 0U, BUTTON_IDLE };
    button_t button_s2 = { GPIOA, 4U, true, 0U, BUTTON_IDLE };
    button_t button_s3 = { GPIOB, 0U, true, 0U, BUTTON_IDLE };

    heartbeat_t heartbeat = {
        .led = &led_d1,
        .state = LED_HEARTBEAT_OFF,
        .on_ms = 120U,
        .off_ms = 880U,
        .last_transition_ms = msTicks
    };

    const uint32_t debounce_ms = 30U;

    while (1) {
        // SUPERLOOP
        const uint32_t now_ms = msTicks;

        heartbeat_fsm_update(&heartbeat, now_ms);

        if (button_fsm_update(&button_s1, now_ms, debounce_ms)) {
            led_toggle(&led_d2);
        }

        if (button_fsm_update(&button_s2, now_ms, debounce_ms)) {
            led_toggle(&led_d3);
        }

        if (button_fsm_update(&button_s3, now_ms, debounce_ms)) {
            led_toggle(&led_d4);
        }
    }
}
