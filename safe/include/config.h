#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define DISPLAY_WIDTH 4U
#define USER_PIN_LENGTH 4U
#define ADMIN_PIN_LENGTH 6U
#define MAX_FAILED_ATTEMPTS 3U
#define DEFAULT_LOCKOUT_SECONDS 30U
#define MIN_LOCKOUT_SECONDS 5U
#define MAX_LOCKOUT_SECONDS 9999U

#define KEYPAD_QUEUE_CAPACITY 16U

#define ACCESS_GRANTED_MS 1500U
#define ACCESS_DENIED_MS 1200U
#define ADMIN_NOTICE_MS 900U
#define KEYPRESS_FEEDBACK_MS 60U
#define MAIN_LOOP_TICK_MS 50U
#define DISPLAY_REFRESH_MS 2U
#define INPUT_SCAN_MS 1U
#define KEYPAD_DEBOUNCE_MS 30U
#define BUTTON_DEBOUNCE_MS 20U

/*
 * Some Multi-Function Shield variants wire the active buzzer as active HIGH
 * instead of active LOW. This project defaults to the observed active level
 * for the current prototype.
 */
#define BUZZER_ACTIVE_LOW 1U

/*
 * Full board bring-up uses the physical shield and keypad instead of the
 * scripted host-side demo.
 */
#define ENABLE_DEMO_SCRIPT 0

#endif
