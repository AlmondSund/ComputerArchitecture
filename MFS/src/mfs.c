#include <stdint.h>

#include "stm32l476xx.h"

#define BLINKS_PER_SECOND (1)
#define LED_PIN_PA5 (5UL)
#define SYSTICK_TICK_HZ (1000UL)
#define HALF_PERIOD_MS ((uint32_t)(500.0 / BLINKS_PER_SECOND))

/* Waits a blocking delay by polling the SysTick COUNTFLAG every 1 ms tick. */
static void delay_ms(
    uint32_t delay_ms
)
{
    while (delay_ms > 0U)
    {
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0U)
        {
            /* Busy wait until one 1 ms tick elapses. */
        }

        --delay_ms;
    }
}

/* This program configures PA5 on STM32L476RG for LED blinking output. */
int main(void)
{
    /* Enable the GPIOA clock before touching PA5 registers. */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    /* Configure PA5 as push-pull output mode. */
    GPIOA->MODER &= ~(0x3UL << (LED_PIN_PA5 * 2UL));
    GPIOA->MODER |= (0x1UL << (LED_PIN_PA5 * 2UL));

    /* Start with LED off (PA5 = 0). */
    GPIOA->BSRR = (1UL << (LED_PIN_PA5 + 16UL));

    /*
     * Configure SysTick to roll over every 1 ms:
     *   reload = f_core / 1000 - 1
     */
    SystemCoreClockUpdate();
    SysTick->LOAD = (SystemCoreClock / SYSTICK_TICK_HZ) - 1UL;
    SysTick->VAL = 0UL;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    while (1)
    {
        /* Superloop: wait half-period, then toggle PA5. */
        delay_ms(HALF_PERIOD_MS);
        GPIOA->ODR ^= (1UL << LED_PIN_PA5);
    }
}
