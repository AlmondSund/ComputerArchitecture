#include "app/led_blink_fade_app.h"

#include <stdint.h>

#include "bsp/led_pwm.h"
#include "service/perceptual_brightness.h"
#include "stm32l4xx_hal.h"

#define LED_BLINK_FADE_PERIOD_MS 1000U
#define LED_BLINK_FADE_HALF_PERIOD_MS (LED_BLINK_FADE_PERIOD_MS / 2U)
#define LED_BLINK_FADE_TASK_PERIOD_MS 10U

typedef enum
{
    LED_BLINK_FADE_STATE_UP = 0,
    LED_BLINK_FADE_STATE_DOWN
} LedBlinkFadeState;

typedef struct
{
    LedBlinkFadeState state;
    uint32_t state_started_ms;
    uint32_t last_task_ms;
} LedBlinkFadeContext;

static LedBlinkFadeContext app;

static uint8_t ScaleElapsedToBrightness(uint32_t elapsed_ms)
{
    if (elapsed_ms >= LED_BLINK_FADE_HALF_PERIOD_MS)
    {
        return PERCEPTUAL_BRIGHTNESS_MAX_LEVEL;
    }

    return (uint8_t)(((elapsed_ms * PERCEPTUAL_BRIGHTNESS_MAX_LEVEL) +
                      (LED_BLINK_FADE_HALF_PERIOD_MS / 2U)) /
                     LED_BLINK_FADE_HALF_PERIOD_MS);
}

static void ApplyVisualLevel(uint8_t visual_level)
{
    uint16_t duty_permille = PerceptualBrightness_ToDutyPermille(visual_level);
    BSP_LED_PWM_SetDutyPermille(duty_permille);
}

static void AdvanceStateMachine(uint32_t now_ms)
{
    while ((now_ms - app.state_started_ms) >= LED_BLINK_FADE_HALF_PERIOD_MS)
    {
        app.state_started_ms += LED_BLINK_FADE_HALF_PERIOD_MS;
        app.state = (app.state == LED_BLINK_FADE_STATE_UP) ?
                        LED_BLINK_FADE_STATE_DOWN :
                        LED_BLINK_FADE_STATE_UP;
    }
}

void APP_LedBlinkFade_Init(void)
{
    uint32_t now_ms = HAL_GetTick();

    app.state = LED_BLINK_FADE_STATE_UP;
    app.state_started_ms = now_ms;
    app.last_task_ms = now_ms;

    ApplyVisualLevel(0U);
}

void APP_LedBlinkFade_Task(void)
{
    uint32_t now_ms = HAL_GetTick();
    uint32_t elapsed_ms;
    uint8_t visual_level;

    if ((now_ms - app.last_task_ms) < LED_BLINK_FADE_TASK_PERIOD_MS)
    {
        return;
    }

    app.last_task_ms = now_ms;
    AdvanceStateMachine(now_ms);

    elapsed_ms = now_ms - app.state_started_ms;
    visual_level = ScaleElapsedToBrightness(elapsed_ms);

    if (app.state == LED_BLINK_FADE_STATE_DOWN)
    {
        visual_level = (uint8_t)(PERCEPTUAL_BRIGHTNESS_MAX_LEVEL - visual_level);
    }

    ApplyVisualLevel(visual_level);
}
