#include "bsp/led_pwm.h"

#include "bsp/board.h"
#include "stm32l4xx_hal.h"

#define LED_PWM_COUNTER_CLOCK_HZ 1000000U
#define LED_PWM_PERIOD_TICKS ((LED_PWM_COUNTER_CLOCK_HZ / BSP_LED_PWM_FREQUENCY_HZ) - 1U)

static TIM_HandleTypeDef htim_led_pwm;
static uint16_t current_duty_permille;

static uint32_t TIM2_GetInputClockHz(void)
{
    uint32_t pclk1_hz = HAL_RCC_GetPCLK1Freq();

    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1)
    {
        pclk1_hz *= 2U;
    }

    return pclk1_hz;
}

static void LED_PWM_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = BSP_LED_PWM_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = BSP_LED_PWM_GPIO_AF;

    HAL_GPIO_Init(BSP_LED_PWM_GPIO_PORT, &GPIO_InitStruct);
}

BSP_LED_PWM_Status BSP_LED_PWM_Init(void)
{
    TIM_OC_InitTypeDef pwm_config = {0};
    uint32_t tim_input_hz = TIM2_GetInputClockHz();

    if (tim_input_hz < LED_PWM_COUNTER_CLOCK_HZ)
    {
        return BSP_LED_PWM_ERROR;
    }

    __HAL_RCC_TIM2_CLK_ENABLE();
    LED_PWM_GPIO_Init();

    htim_led_pwm.Instance = BSP_LED_PWM_TIM;
    htim_led_pwm.Init.Prescaler = (tim_input_hz / LED_PWM_COUNTER_CLOCK_HZ) - 1U;
    htim_led_pwm.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim_led_pwm.Init.Period = LED_PWM_PERIOD_TICKS;
    htim_led_pwm.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim_led_pwm.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_PWM_Init(&htim_led_pwm) != HAL_OK)
    {
        return BSP_LED_PWM_ERROR;
    }

    pwm_config.OCMode = TIM_OCMODE_PWM1;
    pwm_config.Pulse = 0U;
    pwm_config.OCPolarity = TIM_OCPOLARITY_HIGH;
    pwm_config.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(&htim_led_pwm, &pwm_config, BSP_LED_PWM_TIM_CHANNEL) != HAL_OK)
    {
        return BSP_LED_PWM_ERROR;
    }

    if (HAL_TIM_PWM_Start(&htim_led_pwm, BSP_LED_PWM_TIM_CHANNEL) != HAL_OK)
    {
        return BSP_LED_PWM_ERROR;
    }

    BSP_LED_PWM_SetDutyPermille(0U);
    return BSP_LED_PWM_OK;
}

void BSP_LED_PWM_SetDutyPermille(uint16_t duty_permille)
{
    uint32_t pwm_counts = LED_PWM_PERIOD_TICKS + 1U;
    uint32_t compare;

    if (duty_permille > BSP_LED_PWM_DUTY_SCALE)
    {
        duty_permille = BSP_LED_PWM_DUTY_SCALE;
    }

    compare = (pwm_counts * (uint32_t)duty_permille) / BSP_LED_PWM_DUTY_SCALE;
    __HAL_TIM_SET_COMPARE(&htim_led_pwm, BSP_LED_PWM_TIM_CHANNEL, compare);

    current_duty_permille = duty_permille;
}

uint16_t BSP_LED_PWM_GetDutyPermille(void)
{
    return current_duty_permille;
}
