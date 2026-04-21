#include "app_fsm.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static void app_set_state(App *app, State state)
{
    app->state = state;
    app->state_elapsed_ms = 0U;
}

static void app_reset_entry(App *app)
{
    size_t index;

    app->entry_length = 0U;

    for (index = 0U; index < sizeof(app->entry_buffer); ++index) {
        app->entry_buffer[index] = '\0';
    }
}

static void app_store_transient_text(App *app, const char *text)
{
    size_t index;

    for (index = 0U; index < DISPLAY_WIDTH; ++index) {
        if ((text != NULL) && (text[index] != '\0')) {
            app->transient_text[index] = text[index];
        } else {
            app->transient_text[index] = ' ';
        }
    }

    app->transient_text[DISPLAY_WIDTH] = '\0';
}

static void app_enter_idle(App *app)
{
    app_reset_entry(app);
    app->validation_mode = VALIDATION_NONE;
    app->state_timeout_ms = 0U;
    app->next_state = STATE_IDLE;
    app->diagnostics_mode = DIAG_COUNTER;
    app_store_transient_text(app, "SAFE");
    app_set_state(app, STATE_IDLE);
}

static void app_enter_admin_menu(App *app)
{
    app_reset_entry(app);
    app->validation_mode = VALIDATION_NONE;
    app->state_timeout_ms = 0U;
    app->next_state = STATE_ADMIN_MENU;
    app_set_state(app, STATE_ADMIN_MENU);
}

static void app_begin_transient(App *app,
                                     State state,
                                     const char *text,
                                     uint32_t duration_ms,
                                     State next_state)
{
    app_store_transient_text(app, text);
    app->state_timeout_ms = duration_ms;
    app->next_state = next_state;
    app_set_state(app, state);
}

static void app_begin_lockout(App *app)
{
    app_reset_entry(app);
    app->validation_mode = VALIDATION_NONE;
    app->lockout_remaining_ms = app->storage.lockout_duration_ms;
    app->state_timeout_ms = 0U;
    app->next_state = STATE_IDLE;
    app_set_state(app, STATE_LOCKOUT);
}

static bool app_append_digit(App *app, char digit, size_t limit)
{
    if ((digit < '0') || (digit > '9') || (app->entry_length >= limit)) {
        return false;
    }

    app->entry_buffer[app->entry_length] = digit;
    app->entry_length++;
    app->entry_buffer[app->entry_length] = '\0';
    return true;
}

static void app_backspace(App *app)
{
    if (app->entry_length == 0U) {
        return;
    }

    app->entry_length--;
    app->entry_buffer[app->entry_length] = '\0';
}

static bool app_matches_pin(const char *buffer, const char *pin, size_t length)
{
    return (buffer != NULL) &&
           (pin != NULL) &&
           (strlen(buffer) == length) &&
           (strncmp(buffer, pin, length) == 0);
}

static uint32_t app_parse_entry_number(const App *app)
{
    size_t index;
    uint32_t value = 0U;

    for (index = 0U; index < app->entry_length; ++index) {
        value = (value * 10U) + (uint32_t)(app->entry_buffer[index] - '0');
    }

    return value;
}

static void app_cycle_menu(App *app, int direction)
{
    int next = (int)app->menu_item + direction;

    if (next < 0) {
        next = (int)MENU_COUNT - 1;
    } else if (next >= (int)MENU_COUNT) {
        next = 0;
    }

    app->menu_item = (AdminMenuItem)next;
}

static void app_enter_diagnostics(App *app, DiagnosticsMode mode)
{
    app_reset_entry(app);
    app->diagnostics_mode = mode;
    app->state_timeout_ms = 0U;
    app->next_state = STATE_ADMIN_MENU;
    app_set_state(app, STATE_DIAGNOSTICS);
}

