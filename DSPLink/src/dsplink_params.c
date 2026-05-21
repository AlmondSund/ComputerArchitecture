#include "dsplink_params.h"

#include <stdbool.h>
#include <stdint.h>

#include "dsplink_bus.h"

dsplink_output_control_t dsplink_params_set_output_enabled(
        uint8_t address_7bit,
        bool enabled)
{
    uint16_t current = 0U;
    uint16_t updated;
    uint16_t verified = 0U;
    dsplink_output_control_t result = {false, false, 0U};
    const uint16_t pass_mask = DSPLINK_ADAU1701_CORE_ANALOG_PASS_MASK;

    if (dsplink_bus_adau1701_read_u16(
                address_7bit,
                DSPLINK_ADAU1701_CORE_CONTROL_REGISTER,
                &current) != DSPLINK_BUS_OK) {
        return result;
    }

    updated = enabled ? (uint16_t)(current | pass_mask)
                      : (uint16_t)(current & (uint16_t)~pass_mask);

    if (dsplink_bus_adau1701_write_u16(
                address_7bit,
                DSPLINK_ADAU1701_CORE_CONTROL_REGISTER,
                updated) != DSPLINK_BUS_OK) {
        return result;
    }

    if (dsplink_bus_adau1701_read_u16(
                address_7bit,
                DSPLINK_ADAU1701_CORE_CONTROL_REGISTER,
                &verified) != DSPLINK_BUS_OK) {
        return result;
    }

    result.core_control = verified;
    result.output_enabled = (verified & pass_mask) == pass_mask;
    result.write_ok = enabled ? result.output_enabled : ((verified & pass_mask) == 0U);
    return result;
}
