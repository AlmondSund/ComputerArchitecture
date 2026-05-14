#include "timebase.h"

#include <stdint.h>

#include "stm32l476xx.h"

static volatile uint32_t g_ms = 0U;

void SysTick_Handler(void)
{
    ++g_ms;
}

void timebase_init(void)
{
    SysTick_Config(SystemCoreClock / 1000U);
}

uint32_t timebase_now_ms(void)
{
    return g_ms;
}
