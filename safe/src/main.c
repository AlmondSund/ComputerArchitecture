#include "app_fsm.h"
#include "hw.h"

#include <stdint.h>

int main(void)
{
    App app;
    uint32_t last_app_ms;
    uint32_t last_scan_ms;
    uint32_t last_refresh_ms;

    hw_init();
    app_init(&app);

    last_app_ms = hw_now_ms();
    last_scan_ms = last_app_ms;
    last_refresh_ms = last_app_ms;

    for (;;) {
        const uint32_t now_ms = hw_now_ms();

        while ((uint32_t)(now_ms - last_scan_ms) >= INPUT_SCAN_MS) {
            last_scan_ms += INPUT_SCAN_MS;
            hw_poll_inputs(&app, last_scan_ms);
        }

        while ((uint32_t)(now_ms - last_app_ms) >= MAIN_LOOP_TICK_MS) {
            last_app_ms += MAIN_LOOP_TICK_MS;
            app_tick(&app, MAIN_LOOP_TICK_MS);
        }

        while ((uint32_t)(now_ms - last_refresh_ms) >= DISPLAY_REFRESH_MS) {
            last_refresh_ms += DISPLAY_REFRESH_MS;
            hw_apply_outputs(&app, last_refresh_ms);
        }

        hw_wait();
    }
}
