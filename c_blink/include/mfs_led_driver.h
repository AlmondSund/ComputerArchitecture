#ifndef MFS_LED_DRIVER_H
#define MFS_LED_DRIVER_H

#include <stdbool.h>

/**
 * @brief Logical LEDs available on the Multi Function Shield.
 */
typedef enum
{
    MFS_LED_D1 = 0,
    MFS_LED_D2 = 1,
    MFS_LED_D3 = 2,
    MFS_LED_D4 = 3,
    MFS_LED_COUNT
} mfs_led_t;

/**
 * @brief Initializes GPIO clocks and LED pins for the shield.
 *
 * Hardware mapping (NUCLEO-L476RG + Multi Function Shield):
 * - D1 -> PA5
 * - D2 -> PA6
 * - D3 -> PA7
 * - D4 -> PB6
 *
 * Shield LEDs are active-low: GPIO LOW turns LED ON.
 */
void mfs_led_driver_init(void);

/**
 * @brief Forces all shield LEDs to OFF state.
 */
void mfs_led_all_off(void);

/**
 * @brief Writes one LED state.
 *
 * @param led   Logical LED index.
 * @param is_on true = LED ON, false = LED OFF.
 */
void mfs_led_set(
    mfs_led_t led,
    bool is_on
);

/**
 * @brief Toggles one LED using the driver's encapsulated software state.
 *
 * @param led Logical LED index.
 */
void mfs_led_toggle(mfs_led_t led);

#endif /* MFS_LED_DRIVER_H */
