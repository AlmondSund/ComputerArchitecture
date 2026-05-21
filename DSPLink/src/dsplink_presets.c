#include "dsplink_presets.h"

#include <stdbool.h>

#include "dsplink_state.h"

const char *dsplink_preset_description(dsplink_preset_t preset)
{
    switch (preset) {
    case DSPLINK_PRESET_FLAT:
        return "flat control preset";
    case DSPLINK_PRESET_BASS:
        return "bass preset pending SigmaStudio data";
    case DSPLINK_PRESET_VOICE:
        return "voice preset pending SigmaStudio data";
    case DSPLINK_PRESET_NIGHT:
        return "night preset pending SigmaStudio data";
    case DSPLINK_PRESET_FILTER:
        return "filter preset pending SigmaStudio data";
    case DSPLINK_PRESET_COUNT:
    default:
        return "unknown preset";
    }
}

bool dsplink_preset_has_exported_audio_data(dsplink_preset_t preset)
{
    (void)preset;
    return false;
}
