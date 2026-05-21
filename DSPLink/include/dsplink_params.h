#ifndef DSPLINK_PARAMS_H
#define DSPLINK_PARAMS_H

#include <stdbool.h>
#include <stdint.h>

#include "dsplink_bus.h"

typedef struct {
    bool write_ok;
    bool output_enabled;
    uint16_t core_control;
} dsplink_output_control_t;

dsplink_output_control_t dsplink_params_set_output_enabled(
        uint8_t address_7bit,
        bool enabled);

#endif