static void app_handle_validation(App *app)
{
    if (app->validation_mode == VALIDATION_USER_PIN) {
        if (app_matches_pin(app->entry_buffer, app->storage.user_pin, USER_PIN_LENGTH)) {
            storage_reset_failed_attempts(&app->storage);
            app_begin_transient(app,
                                     STATE_ACCESS_GRANTED,
                                     "OPEN",
                                     ACCESS_GRANTED_MS,
                                     STATE_IDLE);
        } else {
            storage_increment_failed_attempts(&app->storage);
            if (app->storage.failed_attempts >= app->storage.max_failed_attempts) {
                app_begin_transient(app,
                                         STATE_ACCESS_DENIED,
                                         "FAIL",
                                         ACCESS_DENIED_MS,
                                         STATE_LOCKOUT);
            } else {
                app_begin_transient(app,
                                         STATE_ACCESS_DENIED,
                                         "FAIL",
                                         ACCESS_DENIED_MS,
                                         STATE_IDLE);
            }
        }
    } else if (app->validation_mode == VALIDATION_ADMIN_PIN) {
        if (app_matches_pin(app->entry_buffer, app->storage.admin_pin, ADMIN_PIN_LENGTH)) {
            app_begin_transient(app,
                                     STATE_ACCESS_GRANTED,
                                     "AUTH",
                                     ADMIN_NOTICE_MS,
                                     STATE_ADMIN_MENU);
        } else {
            app_begin_transient(app,
                                     STATE_ACCESS_DENIED,
                                     "FAIL",
                                     ADMIN_NOTICE_MS,
                                     STATE_IDLE);
        }
    }

    app_reset_entry(app);
    app->validation_mode = VALIDATION_NONE;
}

static void app_handle_idle_key(App *app, char key)
{
    if ((key >= '0') && (key <= '9')) {
        app_reset_entry(app);
        app_set_state(app, STATE_ENTER_PIN);
        (void)app_append_digit(app, key, USER_PIN_LENGTH);
    } else if (key == 'A') {
        app_reset_entry(app);
        app_set_state(app, STATE_ADMIN_AUTH);
    }
}

static void app_handle_enter_pin_key(App *app, char key)
{
    if ((key >= '0') && (key <= '9')) {
        (void)app_append_digit(app, key, USER_PIN_LENGTH);
    } else if (key == '*') {
        if (app->entry_length == 0U) {
            app_enter_idle(app);
        } else {
            app_backspace(app);
        }
    } else if ((key == '#') && (app->entry_length == USER_PIN_LENGTH)) {
        app->validation_mode = VALIDATION_USER_PIN;
        app_set_state(app, STATE_VALIDATE_PIN);
    }
}

static void app_handle_admin_auth_key(App *app, char key)
{
    if ((key >= '0') && (key <= '9')) {
        (void)app_append_digit(app, key, ADMIN_PIN_LENGTH);
    } else if (key == '*') {
        if (app->entry_length == 0U) {
            app_enter_idle(app);
        } else {
            app_backspace(app);
        }
    } else if ((key == '#') && (app->entry_length == ADMIN_PIN_LENGTH)) {
        app->validation_mode = VALIDATION_ADMIN_PIN;
        app_set_state(app, STATE_VALIDATE_PIN);
    }
}

static void app_handle_admin_menu_key(App *app, char key)
{
    if (key == 'B') {
        app_cycle_menu(app, 1);
    } else if (key == 'C') {
        app_cycle_menu(app, -1);
    } else if (key == '*') {
        app_enter_idle(app);
    } else if (key == '#') {
        switch (app->menu_item) {
        case MENU_CHANGE_PIN:
            app_reset_entry(app);
            app_set_state(app, STATE_CHANGE_PIN);
            break;
        case MENU_SET_LOCKOUT:
            app_reset_entry(app);
            app_set_state(app, STATE_SET_LOCKOUT);
            break;
        case MENU_TOGGLE_SOUND:
            app->storage.sound_enabled = !app->storage.sound_enabled;
            app_begin_transient(app,
                                     STATE_ACCESS_GRANTED,
                                     app->storage.sound_enabled ? " ON " : "OFF ",
                                     ADMIN_NOTICE_MS,
                                     STATE_ADMIN_MENU);
            break;
        case MENU_SHOW_FAILS:
            app_enter_diagnostics(app, DIAG_COUNTER);
            break;
        case MENU_DIAGNOSTICS:
            app_enter_diagnostics(app, DIAG_STATUS);
            break;
        default:
            break;
        }
    }
}

