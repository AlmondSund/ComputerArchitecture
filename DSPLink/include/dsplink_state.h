#ifndef DSPLINK_STATE_H
#define DSPLINK_STATE_H

#include <stdbool.h>
#include <stdint.h>

enum {
    DSPLINK_DISPLAY_WIDTH = 4U,
    DSPLINK_POT_MAX = 4095U,
    DSPLINK_PARAMETER_HYSTERESIS_PERCENT = 3U
};

typedef enum {
    DSPLINK_PRESET_FLAT = 0,
    DSPLINK_PRESET_BASS,
    DSPLINK_PRESET_VOICE,
    DSPLINK_PRESET_NIGHT,
    DSPLINK_PRESET_FILTER,
    DSPLINK_PRESET_COUNT
} dsplink_preset_t;

typedef enum {
    DSPLINK_UI_EVENT_NONE = 0,
    DSPLINK_UI_EVENT_PREVIOUS = 1U << 0,
    DSPLINK_UI_EVENT_NEXT = 1U << 1,
    DSPLINK_UI_EVENT_TOGGLE_BYPASS = 1U << 2,
    DSPLINK_UI_EVENT_PARAMETER_CHANGED = 1U << 3
} dsplink_ui_event_t;

typedef struct {
    dsplink_preset_t preset;
    bool bypass_enabled;
    uint16_t parameter_raw;
    uint8_t parameter_percent;
    char display[DSPLINK_DISPLAY_WIDTH + 1U];
} dsplink_state_t;

void dsplink_state_init(dsplink_state_t *state);
bool dsplink_state_apply_event(dsplink_state_t *state, dsplink_ui_event_t event);
bool dsplink_state_set_parameter_raw(dsplink_state_t *state, uint16_t raw_value);
const char *dsplink_preset_name(dsplink_preset_t preset);

#endif
