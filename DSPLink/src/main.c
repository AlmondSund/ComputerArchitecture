#include <stdbool.h>
#include <stdint.h>

#include "dsplink_boot.h"
#include "dsplink_bus.h"
#include "dsplink_params.h"
#include "dsplink_presets.h"
#include "dsplink_state.h"
#include "mfs.h"
#include "stm32f4xx.h"
#include "uart_debug.h"

enum {
    UART_BAUD = 115200U,
    POT_SAMPLE_MS = 80U,
    STATUS_LOG_MS = 1000U,
    PARAMETER_LOG_STEP_PERCENT = 5U,
    BOOT_BEEP_MS = 60U,
    EVENT_BEEP_MS = 25U
};

static volatile uint32_t g_ms = 0U;

void SysTick_Handler(void)
{
    g_ms++;
}

static uint32_t now_ms(void)
{
    return g_ms;
}

static void log_preset(const dsplink_state_t *state)
{
    uart_debug_write("[DSPLink] Active preset: ");
    uart_debug_write(dsplink_preset_name(state->preset));
    uart_debug_write(state->bypass_enabled ? " bypass=on desc=" : " bypass=off desc=");
    uart_debug_write(dsplink_preset_description(state->preset));
    uart_debug_write(" exported_audio=");
    uart_debug_write(dsplink_preset_has_exported_audio_data(state->preset) ? "yes\n" : "no\n");
}

static void log_parameter(const dsplink_state_t *state)
{
    uart_debug_write("[DSPLink] Parameter input: ");
    uart_debug_write_u32(state->parameter_percent);
    uart_debug_write("% raw=");
    uart_debug_write_u32(state->parameter_raw);
    uart_debug_write("\n");
}

static void uart_write_hex_nibble(uint8_t value)
{
    value &= 0x0FU;
    if (value < 10U) {
        uart_debug_putc((char)('0' + value));
    } else {
        uart_debug_putc((char)('A' + value - 10U));
    }
}

static void uart_write_hex_u8(uint8_t value)
{
    uart_debug_write("0x");
    uart_write_hex_nibble((uint8_t)(value >> 4U));
    uart_write_hex_nibble(value);
}

static void uart_write_hex_u16(uint16_t value)
{
    uart_debug_write("0x");
    uart_write_hex_nibble((uint8_t)(value >> 12U));
    uart_write_hex_nibble((uint8_t)(value >> 8U));
    uart_write_hex_nibble((uint8_t)(value >> 4U));
    uart_write_hex_nibble((uint8_t)value);
}

static void log_dsp_boot(const dsplink_boot_result_t *boot)
{
    uart_debug_write("[DSPLink] I2C1 scan devices=");
    uart_debug_write_u32(boot->device_count);
    uart_debug_write("\n");

    if (!boot->found) {
        uart_debug_write("[DSPLink] TSA1701/ADAU1701 not detected on I2C1 PB8/PB9\n");
        return;
    }

    uart_debug_write("[DSPLink] TSA1701/ADAU1701 detected at 7-bit address ");
    uart_write_hex_u8(boot->address);
    uart_debug_write(" (datasheet write byte ");
    uart_write_hex_u8((uint8_t)(boot->address << 1U));
    uart_debug_write(")\n");

    if (!boot->readback_ok) {
        uart_debug_write("[DSPLink] TSA1701 register readback failed\n");
        return;
    }

    uart_debug_write("[DSPLink] TSA1701 boot core_before=");
    uart_write_hex_u16(boot->core_before);
    uart_debug_write(" core_after=");
    uart_write_hex_u16(boot->core_after);
    uart_debug_write(" dac_before=");
    uart_write_hex_u16(boot->dac_before);
    uart_debug_write(" dac_after=");
    uart_write_hex_u16(boot->dac_after);
    uart_debug_write(boot->dac_config_ok ? " dac_cfg=ok" : " dac_cfg=fail");
    uart_debug_write(boot->core_control_ok ? " core_cfg=ok" : " core_cfg=fail");
    uart_debug_write(boot->ready ? " ready=yes\n" : " ready=no\n");
}

static bool apply_dsp_output_gate(dsplink_boot_result_t *boot, bool output_enabled)
{
    const dsplink_output_control_t output =
            dsplink_params_set_output_enabled(boot->address, output_enabled);

    boot->core_after = output.core_control;

    uart_debug_write("[DSPLink] Output gate ");
    uart_debug_write(output_enabled ? "enabled" : "disabled");
    uart_debug_write(" core=");
    uart_write_hex_u16(output.core_control);
    uart_debug_write(" status=");
    uart_debug_write(output.write_ok ? "OK\n" : "FAIL\n");

    return output.write_ok;
}

