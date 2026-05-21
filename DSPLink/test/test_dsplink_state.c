#include "dsplink_state.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

static void test_initial_state(void)
{
    dsplink_state_t state;

    dsplink_state_init(&state);

    assert(state.preset == DSPLINK_PRESET_FLAT);
    assert(!state.bypass_enabled);
    assert(strcmp(state.display, " P0 ") == 0);
}

static void test_preset_wraps(void)
{
    dsplink_state_t state;

    dsplink_state_init(&state);

    assert(dsplink_state_apply_event(&state, DSPLINK_UI_EVENT_PREVIOUS));
    assert(state.preset == DSPLINK_PRESET_FILTER);
    assert(strcmp(state.display, " P4 ") == 0);

    assert(dsplink_state_apply_event(&state, DSPLINK_UI_EVENT_NEXT));
    assert(state.preset == DSPLINK_PRESET_FLAT);
    assert(strcmp(state.display, " P0 ") == 0);
}

static void test_bypass_toggle(void)
{
    dsplink_state_t state;

    dsplink_state_init(&state);

    assert(dsplink_state_apply_event(&state, DSPLINK_UI_EVENT_TOGGLE_BYPASS));
    assert(state.bypass_enabled);
    assert(strcmp(state.display, " BP ") == 0);

    assert(dsplink_state_apply_event(&state, DSPLINK_UI_EVENT_NEXT));
    assert(!state.bypass_enabled);
    assert(state.preset == DSPLINK_PRESET_BASS);
    assert(strcmp(state.display, " P1 ") == 0);
}

static void test_parameter_mapping(void)
{
    dsplink_state_t state;

    dsplink_state_init(&state);

    assert(dsplink_state_set_parameter_raw(&state, 4095U));
    assert(state.parameter_percent == 100U);

    assert(dsplink_state_set_parameter_raw(&state, 2048U));
    assert((state.parameter_percent == 50U) || (state.parameter_percent == 51U));
}

static void test_parameter_hysteresis_rejects_adc_chatter(void)
{
    dsplink_state_t state;

    dsplink_state_init(&state);

    assert(dsplink_state_set_parameter_raw(&state, 3200U));
    assert(state.parameter_percent == 78U);

    assert(!dsplink_state_set_parameter_raw(&state, 3270U));
    assert(state.parameter_percent == 78U);

    assert(dsplink_state_set_parameter_raw(&state, 3400U));
    assert(state.parameter_percent == 83U);
}

int main(void)
{
    test_initial_state();
    test_preset_wraps();
    test_bypass_toggle();
    test_parameter_mapping();
    test_parameter_hysteresis_rejects_adc_chatter();
    return 0;
}
