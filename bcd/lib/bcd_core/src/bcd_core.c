#include "bcd_core.h"

static void bcd_core_advance_once(bcd_core_t *core)
{
    ++core->segment_index;
    if (core->segment_index < BCD_SEGMENT_COUNT) {
        return;
    }

    core->segment_index = 0U;
    core->digit_index = (uint8_t)((core->digit_index + 1U) % BCD_DIGIT_COUNT);
}

void bcd_core_init(bcd_core_t *core, uint32_t now_ms)
{
    if (core == 0) {
        return;
    }

    core->digit_index = 0U;
    core->segment_index = 0U;
    core->phase_ms = now_ms;
}

void bcd_core_tick(bcd_core_t *core, uint32_t now_ms)
{
    uint32_t elapsed;

    if (core == 0) {
        return;
    }

    elapsed = (uint32_t)(now_ms - core->phase_ms);
    while (elapsed >= BCD_STEP_MS) {
        core->phase_ms += BCD_STEP_MS;
        elapsed -= BCD_STEP_MS;
        bcd_core_advance_once(core);
    }
}

void bcd_core_frame(const bcd_core_t *core, bcd_frame_t *frame)
{
    uint32_t i;

    if ((core == 0) || (frame == 0)) {
        return;
    }

    for (i = 0U; i < BCD_DIGIT_COUNT; ++i) {
        frame->digits[i] = 0U;
    }

    if ((core->digit_index >= BCD_DIGIT_COUNT)
            || (core->segment_index >= BCD_SEGMENT_COUNT)) {
        return;
    }

    frame->digits[core->digit_index] = (uint8_t)(1U << core->segment_index);
}
