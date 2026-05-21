#include "dsplink_state.h"

#include <stddef.h>

static void dsplink_state_update_display(dsplink_state_t *state)
{
    if (state == NULL) {
        return;
    }

    if (state->bypass_enabled) {
        state->display[0] = ' ';
        state->display[1] = 'B';
        state->display[2] = 'P';
        state->display[3] = ' ';
    } else {
        state->display[0] = ' ';
        state->display[1] = 'P';
        state->display[2] = (char)('0' + (uint8_t)state->preset);
        state->display[3] = ' ';
    }

    state->display[DSPLINK_DISPLAY_WIDTH] = '\0';
}

static uint8_t dsplink_percent_from_raw(uint16_t raw_value)
{
    uint32_t percent = ((uint32_t)raw_value * 100U + (DSPLINK_POT_MAX / 2U))
                       / DSPLINK_POT_MAX;

    if (percent > 100U) {
        percent = 100U;
    }

    return (uint8_t)percent;
}

void dsplink_state_init(dsplink_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->preset = DSPLINK_PRESET_FLAT;
    state->bypass_enabled = false;
    state->parameter_raw = 0U;
    state->parameter_percent = 0U;
    dsplink_state_update_display(state);
}

bool dsplink_state_apply_event(dsplink_state_t *state, dsplink_ui_event_t event)
{
    bool changed = false;

    if (state == NULL) {
        return false;
    }

    if ((event & DSPLINK_UI_EVENT_PREVIOUS) != 0U) {
        state->preset = (state->preset == DSPLINK_PRESET_FLAT)
                                ? DSPLINK_PRESET_FILTER
                                : (dsplink_preset_t)((uint8_t)state->preset - 1U);
        state->bypass_enabled = false;
        changed = true;
    }

    if ((event & DSPLINK_UI_EVENT_NEXT) != 0U) {
        state->preset = (dsplink_preset_t)(((uint8_t)state->preset + 1U)
                                           % (uint8_t)DSPLINK_PRESET_COUNT);
        state->bypass_enabled = false;
        changed = true;
    }

    if ((event & DSPLINK_UI_EVENT_TOGGLE_BYPASS) != 0U) {
        state->bypass_enabled = !state->bypass_enabled;
        changed = true;
    }

    if (changed) {
        dsplink_state_update_display(state);
    }

    return changed;
}

bool dsplink_state_set_parameter_raw(dsplink_state_t *state, uint16_t raw_value)
{
    uint8_t percent;
    uint8_t delta;

    if (state == NULL) {
        return false;
    }

    if (raw_value > DSPLINK_POT_MAX) {
        raw_value = DSPLINK_POT_MAX;
    }

    percent = dsplink_percent_from_raw(raw_value);
    delta = (percent > state->parameter_percent)
                    ? (uint8_t)(percent - state->parameter_percent)
                    : (uint8_t)(state->parameter_percent - percent);

    if (delta < DSPLINK_PARAMETER_HYSTERESIS_PERCENT) {
        state->parameter_raw = raw_value;
        return false;
    }

    state->parameter_raw = raw_value;
    state->parameter_percent = percent;
    return true;
}

const char *dsplink_preset_name(dsplink_preset_t preset)
{
    switch (preset) {
    case DSPLINK_PRESET_FLAT:
        return "FLAT";
    case DSPLINK_PRESET_BASS:
        return "BASS";
    case DSPLINK_PRESET_VOICE:
        return "VOICE";
    case DSPLINK_PRESET_NIGHT:
        return "NIGHT";
    case DSPLINK_PRESET_FILTER:
        return "FILTER";
    case DSPLINK_PRESET_COUNT:
    default:
        return "UNKNOWN";
    }
}
