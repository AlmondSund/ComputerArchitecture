#include "app_fsm.h"

#include <stddef.h>
#include <string.h>

static uint8_t app_clamp_led_index(uint8_t index)
{
    return (index < LED_COUNT) ? index : 0U;
}

static uint8_t app_percent_decrease(uint8_t percent)
{
    if (percent <= BRIGHTNESS_STEP_PERCENT) {
        return 0U;
    }

    return (uint8_t)(percent - BRIGHTNESS_STEP_PERCENT);
}

static uint8_t app_percent_increase(uint8_t percent)
{
    if (percent >= (uint8_t)(BRIGHTNESS_PERCENT_MAX - BRIGHTNESS_STEP_PERCENT)) {
        return BRIGHTNESS_PERCENT_MAX;
    }

    return (uint8_t)(percent + BRIGHTNESS_STEP_PERCENT);
}

/*
 * Approximate perceptual brightness correction with a gamma-2 transfer so the
 * user-facing percentage is closer to how the eye perceives brightness than a
 * raw linear PWM duty cycle would be.
 */
static uint8_t app_percent_to_pwm(uint8_t percent)
{
    const uint32_t clamped = (percent > BRIGHTNESS_PERCENT_MAX)
                                 ? BRIGHTNESS_PERCENT_MAX
                                 : percent;

    return (uint8_t)(((clamped * clamped * LED_PWM_MAX) + 5000U) / 10000U);
}

static void app_sync_led_output(App *app, uint8_t led_index)
{
    const uint8_t index = app_clamp_led_index(led_index);

    app->led_pwm_levels[index] =
        app_percent_to_pwm(app->led_brightness_percent[index]);
}

static void app_render_selection(App *app)
{
    char buffer[DISPLAY_WIDTH + 1U] = {'S', 'E', 'L', '1', '\0'};

    buffer[3] = (char)('1' + app_clamp_led_index(app->selection_led));
    display_show_text(&app->display, buffer);
}

static void app_render_dimming(App *app)
{
    char buffer[DISPLAY_WIDTH + 1U] = {'D', '1', ' ', ' ', '\0'};

    if (((app->display_elapsed_ms / DISPLAY_PAGE_MS) % 2U) == 0U) {
        buffer[1] = (char)('1' + app_clamp_led_index(app->active_led));
        display_show_text(&app->display, buffer);
    } else {
        display_show_number(&app->display,
                            app->led_brightness_percent[app->active_led],
                            false);
    }
}

static void app_render(App *app)
{
    if (app->state == STATE_SELECT_LED) {
        app_render_selection(app);
    } else {
        app_render_dimming(app);
    }
}

static void app_move_selection(App *app, int direction)
{
    int next = (int)app->selection_led + direction;

    if (next < 0) {
        next = (int)LED_COUNT - 1;
    } else if (next >= (int)LED_COUNT) {
        next = 0;
    }

    app->selection_led = (uint8_t)next;
}

static void app_adjust_active_level(App *app, ButtonEvent button)
{
    uint8_t *percent = &app->led_brightness_percent[app->active_led];

    if (button == BUTTON_DECREASE) {
        *percent = app_percent_decrease(*percent);
    } else if (button == BUTTON_INCREASE) {
        *percent = app_percent_increase(*percent);
    }

    app_sync_led_output(app, app->active_led);
}

static void app_handle_button(App *app, ButtonEvent button)
{
    if (app->state == STATE_SELECT_LED) {
        if (button == BUTTON_DECREASE) {
            app_move_selection(app, -1);
        } else if (button == BUTTON_INCREASE) {
            app_move_selection(app, 1);
        } else if (button == BUTTON_SELECT_LONG) {
            app->active_led = app->selection_led;
            app->state = STATE_DIMMING;
            app->display_elapsed_ms = 0U;
        }
        return;
    }

    if ((button == BUTTON_DECREASE) || (button == BUTTON_INCREASE)) {
        app_adjust_active_level(app, button);
    } else if (button == BUTTON_SELECT_SHORT) {
        app->selection_led = app->active_led;
        app->state = STATE_SELECT_LED;
        app->display_elapsed_ms = 0U;
    }
}

void app_init(App *app)
{
    if (app == NULL) {
        return;
    }

    memset(app, 0, sizeof(*app));
    buttons_init(&app->buttons);
    display_init(&app->display);

    app->state = STATE_DIMMING;
    app->active_led = 0U;
    app->selection_led = 0U;
    app_render(app);
}

bool app_inject_button(App *app, ButtonEvent button)
{
    if (app == NULL) {
        return false;
    }

    return buttons_push(&app->buttons, button);
}

void app_tick(App *app, uint32_t elapsed_ms)
{
    ButtonEvent button;

    if (app == NULL) {
        return;
    }

    app->display_elapsed_ms += elapsed_ms;

    while (buttons_pop(&app->buttons, &button)) {
        app_handle_button(app, button);
    }

    app_render(app);
}

const char *app_state_name(AppState state)
{
    switch (state) {
    case STATE_DIMMING:
        return "DIMMING";
    case STATE_SELECT_LED:
        return "SELECT_LED";
    default:
        return "UNKNOWN";
    }
}