static void app_handle_change_pin_key(App *app, char key)
{
    if ((key >= '0') && (key <= '9')) {
        (void)app_append_digit(app, key, USER_PIN_LENGTH);
    } else if (key == '*') {
        if (app->entry_length == 0U) {
            app_enter_admin_menu(app);
        } else {
            app_backspace(app);
        }
    } else if ((key == '#') && (app->entry_length == USER_PIN_LENGTH)) {
        if (storage_set_user_pin(&app->storage, app->entry_buffer)) {
            app_begin_transient(app,
                                     STATE_ACCESS_GRANTED,
                                     "SAVE",
                                     ADMIN_NOTICE_MS,
                                     STATE_ADMIN_MENU);
        } else {
            app_begin_transient(app,
                                     STATE_ACCESS_DENIED,
                                     "ERR ",
                                     ADMIN_NOTICE_MS,
                                     STATE_CHANGE_PIN);
        }
        app_reset_entry(app);
    }
}

static void app_handle_set_lockout_key(App *app, char key)
{
    if ((key >= '0') && (key <= '9')) {
        (void)app_append_digit(app, key, DISPLAY_WIDTH);
    } else if (key == '*') {
        if (app->entry_length == 0U) {
            app_enter_admin_menu(app);
        } else {
            app_backspace(app);
        }
    } else if ((key == '#') && (app->entry_length > 0U)) {
        if (storage_set_lockout_seconds(&app->storage, app_parse_entry_number(app))) {
            app_begin_transient(app,
                                     STATE_ACCESS_GRANTED,
                                     "TIME",
                                     ADMIN_NOTICE_MS,
                                     STATE_ADMIN_MENU);
        } else {
            app_begin_transient(app,
                                     STATE_ACCESS_DENIED,
                                     "ERR ",
                                     ADMIN_NOTICE_MS,
                                     STATE_SET_LOCKOUT);
        }
        app_reset_entry(app);
    }
}

static void app_handle_diagnostics_key(App *app, char key)
{
    if (key == '*') {
        app_enter_admin_menu(app);
    } else if ((key == 'B') || (key == 'C') || (key == '#')) {
        app->diagnostics_mode = (app->diagnostics_mode == DIAG_COUNTER)
                                    ? DIAG_STATUS
                                    : DIAG_COUNTER;
    }
}

static void app_handle_key(App *app, char key)
{
    switch (app->state) {
    case STATE_IDLE:
        app_handle_idle_key(app, key);
        break;
    case STATE_ENTER_PIN:
        app_handle_enter_pin_key(app, key);
        break;
    case STATE_ADMIN_AUTH:
        app_handle_admin_auth_key(app, key);
        break;
    case STATE_ADMIN_MENU:
        app_handle_admin_menu_key(app, key);
        break;
    case STATE_CHANGE_PIN:
        app_handle_change_pin_key(app, key);
        break;
    case STATE_SET_LOCKOUT:
        app_handle_set_lockout_key(app, key);
        break;
    case STATE_DIAGNOSTICS:
        app_handle_diagnostics_key(app, key);
        break;
    case STATE_VALIDATE_PIN:
    case STATE_ACCESS_GRANTED:
    case STATE_ACCESS_DENIED:
    case STATE_LOCKOUT:
        break;
    default:
        break;
    }
}

