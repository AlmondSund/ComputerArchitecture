#ifndef APP_FSM_H
#define APP_FSM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "display.h"
#include "indicators.h"
#include "keypad.h"
#include "config.h"
#include "storage.h"

typedef enum {
    STATE_IDLE = 0,
    STATE_ENTER_PIN,
    STATE_VALIDATE_PIN,
    STATE_ACCESS_GRANTED,
    STATE_ACCESS_DENIED,
    STATE_LOCKOUT,
    STATE_ADMIN_AUTH,
    STATE_ADMIN_MENU,
    STATE_CHANGE_PIN,
    STATE_SET_LOCKOUT,
    STATE_DIAGNOSTICS
} State;

typedef enum {
    MENU_CHANGE_PIN = 0,
    MENU_SET_LOCKOUT,
    MENU_TOGGLE_SOUND,
    MENU_SHOW_FAILS,
    MENU_DIAGNOSTICS,
    MENU_COUNT
} AdminMenuItem;

typedef enum {
    VALIDATION_NONE = 0,
    VALIDATION_USER_PIN,
    VALIDATION_ADMIN_PIN
} ValidationMode;

typedef enum {
    DIAG_COUNTER = 0,
    DIAG_STATUS
} DiagnosticsMode;

/*
 * Full logical application state. Hardware drivers read display/indicator
 * intent and feed keypad events into the queue.
 */
typedef struct {
    State state;
    State next_state;
    ValidationMode validation_mode;
    AdminMenuItem menu_item;
    DiagnosticsMode diagnostics_mode;

    Storage storage;
    Keypad keypad;
    Display display;
    Indicators indicators;

    char entry_buffer[ADMIN_PIN_LENGTH + 1U];
    size_t entry_length;

    uint32_t state_elapsed_ms;
    uint32_t state_timeout_ms;
    uint32_t lockout_remaining_ms;
    uint32_t keypress_feedback_ms;
    char transient_text[DISPLAY_WIDTH + 1U];
} App;

void app_init(App *app);
bool app_inject_key(App *app, char key);
void app_tick(App *app, uint32_t elapsed_ms);
const char *app_state_name(State state);

#endif
