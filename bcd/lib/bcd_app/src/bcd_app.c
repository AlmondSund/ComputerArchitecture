#include <stdint.h>

#include "bcd_app.h"
#include "bcd_core.h"
#include "bcd_hw.h"

void bcd_app_run(void)
{
    bcd_core_t core;
    bcd_frame_t frame;
    uint32_t last_scan_ms;
    uint8_t scan_digit;

    bcd_hw_init();
    bcd_core_init(&core, bcd_hw_now_ms());

    last_scan_ms = 0xFFFFFFFFU;
    scan_digit = 0U;

    while (1) {
        const uint32_t now_ms = bcd_hw_now_ms();

        bcd_core_tick(&core, now_ms);
        bcd_core_frame(&core, &frame);

        if (now_ms != last_scan_ms) {
            bcd_hw_show_digit(scan_digit, frame.digits[scan_digit]);
            scan_digit = (uint8_t)((scan_digit + 1U) % BCD_DIGIT_COUNT);
            last_scan_ms = now_ms;
        }

        bcd_hw_wait();
    }
}
