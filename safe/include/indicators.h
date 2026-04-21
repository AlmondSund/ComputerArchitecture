#ifndef INDICATORS_H
#define INDICATORS_H

#include <stdbool.h>

typedef enum {
    LED_OFF = 0,
    LED_ARMED,
    LED_SUCCESS,
    LED_ERROR,
    LED_LOCKOUT,
    LED_ADMIN
} LedPattern;

typedef enum {
    BUZZER_SILENT = 0,
    BUZZER_KEYPRESS,
    BUZZER_SUCCESS,
    BUZZER_ERROR,
    BUZZER_LOCKOUT
} BuzzerPattern;

/*
 * Logical output intent for LEDs and the buzzer. Board-specific drivers can map
 * these patterns to GPIO/PWM behavior.
 */
typedef struct {
    LedPattern led;
    BuzzerPattern buzzer;
    bool sound_enabled;
} Indicators;

void indicators_init(Indicators *indicators);
void indicators_set(Indicators *indicators,
                         LedPattern led,
                         BuzzerPattern buzzer,
                         bool sound_enabled);

#endif
