#ifndef BLINK_TIMEBASE_H
#define BLINK_TIMEBASE_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32l476xx.h"

/**
 * @brief Enables TIM2/TIM3/TIM4/TIM5 clocks on APB1.
 */
void blink_timebase_enable_timer_clocks(void);

/**
 * @brief Computes PSC so timer tick is close to the requested frequency.
 *
 * Formula: PSC = (TIMCLK1_HZ / target_tick_hz) - 1
 *
 * @param target_tick_hz Desired timer counter tick frequency [Hz].
 * @return Prescaler register value [ticks - 1].
 */
uint32_t blink_timebase_compute_psc_for_target_hz(uint32_t target_tick_hz);

/**
 * @brief Starts one timer in free-running update mode.
 *
 * @param timer            Timer peripheral pointer (TIM2..TIM5).
 * @param auto_reload_tick ARR value [ticks - 1].
 * @param prescaler_tick   PSC value [ticks - 1].
 */
void blink_timebase_start_timer(
    TIM_TypeDef *timer,
    uint32_t auto_reload_tick,
    uint32_t prescaler_tick
);

/**
 * @brief Consumes one update event flag from a timer.
 *
 * @param timer Timer peripheral pointer.
 * @return true if an update event was pending, false otherwise.
 */
bool blink_timebase_consume_update_event(TIM_TypeDef *timer);

#endif /* BLINK_TIMEBASE_H */
