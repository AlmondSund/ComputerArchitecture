#include "indicators.h"

#include <stddef.h>

void indicators_init(Indicators *indicators)
{
    if (indicators == NULL) {
        return;
    }

    indicators->led = LED_OFF;
    indicators->buzzer = BUZZER_SILENT;
    indicators->sound_enabled = true;
}

void indicators_set(Indicators *indicators,
                    LedPattern led,
                    BuzzerPattern buzzer,
                         bool sound_enabled)
{
    if (indicators == NULL) {
        return;
    }

    indicators->led = led;
    indicators->sound_enabled = sound_enabled;
    indicators->buzzer = sound_enabled ? buzzer : BUZZER_SILENT;
}
