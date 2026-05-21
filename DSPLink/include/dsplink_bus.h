#ifndef DSPLINK_BUS_H
#define DSPLINK_BUS_H

#include <stdbool.h>
#include <stdint.h>

enum {
    DSPLINK_I2C_SCAN_FIRST_ADDRESS = 0x08U,
    DSPLINK_I2C_SCAN_LAST_ADDRESS = 0x77U,
    DSPLINK_ADAU1701_ADDRESS_FIRST = 0x34U,
    DSPLINK_ADAU1701_ADDRESS_LAST = 0x37U,
    DSPLINK_ADAU1701_CORE_CONTROL_REGISTER = 0x081CU,
    DSPLINK_ADAU1701_CORE_ADC_PASS = 1U << 4,
    DSPLINK_ADAU1701_CORE_DAC_PASS = 1U << 3,
    DSPLINK_ADAU1701_DAC_SETUP_REGISTER = 0x0827U,
    DSPLINK_ADAU1701_DAC_SETUP_DS_48KHZ = 0x0001U
};

typedef enum {
    DSPLINK_BUS_OK = 0,
    DSPLINK_BUS_ERROR_TIMEOUT,
    DSPLINK_BUS_ERROR_BUSY
} dsplink_bus_status_t;

typedef struct {
    bool found_any_device;
    bool found_adau1701;
    bool control_read_ok;
    uint8_t adau1701_address;
    uint8_t device_count;
    uint16_t dsp_core_control;
    uint16_t dac_setup;
} dsplink_bus_scan_t;

void dsplink_bus_i2c1_init(void);
dsplink_bus_status_t dsplink_bus_i2c1_probe(uint8_t address_7bit);
dsplink_bus_scan_t dsplink_bus_i2c1_scan(void);
dsplink_bus_status_t dsplink_bus_adau1701_read_u16(
        uint8_t address_7bit,
        uint16_t register_address,
        uint16_t *value_out);
dsplink_bus_status_t dsplink_bus_adau1701_write_u16(
        uint8_t address_7bit,
        uint16_t register_address,
        uint16_t value);
dsplink_bus_status_t dsplink_bus_adau1701_write_block(
        uint8_t address_7bit,
        uint16_t start_address,
        const uint8_t *data,
        uint16_t length);
const char *dsplink_bus_status_name(dsplink_bus_status_t status);

#endif
