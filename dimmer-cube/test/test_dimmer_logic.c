#include "app_fsm.h"

#include <assert.h>
#include <string.h>

static void tick_for(App *app, uint32_t duration_ms)
{
    uint32_t elapsed = 0U;

    while (elapsed < duration_ms) {
        app_tick(app, APP_TICK_MS);
        elapsed += APP_TICK_MS;
    }
}

static void test_buttons_adjust_active_led_brightness(void)
{
    App app;

    app_init(&app);
    assert(app_inject_button(&app, BUTTON_INCREASE));
    app_tick(&app, APP_TICK_MS);

    assert(app.state == STATE_DIMMING);
    assert(app.led_brightness_percent[0] == BRIGHTNESS_STEP_PERCENT);
    assert(app.led_pwm_levels[0] == 1U);
    assert(app.led_brightness_percent[1] == 0U);
    assert(app.led_pwm_levels[1] == 0U);

    assert(app_inject_button(&app, BUTTON_DECREASE));
    app_tick(&app, APP_TICK_MS);
    assert(app.led_brightness_percent[0] == 0U);
    assert(app.led_pwm_levels[0] == 0U);
}

static void test_fifty_percent_uses_perceptual_pwm_mapping(void)
{
    App app;
    uint32_t step;

    app_init(&app);

    for (step = 0U; step < 10U; ++step) {
        assert(app_inject_button(&app, BUTTON_INCREASE));
        app_tick(&app, APP_TICK_MS);
    }

    assert(app.led_brightness_percent[0] == 50U);
    assert(app.led_pwm_levels[0] == 64U);
}

static void test_short_press_enters_selection_and_long_press_commits(void)
{
    App app;

    app_init(&app);
    assert(app_inject_button(&app, BUTTON_INCREASE));
    app_tick(&app, APP_TICK_MS);

    assert(app_inject_button(&app, BUTTON_SELECT_SHORT));
    app_tick(&app, APP_TICK_MS);
    assert(app.state == STATE_SELECT_LED);
    assert(strcmp(app.display.text, "SEL1") == 0);

    assert(app_inject_button(&app, BUTTON_INCREASE));
    app_tick(&app, APP_TICK_MS);
    assert(app.selection_led == 1U);
    assert(strcmp(app.display.text, "SEL2") == 0);

    assert(app_inject_button(&app, BUTTON_DECREASE));
    app_tick(&app, APP_TICK_MS);
    assert(app.selection_led == 0U);
    assert(strcmp(app.display.text, "SEL1") == 0);

    assert(app_inject_button(&app, BUTTON_INCREASE));
    app_tick(&app, APP_TICK_MS);
    assert(app.selection_led == 1U);

    assert(app.led_brightness_percent[0] == BRIGHTNESS_STEP_PERCENT);
    assert(app.led_pwm_levels[0] == 1U);
    assert(app.led_brightness_percent[1] == 0U);
    assert(app.led_pwm_levels[1] == 0U);

    assert(app_inject_button(&app, BUTTON_SELECT_LONG));
    app_tick(&app, APP_TICK_MS);
    assert(app.state == STATE_DIMMING);
    assert(app.active_led == 1U);
    assert(app.led_brightness_percent[1] == 0U);
    assert(app.led_pwm_levels[1] == 0U);
}

static void test_selection_wraps_in_both_directions(void)
{
    App app;

    app_init(&app);
    assert(app_inject_button(&app, BUTTON_SELECT_SHORT));
    app_tick(&app, APP_TICK_MS);

    assert(app_inject_button(&app, BUTTON_DECREASE));
    app_tick(&app, APP_TICK_MS);
    assert(app.selection_led == 3U);

    assert(app_inject_button(&app, BUTTON_INCREASE));
    app_tick(&app, APP_TICK_MS);
    assert(app.selection_led == 0U);
}

static void test_display_alternates_between_led_and_percent(void)
{
    App app;
    uint32_t step;

    app_init(&app);

    for (step = 0U; step < 10U; ++step) {
        assert(app_inject_button(&app, BUTTON_INCREASE));
        app_tick(&app, APP_TICK_MS);
    }
    assert(strcmp(app.display.text, "D1  ") == 0);

    tick_for(&app, DISPLAY_PAGE_MS);
    assert(strcmp(app.display.text, "  50") == 0);
}

int main(void)
{
    test_buttons_adjust_active_led_brightness();
    test_fifty_percent_uses_perceptual_pwm_mapping();
    test_short_press_enters_selection_and_long_press_commits();
    test_selection_wraps_in_both_directions();
    test_display_alternates_between_led_and_percent();
    return 0;
}
