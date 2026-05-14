#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t temp_raw;
    uint16_t vref_raw;
    uint32_t vdda_mv;
    int32_t temp_centi_c;
    bool valid;
    bool adc_fault;
    bool dma_fault;
} temp_sensor_snapshot_t;

bool temp_sensor_init(void);
bool temp_sensor_read(temp_sensor_snapshot_t *snapshot);

#endif
