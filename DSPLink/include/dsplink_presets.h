#ifndef DSPLINK_PRESETS_H
#define DSPLINK_PRESETS_H

#include <stdbool.h>

#include "dsplink_state.h"

const char *dsplink_preset_description(dsplink_preset_t preset);
bool dsplink_preset_has_exported_audio_data(dsplink_preset_t preset);

#endif