static void app_render_menu(App *app)
{
    switch (app->menu_item) {
    case MENU_CHANGE_PIN:
        display_show_text(&app->display, "PINC");
        break;
    case MENU_SET_LOCKOUT:
        display_show_text(&app->display, "TIME");
        break;
    case MENU_TOGGLE_SOUND:
        display_show_text(&app->display, "BEEP");
        break;
    case MENU_SHOW_FAILS:
        display_show_text(&app->display, "FAIL");
        break;
    case MENU_DIAGNOSTICS:
        display_show_text(&app->display, "DIAG");
        break;
    default:
        display_show_text(&app->display, "MENU");
        break;
    }
}

static void app_render_diagnostics(App *app)
{
    char buffer[DISPLAY_WIDTH + 1U];

    if (app->diagnostics_mode == DIAG_COUNTER) {
        buffer[0] = 'F';
        buffer[1] = (char)('0' + ((app->storage.failed_attempts / 100U) % 10U));
        buffer[2] = (char)('0' + ((app->storage.failed_attempts / 10U) % 10U));
        buffer[3] = (char)('0' + (app->storage.failed_attempts % 10U));
        buffer[4] = '\0';
        display_show_text(&app->display, buffer);
    } else {
        uint32_t seconds = app->storage.lockout_duration_ms / 1000U;
        if (seconds > 999U) {
            seconds = 999U;
        }

        buffer[0] = app->storage.sound_enabled ? 'O' : 'M';
        buffer[1] = (char)('0' + ((seconds / 100U) % 10U));
        buffer[2] = (char)('0' + ((seconds / 10U) % 10U));
        buffer[3] = (char)('0' + (seconds % 10U));
        buffer[4] = '\0';
        display_show_text(&app->display, buffer);
    }
}

static void app_render(App *app)
{
    LedPattern led = LED_OFF;
    BuzzerPattern buzzer = BUZZER_SILENT;

    switch (app->state) {
    case STATE_IDLE:
        display_show_text(&app->display, "SAFE");
        led = LED_ARMED;
        break;
    case STATE_ENTER_PIN:
        if (app->entry_length == 0U) {
            display_show_text(&app->display, "PIN ");
        } else {
            display_show_progress(&app->display, app->entry_length, USER_PIN_LENGTH);
        }
        led = LED_ARMED;
        break;
    case STATE_VALIDATE_PIN:
        display_show_text(&app->display, "WAIT");
        led = LED_ARMED;
        break;
    case STATE_ACCESS_GRANTED:
        display_show_text(&app->display, app->transient_text);
        led = LED_SUCCESS;
        buzzer = BUZZER_SUCCESS;
        break;
    case STATE_ACCESS_DENIED:
        display_show_text(&app->display, app->transient_text);
        led = LED_ERROR;
        buzzer = BUZZER_ERROR;
        break;
    case STATE_LOCKOUT: {
        uint32_t seconds_remaining = (app->lockout_remaining_ms + 999U) / 1000U;

        display_show_number(&app->display, seconds_remaining, true);
        led = LED_LOCKOUT;
        buzzer = BUZZER_LOCKOUT;
        break;
    }
    case STATE_ADMIN_AUTH:
        if (app->entry_length == 0U) {
            display_show_text(&app->display, "AUTH");
        } else {
            display_show_progress(&app->display, app->entry_length, ADMIN_PIN_LENGTH);
        }
        led = LED_ADMIN;
        break;
    case STATE_ADMIN_MENU:
        app_render_menu(app);
        led = LED_ADMIN;
        break;
    case STATE_CHANGE_PIN:
        if (app->entry_length == 0U) {
            display_show_text(&app->display, "NEW ");
        } else {
            display_show_progress(&app->display, app->entry_length, USER_PIN_LENGTH);
        }
        led = LED_ADMIN;
        break;
    case STATE_SET_LOCKOUT:
        if (app->entry_length == 0U) {
            display_show_text(&app->display, "SECS");
        } else {
            display_show_number(&app->display, app_parse_entry_number(app), false);
        }
        led = LED_ADMIN;
        break;
    case STATE_DIAGNOSTICS:
        app_render_diagnostics(app);
        led = LED_ADMIN;
        break;
    default:
        display_show_text(&app->display, "SAFE");
        led = LED_ARMED;
        break;
    }

    if ((buzzer == BUZZER_SILENT) &&
        (app->storage.sound_enabled) &&
        (app->keypress_feedback_ms > 0U)) {
        buzzer = BUZZER_KEYPRESS;
    }

    indicators_set(&app->indicators, led, buzzer, app->storage.sound_enabled);
}

