#include <assert.h>
#include <stdint.h>

#include "bcd_core.h"

static void test_initial_frame_lights_segment_a_on_first_digit(void)
{
    bcd_core_t core;
    bcd_frame_t frame;

    bcd_core_init(&core, 0U);
    bcd_core_frame(&core, &frame);

    assert(frame.digits[0] == BCD_SEG_A);
    assert(frame.digits[1] == 0U);
    assert(frame.digits[2] == 0U);
    assert(frame.digits[3] == 0U);
}

static void test_segments_advance_with_fixed_step_time(void)
{
    bcd_core_t core;
    bcd_frame_t frame;

    bcd_core_init(&core, 10U);

    bcd_core_tick(&core, 10U + BCD_STEP_MS - 1U);
    bcd_core_frame(&core, &frame);
    assert(frame.digits[0] == BCD_SEG_A);

    bcd_core_tick(&core, 10U + BCD_STEP_MS);
    bcd_core_frame(&core, &frame);
    assert(frame.digits[0] == BCD_SEG_B);

    bcd_core_tick(&core, 10U + (2U * BCD_STEP_MS));
    bcd_core_frame(&core, &frame);
    assert(frame.digits[0] == BCD_SEG_C);
}

static void test_digit_rollover_after_all_segments(void)
{
    bcd_core_t core;
    bcd_frame_t frame;

    bcd_core_init(&core, 0U);
    bcd_core_tick(&core, BCD_SEGMENT_COUNT * BCD_STEP_MS);
    bcd_core_frame(&core, &frame);

    assert(frame.digits[0] == 0U);
    assert(frame.digits[1] == BCD_SEG_A);
    assert(frame.digits[2] == 0U);
    assert(frame.digits[3] == 0U);
}

static void test_large_time_jump_advances_multiple_positions(void)
{
    bcd_core_t core;
    bcd_frame_t frame;

    bcd_core_init(&core, 0U);
    bcd_core_tick(&core, 10U * BCD_STEP_MS);
    bcd_core_frame(&core, &frame);

    assert(frame.digits[0] == 0U);
    assert(frame.digits[1] == BCD_SEG_C);
    assert(frame.digits[2] == 0U);
    assert(frame.digits[3] == 0U);
}

int main(void)
{
    test_initial_frame_lights_segment_a_on_first_digit();
    test_segments_advance_with_fixed_step_time();
    test_digit_rollover_after_all_segments();
    test_large_time_jump_advances_multiple_positions();
    return 0;
}
