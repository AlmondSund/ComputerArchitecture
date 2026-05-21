#include "dsplink_boot.h"

#include <stdbool.h>
#include <stdint.h>

#include "dsplink_bus.h"

void dsplink_boot_result_clear(dsplink_boot_result_t *result)
{
    if (result == 0) {
        return;
    }

    result->found = false;
    result->readback_ok = false;
    result->dac_config_ok = false;
    result->core_control_ok = false;
    result->ready = false;
    result->address = 0U;
    result->device_count = 0U;
    result->core_before = 0U;
    result->core_after = 0U;
    result->dac_before = 0U;
    result->dac_after = 0U;
}

dsplink_boot_result_t dsplink_boot_tsa1701(void)
{
    uint16_t core_after = 0U;
    uint16_t dac_after = 0U;
    const dsplink_bus_scan_t scan = dsplink_bus_i2c1_scan();
    dsplink_boot_result_t result;

    dsplink_boot_result_clear(&result);
    result.found = scan.found_adau1701;
    result.readback_ok = scan.control_read_ok;
    result.address = scan.adau1701_address;
    result.device_count = scan.device_count;
    result.core_before = scan.dsp_core_control;
    result.core_after = scan.dsp_core_control;
    result.dac_before = scan.dac_setup;
    result.dac_after = scan.dac_setup;

    if (!scan.found_adau1701 || !scan.control_read_ok) {
        return result;
    }

    {
        const uint16_t target_dac_setup =
                (uint16_t)((scan.dac_setup & 0xFFFCU)
                           | DSPLINK_ADAU1701_DAC_SETUP_DS_48KHZ);
        const dsplink_bus_status_t write_status =
                dsplink_bus_adau1701_write_u16(
                    scan.adau1701_address,
                    DSPLINK_ADAU1701_DAC_SETUP_REGISTER,
                    target_dac_setup);
        const dsplink_bus_status_t verify_status =
                dsplink_bus_adau1701_read_u16(
                    scan.adau1701_address,
                    DSPLINK_ADAU1701_DAC_SETUP_REGISTER,
                    &dac_after);

        result.dac_after = dac_after;
        result.dac_config_ok =
                (write_status == DSPLINK_BUS_OK)
                && (verify_status == DSPLINK_BUS_OK)
                && (dac_after == target_dac_setup);
    }

    {
        const uint16_t target_core =
                (uint16_t)(scan.dsp_core_control
                           | DSPLINK_ADAU1701_CORE_ADC_PASS
                           | DSPLINK_ADAU1701_CORE_DAC_PASS);
        const dsplink_bus_status_t write_status =
                dsplink_bus_adau1701_write_u16(
                    scan.adau1701_address,
                    DSPLINK_ADAU1701_CORE_CONTROL_REGISTER,
                    target_core);
        const dsplink_bus_status_t verify_status =
                dsplink_bus_adau1701_read_u16(
                    scan.adau1701_address,
                    DSPLINK_ADAU1701_CORE_CONTROL_REGISTER,
                    &core_after);

        result.core_after = core_after;
        result.core_control_ok =
                (write_status == DSPLINK_BUS_OK)
                && (verify_status == DSPLINK_BUS_OK)
                && ((core_after & DSPLINK_ADAU1701_CORE_ADC_PASS) != 0U)
                && ((core_after & DSPLINK_ADAU1701_CORE_DAC_PASS) != 0U);
    }

    result.ready = result.dac_config_ok && result.core_control_ok;
    return result;
}