static void update_status_leds(const dsplink_state_t *state, bool dsp_ready)
{
    uint8_t mask = MFS_LED_RUNNING;

    if (dsp_ready) {
        mask |= MFS_LED_READY;
    }

    if (!state->bypass_enabled) {
        mask |= MFS_LED_PRESET;
    }

    if (!dsp_ready) {
        mask |= MFS_LED_ERROR;
    }

    mfs_set_leds(mask);
}

static dsplink_boot_result_t boot_board(dsplink_state_t *state)
{
    dsplink_boot_result_t dsp_boot;

    SystemCoreClockUpdate();
    (void)SysTick_Config(SystemCoreClock / 1000U);
    NVIC_SetPriority(SysTick_IRQn, 3U);

    uart_debug_init(UART_BAUD);
    uart_debug_write("[DSPLink] Boot\n");
    uart_debug_write("[DSPLink] Clock initialized\n");

    mfs_init();
    uart_debug_write("[DSPLink] MFS initialized\n");

    dsplink_state_init(state);
    mfs_display_text(state->display);
    mfs_beep(now_ms(), BOOT_BEEP_MS);

    uart_debug_write("[DSPLink] Controller ready: MFS + NUCLEO-F429ZI\n");
    uart_debug_write("[DSPLink] Initializing I2C1 on PB8/PB9\n");
    dsplink_bus_i2c1_init();
    dsp_boot = dsplink_boot_tsa1701();
    log_dsp_boot(&dsp_boot);

    if (dsp_boot.ready) {
        mfs_display_text(state->display);
    } else {
        mfs_display_text(" Er ");
    }

    update_status_leds(state, dsp_boot.ready);
    log_preset(state);
    return dsp_boot;
}

int main(void)
{
    dsplink_state_t state;
    dsplink_boot_result_t dsp_boot;
    bool dsp_runtime_ok;
    uint32_t last_pot_sample_ms = 0U;
    uint32_t last_status_log_ms = 0U;
    uint8_t last_logged_percent_bucket = 0xFFU;

    dsp_boot = boot_board(&state);
    dsp_runtime_ok = dsp_boot.ready;

    for (;;) {
        const uint32_t current_ms = now_ms();
        const dsplink_ui_event_t events = mfs_poll_buttons(current_ms);

        if (events != DSPLINK_UI_EVENT_NONE) {
            if (dsplink_state_apply_event(&state, events)) {
                if (dsp_boot.ready) {
                    dsp_runtime_ok =
                            apply_dsp_output_gate(&dsp_boot, !state.bypass_enabled);
                }

                if (dsp_runtime_ok) {
                    mfs_display_text(state.display);
                } else {
                    mfs_display_text(" Er ");
                }
                update_status_leds(&state, dsp_runtime_ok);
                mfs_beep(current_ms, EVENT_BEEP_MS);
                log_preset(&state);
            }
        }

        if ((uint32_t)(current_ms - last_pot_sample_ms) >= POT_SAMPLE_MS) {
            const uint16_t pot = mfs_read_potentiometer();

            last_pot_sample_ms = current_ms;
            if (dsplink_state_set_parameter_raw(&state, pot)) {
                const uint8_t bucket = (uint8_t)(state.parameter_percent
                                                 / PARAMETER_LOG_STEP_PERCENT);

                if (bucket != last_logged_percent_bucket) {
                    last_logged_percent_bucket = bucket;
                    log_parameter(&state);
                }
            }
        }

        if ((uint32_t)(current_ms - last_status_log_ms) >= STATUS_LOG_MS) {
            last_status_log_ms = current_ms;
            uart_debug_write("[DSPLink] UI alive preset=");
            uart_debug_write(dsplink_preset_name(state.preset));
            uart_debug_write(state.bypass_enabled ? " bypass=on param=" : " bypass=off param=");
            uart_debug_write_u32(state.parameter_percent);
            uart_debug_write(dsp_runtime_ok ? "% dsp=ready addr=" : "% dsp=not-ready addr=");
            uart_write_hex_u8(dsp_boot.address);
            uart_debug_write(dsp_boot.readback_ok ? " read=ok core=" : " read=fail core=");
            uart_write_hex_u16(dsp_boot.core_after);
            uart_debug_write(" dac_after=");
            uart_write_hex_u16(dsp_boot.dac_after);
            uart_debug_write(dsp_boot.dac_config_ok ? " dac_cfg=ok" : " dac_cfg=fail");
            uart_debug_write(dsp_boot.core_control_ok ? " core_cfg=ok" : " core_cfg=fail");
            uart_debug_write(" exported_audio=");
            uart_debug_write(dsplink_preset_has_exported_audio_data(state.preset) ? "yes" : "no");
            uart_debug_write("\n");
        }

        mfs_refresh_display(current_ms);
        mfs_service_buzzer(current_ms);
        __WFI();
    }
}
