#ifndef APP_FSM_H
#define APP_FSM_H

#include <stdbool.h>
#include <stdint.h>

#include "buttons.h"
#include "config.h"
#include "display.h"

typedef enum {
    STATE_DIMMING = 0,
    STATE_SELECT_LED
} AppState;

/*
 * Full logical dimmer state. Hardware drivers queue debounced button events;
 * the application tick owns all state transitions, brightness updates, and
 * display rendering.
 */
typedef struct {
    AppState state;
    ButtonQueue buttons;
    Display display;

    uint8_t led_brightness_percent[LED_COUNT];
    uint8_t led_pwm_levels[LED_COUNT];
    uint8_t active_led;
    uint8_t selection_led;
    uint32_t display_elapsed_ms;
} App;

void app_init(App *app);
bool app_inject_button(App *app, ButtonEvent button);
void app_tick(App *app, uint32_t elapsed_ms);
const char *app_state_name(AppState state);

#endif
