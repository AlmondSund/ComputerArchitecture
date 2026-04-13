#ifndef BCD_CORE_H
#define BCD_CORE_H

#include <stdint.h>

enum {
    BCD_DIGIT_COUNT = 4U,
    BCD_SEGMENT_COUNT = 8U,
    BCD_SEG_A = 1U << 0,
    BCD_SEG_B = 1U << 1,
    BCD_SEG_C = 1U << 2,
    BCD_SEG_D = 1U << 3,
    BCD_SEG_E = 1U << 4,
    BCD_SEG_F = 1U << 5,
    BCD_SEG_G = 1U << 6,
    BCD_SEG_DP = 1U << 7,
    BCD_STEP_MS = 250U,
};

/**
 * @brief Logical 4-digit display frame.
 */
typedef struct {
    uint8_t digits[BCD_DIGIT_COUNT];
} bcd_frame_t;

/**
 * @brief Mutable state for the segment-test pattern generator.
 */
typedef struct {
    uint8_t digit_index;
    uint8_t segment_index;
    uint32_t phase_ms;
} bcd_core_t;

/**
 * @brief Initialize the self-test pattern generator.
 *
 * The initial frame lights segment A on digit 0.
 *
 * @param core Mutable core instance.
 * @param now_ms Current monotonic time in milliseconds.
 */
void bcd_core_init(bcd_core_t *core, uint32_t now_ms);

/**
 * @brief Advance the current test position based on elapsed time.
 *
 * @param core Mutable core instance.
 * @param now_ms Current monotonic time in milliseconds.
 */
void bcd_core_tick(bcd_core_t *core, uint32_t now_ms);

/**
 * @brief Materialize the current 4-digit logical frame.
 *
 * @param core Immutable core instance.
 * @param frame Output frame.
 */
void bcd_core_frame(const bcd_core_t *core, bcd_frame_t *frame);

#endif // BCD_CORE_H
