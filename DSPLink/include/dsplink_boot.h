#ifndef DSPLINK_BOOT_H
#define DSPLINK_BOOT_H

#include <stdbool.h>
#include <stdint.h>

#include "dsplink_bus.h"

typedef struct {
    bool found;
    bool readback_ok;
    bool dac_config_ok;
    bool core_control_ok;
    bool ready;
    uint8_t address;
    uint8_t device_count;
    uint16_t core_before;
    uint16_t core_after;
    uint16_t dac_before;
    uint16_t dac_after;
} dsplink_boot_result_t;

void dsplink_boot_result_clear(dsplink_boot_result_t *result);
dsplink_boot_result_t dsplink_boot_tsa1701(void);

#endif
