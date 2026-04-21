#include "app_fsm.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void inject_keys(App *app, const char *keys)
{
    size_t index;

    for (index = 0U; keys[index] != '\0'; ++index) {
        assert(app_inject_key(app, keys[index]));
    }
}

static void tick_until(App *app, uint32_t duration_ms)
{
    uint32_t elapsed = 0U;

    while (elapsed < duration_ms) {
        app_tick(app, 50U);
        elapsed += 50U;
    }
}

static void test_valid_pin_unlocks(void)
{
    App app;

    app_init(&app);
    inject_keys(&app, "1234#");
    app_tick(&app, 50U);

    assert(app.state == STATE_ACCESS_GRANTED);
    assert(strcmp(app.display.text, "OPEN") == 0);
    assert(app.storage.failed_attempts == 0U);

    tick_until(&app, ACCESS_GRANTED_MS);
    assert(app.state == STATE_IDLE);
    assert(strcmp(app.display.text, "SAFE") == 0);
}

static void test_lockout_after_three_failures(void)
{
    App app;

    app_init(&app);
    assert(storage_set_lockout_seconds(&app.storage, 5U));

    inject_keys(&app, "0000#");
    app_tick(&app, 50U);
    assert(app.state == STATE_ACCESS_DENIED);
    tick_until(&app, ACCESS_DENIED_MS);

    inject_keys(&app, "0000#");
    app_tick(&app, 50U);
    assert(app.state == STATE_ACCESS_DENIED);
    tick_until(&app, ACCESS_DENIED_MS);

    inject_keys(&app, "0000#");
    app_tick(&app, 50U);
    assert(app.state == STATE_ACCESS_DENIED);
    tick_until(&app, ACCESS_DENIED_MS);

    assert(app.state == STATE_LOCKOUT);
    assert(app.lockout_remaining_ms > 0U);

    tick_until(&app, app.lockout_remaining_ms + 50U);
    assert(app.state == STATE_IDLE);
    assert(app.storage.failed_attempts == 0U);
}

static void test_admin_can_change_user_pin_and_toggle_sound(void)
{
    App app;

    app_init(&app);
    inject_keys(&app, "A654321#");
    app_tick(&app, 50U);
    assert(app.state == STATE_ACCESS_GRANTED);
    tick_until(&app, ADMIN_NOTICE_MS);

    assert(app.state == STATE_ADMIN_MENU);
    assert(app.menu_item == MENU_CHANGE_PIN);

    inject_keys(&app, "#2468#");
    app_tick(&app, 50U);
    assert(app.state == STATE_ACCESS_GRANTED);
    tick_until(&app, ADMIN_NOTICE_MS);

    assert(strcmp(app.storage.user_pin, "2468") == 0);
    assert(app.state == STATE_ADMIN_MENU);

    inject_keys(&app, "BB#");
    app_tick(&app, 50U);
    assert(app.state == STATE_ACCESS_GRANTED);
    tick_until(&app, ADMIN_NOTICE_MS);

    assert(app.storage.sound_enabled == false);

    inject_keys(&app, "*2468#");
    app_tick(&app, 50U);
    assert(app.state == STATE_ACCESS_GRANTED);
    assert(strcmp(app.display.text, "OPEN") == 0);
}

int main(void)
{
    test_valid_pin_unlocks();
    test_lockout_after_three_failures();
    test_admin_can_change_user_pin_and_toggle_sound();

    puts("safe logic tests passed");
    return 0;
}
