#include "blink_timebase.h"

#include <stddef.h>

/* These constants match the assembly implementation assumptions. */
#define RCC_HSE_VALUE_HZ (8000000UL)
#define RCC_HSI16_VALUE_HZ (16000000UL)

/* Target timer clock source is APB1 timer clock (TIM2..TIM5). */
#define RCC_CFGR_SWS_SHIFT (2U)
#define RCC_CFGR_HPRE_SHIFT (4U)
#define RCC_CFGR_PPRE1_SHIFT (8U)

/* PLLCFGR bit fields used to reconstruct PLLR output frequency. */
#define RCC_PLLCFGR_PLLSRC_MASK (0x3UL)
#define RCC_PLLCFGR_PLLM_SHIFT (4U)
#define RCC_PLLCFGR_PLLM_MASK (0x7UL)
#define RCC_PLLCFGR_PLLN_SHIFT (8U)
#define RCC_PLLCFGR_PLLN_MASK (0x7FUL)
#define RCC_PLLCFGR_PLLR_SHIFT (25U)
#define RCC_PLLCFGR_PLLR_MASK (0x3UL)

/* RCC CR/CSR fields needed to decode current MSI frequency. */
#define RCC_CR_MSIRGSEL_BIT (3U)
#define RCC_CR_MSIRANGE_SHIFT (4U)
#define RCC_CR_MSIRANGE_MASK (0xFUL)
#define RCC_CSR_MSISRANGE_SHIFT (8U)
#define RCC_CSR_MSISRANGE_MASK (0xFUL)

/**
 * @brief Returns MSI oscillator frequency from RCC range configuration.
 *
 * @return MSI clock frequency [Hz].
 */
static uint32_t blink_rcc_get_msi_hz(void)
{
    static const uint32_t msi_freq_table_hz[16] = {
        100000UL, 200000UL, 400000UL, 800000UL,
        1000000UL, 2000000UL, 4000000UL, 8000000UL,
        16000000UL, 24000000UL, 32000000UL, 48000000UL,
        0UL, 0UL, 0UL, 0UL
    };

    uint32_t range_index = 0UL;

    /*
     * MSIRGSEL selects where the effective range comes from:
     * - 1: RCC_CR.MSIRANGE
     * - 0: RCC_CSR.MSISRANGE
     */
    if ((RCC->CR & (1UL << RCC_CR_MSIRGSEL_BIT)) != 0UL)
    {
        range_index = (RCC->CR >> RCC_CR_MSIRANGE_SHIFT) & RCC_CR_MSIRANGE_MASK;
    }
    else
    {
        range_index = (RCC->CSR >> RCC_CSR_MSISRANGE_SHIFT) & RCC_CSR_MSISRANGE_MASK;
    }

    return msi_freq_table_hz[range_index];
}

/**
 * @brief Reconstructs PLLR output frequency from RCC_PLLCFGR.
 *
 * @return PLLR clock frequency [Hz]. Returns 0 if PLL source is invalid.
 */
static uint32_t blink_rcc_get_pll_r_hz(void)
{
    const uint32_t pllcfgr = RCC->PLLCFGR;

    uint32_t pll_src_hz = 0UL;
    const uint32_t pll_src = pllcfgr & RCC_PLLCFGR_PLLSRC_MASK;
    if (pll_src == 1UL)
    {
        pll_src_hz = blink_rcc_get_msi_hz();
    }
    else if (pll_src == 2UL)
    {
        pll_src_hz = RCC_HSI16_VALUE_HZ;
    }
    else if (pll_src == 3UL)
    {
        pll_src_hz = RCC_HSE_VALUE_HZ;
    }
    else
    {
        return 0UL;
    }

    const uint32_t pll_m_div = ((pllcfgr >> RCC_PLLCFGR_PLLM_SHIFT) & RCC_PLLCFGR_PLLM_MASK) + 1UL;
    const uint32_t pll_n_mul = (pllcfgr >> RCC_PLLCFGR_PLLN_SHIFT) & RCC_PLLCFGR_PLLN_MASK;
    const uint32_t pll_r_div = 2UL * (((pllcfgr >> RCC_PLLCFGR_PLLR_SHIFT) & RCC_PLLCFGR_PLLR_MASK) + 1UL);

    if ((pll_m_div == 0UL) || (pll_n_mul == 0UL) || (pll_r_div == 0UL))
    {
        return 0UL;
    }

    const uint32_t pll_vco_in_hz = pll_src_hz / pll_m_div;
    const uint32_t pll_vco_hz = pll_vco_in_hz * pll_n_mul;
    return pll_vco_hz / pll_r_div;
}