void app_init(App *app)
{
    if (app == NULL) {
        return;
    }

    memset(app, 0, sizeof(*app));
    storage_init_defaults(&app->storage);
    keypad_init(&app->keypad);
    display_init(&app->display);
    indicators_init(&app->indicators);
    app->menu_item = MENU_CHANGE_PIN;
    app_enter_idle(app);
    app_render(app);
}

bool app_inject_key(App *app, char key)
{
    if ((app == NULL) || !keypad_push(&app->keypad, key)) {
        return false;
    }

    app->keypress_feedback_ms = KEYPRESS_FEEDBACK_MS;
    return true;
}

void app_tick(App *app, uint32_t elapsed_ms)
{
    char key;

    if (app == NULL) {
        return;
    }

    app->state_elapsed_ms += elapsed_ms;

    if (elapsed_ms >= app->keypress_feedback_ms) {
        app->keypress_feedback_ms = 0U;
    } else {
        app->keypress_feedback_ms -= elapsed_ms;
    }

    if (app->state == STATE_LOCKOUT) {
        if (elapsed_ms >= app->lockout_remaining_ms) {
            app->lockout_remaining_ms = 0U;
        } else {
            app->lockout_remaining_ms -= elapsed_ms;
        }

        if (app->lockout_remaining_ms == 0U) {
            storage_reset_failed_attempts(&app->storage);
            app_enter_idle(app);
        }
    }

    while (keypad_pop(&app->keypad, &key)) {
        if (app->state == STATE_LOCKOUT) {
            continue;
        }

        app_handle_key(app, key);
    }

    if (app->state == STATE_VALIDATE_PIN) {
        app_handle_validation(app);
    }

    if (((app->state == STATE_ACCESS_GRANTED) || (app->state == STATE_ACCESS_DENIED)) &&
        (app->state_timeout_ms > 0U) &&
        (app->state_elapsed_ms >= app->state_timeout_ms)) {
        if (app->next_state == STATE_LOCKOUT) {
            app_begin_lockout(app);
        } else if (app->next_state == STATE_ADMIN_MENU) {
            app_enter_admin_menu(app);
        } else if (app->next_state == STATE_CHANGE_PIN) {
            app_reset_entry(app);
            app_set_state(app, STATE_CHANGE_PIN);
        } else if (app->next_state == STATE_SET_LOCKOUT) {
            app_reset_entry(app);
            app_set_state(app, STATE_SET_LOCKOUT);
        } else {
            app_enter_idle(app);
        }
    }

    app_render(app);
}

const char *app_state_name(State state)
{
    switch (state) {
    case STATE_IDLE:
        return "STATE_IDLE";
    case STATE_ENTER_PIN:
        return "STATE_ENTER_PIN";
    case STATE_VALIDATE_PIN:
        return "STATE_VALIDATE_PIN";
    case STATE_ACCESS_GRANTED:
        return "STATE_ACCESS_GRANTED";
    case STATE_ACCESS_DENIED:
        return "STATE_ACCESS_DENIED";
    case STATE_LOCKOUT:
        return "STATE_LOCKOUT";
    case STATE_ADMIN_AUTH:
        return "STATE_ADMIN_AUTH";
    case STATE_ADMIN_MENU:
        return "STATE_ADMIN_MENU";
    case STATE_CHANGE_PIN:
        return "STATE_CHANGE_PIN";
    case STATE_SET_LOCKOUT:
        return "STATE_SET_LOCKOUT";
    case STATE_DIAGNOSTICS:
        return "STATE_DIAGNOSTICS";
    default:
        return "STATE_UNKNOWN";
    }
}
