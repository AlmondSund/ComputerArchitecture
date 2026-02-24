#include "mfs_led_driver.h"

#include <stdint.h>

#include "stm32l476xx.h"

typedef struct
{
    GPIO_TypeDef *port;
    uint8_t pin;
    uint32_t bsrr_on_mask;
    uint32_t bsrr_off_mask;
} mfs_led_hw_t;

typedef struct
{
    bool is_initialized;
    uint8_t led_state_mask;
} mfs_led_driver_context_t;

static mfs_led_driver_context_t s_driver_context = {
    .is_initialized = false,
    .led_state_mask = 0U
};

static const mfs_led_hw_t s_led_hw_map[MFS_LED_COUNT] = {
    [MFS_LED_D1] = {.port = GPIOA, .pin = 5U, .bsrr_on_mask = (1UL << (5U + 16U)), .bsrr_off_mask = (1UL << 5U)},
    [MFS_LED_D2] = {.port = GPIOA, .pin = 6U, .bsrr_on_mask = (1UL << (6U + 16U)), .bsrr_off_mask = (1UL << 6U)},
    [MFS_LED_D3] = {.port = GPIOA, .pin = 7U, .bsrr_on_mask = (1UL << (7U + 16U)), .bsrr_off_mask = (1UL << 7U)},
    [MFS_LED_D4] = {.port = GPIOB, .pin = 6U, .bsrr_on_mask = (1UL << (6U + 16U)), .bsrr_off_mask = (1UL << 6U)}
};

/**
 * @brief Validates logical LED indices before hardware access.
 */
static bool mfs_led_is_valid(mfs_led_t led)
{
    return led < MFS_LED_COUNT;
}

/**
 * @brief Configures one GPIO pin as a push-pull output.
 *
 * @param port GPIO peripheral base.
 * @param pin  Pin number in [0..15].
 */
static void mfs_led_configure_gpio_output(GPIO_TypeDef *port, uint8_t pin)
{
    const uint32_t mode_shift = (uint32_t)pin * 2UL;
    const uint32_t mode_mask = (3UL << mode_shift);
    const uint32_t mode_output = (1UL << mode_shift);

    port->MODER &= ~mode_mask;
    port->MODER |= mode_output;
}

/**
 * @brief Writes one LED state and updates the encapsulated software state.
 *
 * @param led   Logical LED index.
 * @param is_on true = LED ON, false = LED OFF.
 */
static void mfs_led_write_state(mfs_led_t led, bool is_on)
{
    const mfs_led_hw_t *const hw = &s_led_hw_map[led];
    const uint8_t led_bit = (uint8_t)(1U << (uint8_t)led);

    hw->port->BSRR = is_on ? hw->bsrr_on_mask : hw->bsrr_off_mask;

    if (is_on)
    {
        s_driver_context.led_state_mask |= led_bit;
    }
    else
    {
        s_driver_context.led_state_mask &= (uint8_t)(~led_bit);
    }
}

void mfs_led_driver_init(void)
{
    /* Enable GPIOA/GPIOB before touching LED pins. */
    RCC->AHB2ENR |= (RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN);
    (void)RCC->AHB2ENR;
    __DSB();
    __ISB();

    /*
     * Force OFF state before mode switch to avoid startup glitches.
     * LEDs are active-low, therefore OFF means writing HIGH via BSRR set bits.
     */
    GPIOA->BSRR =
        s_led_hw_map[MFS_LED_D1].bsrr_off_mask |
        s_led_hw_map[MFS_LED_D2].bsrr_off_mask |
        s_led_hw_map[MFS_LED_D3].bsrr_off_mask;
    GPIOB->BSRR = s_led_hw_map[MFS_LED_D4].bsrr_off_mask;

    /* Configure all shield LED pins as outputs. */
    for (uint32_t led_index = 0U; led_index < (uint32_t)MFS_LED_COUNT; ++led_index)
    {
        mfs_led_configure_gpio_output(
            s_led_hw_map[led_index].port,
            s_led_hw_map[led_index].pin
        );
    }

    s_driver_context.led_state_mask = 0U;
    s_driver_context.is_initialized = true;
}

void mfs_led_all_off(void)
{
    if (!s_driver_context.is_initialized)
    {
        mfs_led_driver_init();
    }

    /* Group GPIO writes by port to keep updates deterministic and concise. */
    GPIOA->BSRR =
        s_led_hw_map[MFS_LED_D1].bsrr_off_mask |
        s_led_hw_map[MFS_LED_D2].bsrr_off_mask |
        s_led_hw_map[MFS_LED_D3].bsrr_off_mask;
    GPIOB->BSRR = s_led_hw_map[MFS_LED_D4].bsrr_off_mask;
    s_driver_context.led_state_mask = 0U;
}

void mfs_led_set(
    mfs_led_t led,
    bool is_on
)
{
    if (!mfs_led_is_valid(led))
    {
        return;
    }

    if (!s_driver_context.is_initialized)
    {
        mfs_led_driver_init();
    }

    mfs_led_write_state(led, is_on);
}

void mfs_led_toggle(mfs_led_t led)
{
    if (!mfs_led_is_valid(led))
    {
        return;
    }

    if (!s_driver_context.is_initialized)
    {
        mfs_led_driver_init();
    }

    const bool led_is_on = (s_driver_context.led_state_mask & (uint8_t)(1U << (uint8_t)led)) != 0U;
    mfs_led_write_state(led, !led_is_on);
}