/**
 * @brief Computes TIMCLK1 frequency used by TIM2..TIM5.
 *
 * It follows the same sequence as the assembly version:
 * 1) Determine SYSCLK from SWS.
 * 2) Apply HPRE to get HCLK.
 * 3) Apply PPRE1 to get PCLK1.
 * 4) Apply APB1 timer x2 rule when PPRE1 is not /1.
 *
 * @return TIMCLK1 frequency [Hz].
 */
static uint32_t blink_rcc_get_timclk1_hz(void)
{
    static const uint32_t hpre_div_table[16] = {
        1UL, 1UL, 1UL, 1UL, 1UL, 1UL, 1UL, 1UL,
        2UL, 4UL, 8UL, 16UL, 64UL, 128UL, 256UL, 512UL
    };
    static const uint32_t ppre_div_table[8] = {
        1UL, 1UL, 1UL, 1UL, 2UL, 4UL, 8UL, 16UL
    };

    const uint32_t cfgr = RCC->CFGR;

    /* Decode current SYSCLK source selected by the clock tree. */
    const uint32_t sws = (cfgr >> RCC_CFGR_SWS_SHIFT) & 0x3UL;
    uint32_t sysclk_hz = 0UL;
    if (sws == 0UL)
    {
        sysclk_hz = blink_rcc_get_msi_hz();
    }
    else if (sws == 1UL)
    {
        sysclk_hz = RCC_HSI16_VALUE_HZ;
    }
    else if (sws == 2UL)
    {
        sysclk_hz = RCC_HSE_VALUE_HZ;
    }
    else
    {
        sysclk_hz = blink_rcc_get_pll_r_hz();
    }

    const uint32_t hpre_index = (cfgr >> RCC_CFGR_HPRE_SHIFT) & 0xFUL;
    const uint32_t hclk_hz = sysclk_hz / hpre_div_table[hpre_index];

    const uint32_t ppre1_index = (cfgr >> RCC_CFGR_PPRE1_SHIFT) & 0x7UL;
    const uint32_t pclk1_hz = hclk_hz / ppre_div_table[ppre1_index];

    /* When APB prescaler is /2 or more, timer clock runs at 2*PCLK. */
    if (ppre1_index >= 4UL)
    {
        return 2UL * pclk1_hz;
    }

    return pclk1_hz;
}

void blink_timebase_enable_timer_clocks(void)
{
    RCC->APB1ENR1 |=
        RCC_APB1ENR1_TIM2EN |
        RCC_APB1ENR1_TIM3EN |
        RCC_APB1ENR1_TIM4EN |
        RCC_APB1ENR1_TIM5EN;

    (void)RCC->APB1ENR1;
    __DSB();
    __ISB();
}

uint32_t blink_timebase_compute_psc_for_target_hz(uint32_t target_tick_hz)
{
    if (target_tick_hz == 0UL)
    {
        return 0UL;
    }

    const uint32_t timclk1_hz = blink_rcc_get_timclk1_hz();
    if (timclk1_hz < target_tick_hz)
    {
        return 0UL;
    }

    return (timclk1_hz / target_tick_hz) - 1UL;
}

void blink_timebase_start_timer(
    TIM_TypeDef *timer,
    uint32_t auto_reload_tick,
    uint32_t prescaler_tick
)
{
    if (timer == NULL)
    {
        return;
    }

    /* Program base time, force shadow transfer, clear UIF, and enable counter. */
    timer->PSC = prescaler_tick;
    timer->ARR = auto_reload_tick;
    timer->EGR = TIM_EGR_UG;
    timer->SR = 0UL;
    timer->CR1 = TIM_CR1_CEN;
}

bool blink_timebase_consume_update_event(TIM_TypeDef *timer)
{
    if (timer == NULL)
    {
        return false;
    }

    if ((timer->SR & TIM_SR_UIF) == 0UL)
    {
        return false;
    }

    timer->SR = 0UL;
    return true;
}
