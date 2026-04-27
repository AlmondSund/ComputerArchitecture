#include "main.h"

#include "app/led_blink_fade_app.h"
#include "bsp/led_pwm.h"
#include "platform/error_handler.h"
#include "platform/system_clock.h"

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    if (BSP_LED_PWM_Init() != BSP_LED_PWM_OK)
    {
        Error_Handler();
    }

    APP_LedBlinkFade_Init();

    while (1)
    {
        APP_LedBlinkFade_Task();
    }
}
