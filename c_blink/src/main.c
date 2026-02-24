#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "blink_timebase.h"
#include "mfs_led_driver.h"

#ifndef STM32L476xx
#error "This project targets STM32L476xx (NUCLEO-L476RG)."
#endif

/* Timer tick target used by the original assembly implementation. */
#define BLINK_TIMER_TICK_HZ (32000UL)

/* ARR values that produce toggle frequencies for each LED channel. */
#define BLINK_D1_TOGGLE_ARR (15999UL) /* ~2 Hz toggle -> 1 blink/s */
#define BLINK_D2_TOGGLE_ARR (7999UL)  /* ~4 Hz toggle -> 2 blinks/s */
#define BLINK_D3_TOGGLE_ARR (5332UL)  /* ~6 Hz toggle -> 3 blinks/s */
#define BLINK_D4_TOGGLE_ARR (3999UL)  /* ~8 Hz toggle -> 4 blinks/s */

typedef struct
{
    TIM_TypeDef *timer;
    mfs_led_t led;
    uint32_t arr_ticks;
    bool led_is_on;
} blink_channel_t;

static blink_channel_t s_blink_channels[] = {
    {.timer = TIM2, .led = MFS_LED_D1, .arr_ticks = BLINK_D1_TOGGLE_ARR, .led_is_on = false},
    {.timer = TIM3, .led = MFS_LED_D2, .arr_ticks = BLINK_D2_TOGGLE_ARR, .led_is_on = false},
    {.timer = TIM4, .led = MFS_LED_D3, .arr_ticks = BLINK_D3_TOGGLE_ARR, .led_is_on = false},
    {.timer = TIM5, .led = MFS_LED_D4, .arr_ticks = BLINK_D4_TOGGLE_ARR, .led_is_on = false}
};

/**
 * @brief Configures all timers with one shared prescaler and per-channel ARR.
 */
static void blink_channels_init(void)
{
    blink_timebase_enable_timer_clocks();

    const uint32_t prescaler_ticks = blink_timebase_compute_psc_for_target_hz(BLINK_TIMER_TICK_HZ);

    /* Start each channel timer with the same timebase and dedicated ARR period. */
    const size_t channel_count = sizeof(s_blink_channels) / sizeof(s_blink_channels[0]);
    for (size_t channel_index = 0U; channel_index < channel_count; ++channel_index)
    {
        blink_channel_t *const channel = &s_blink_channels[channel_index];
        blink_timebase_start_timer(channel->timer, channel->arr_ticks, prescaler_ticks);
    }
}

/**
 * @brief Polls timer update flags once and applies LED toggles.
 */
static void blink_channels_process_once(void)
{
    const size_t channel_count = sizeof(s_blink_channels) / sizeof(s_blink_channels[0]);
    for (size_t channel_index = 0U; channel_index < channel_count; ++channel_index)
    {
        blink_channel_t *const channel = &s_blink_channels[channel_index];
        if (!blink_timebase_consume_update_event(channel->timer))
        {
            continue;
        }

        channel->led_is_on = !channel->led_is_on;
        mfs_led_set(channel->led, channel->led_is_on);
    }
}

int main(void)
{
    /*
     * Initialize shield LED driver first so GPIO state is deterministic before
     * timers start generating update events.
     */
    mfs_led_driver_init();
    mfs_led_all_off();

    blink_channels_init();

    while (1)
    {
        blink_channels_process_once();
    }
}
