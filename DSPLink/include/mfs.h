#ifndef MFS_H
#define MFS_H

#include <stdbool.h>
#include <stdint.h>

#include "dsplink_state.h"

enum {
    MFS_LED_RUNNING = 1U << 0,
    MFS_LED_READY = 1U << 1,
    MFS_LED_PRESET = 1U << 2,
    MFS_LED_ERROR = 1U << 3
};

void mfs_init(void);
dsplink_ui_event_t mfs_poll_buttons(uint32_t now_ms);
uint16_t mfs_read_potentiometer(void);
void mfs_set_leds(uint8_t led_mask);
void mfs_display_text(const char text[DSPLINK_DISPLAY_WIDTH + 1U]);
void mfs_refresh_display(uint32_t now_ms);
void mfs_beep(uint32_t now_ms, uint32_t duration_ms);
void mfs_service_buzzer(uint32_t now_ms);

#endif
