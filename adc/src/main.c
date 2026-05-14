#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "debug_uart.h"
#include "mfs_display.h"
#include "stm32l476xx.h"
#include "temp_sensor.h"
#include "timebase.h"

enum {
    REFRESH_MS = 2U,
    DISPLAY_UPDATE_MS = 250U,
    SERIAL_UPDATE_MS = 1000U,
    USART_BAUD = 115200U,
};

static void format_temperature_text(
        const temp_sensor_snapshot_t *snapshot,
        char out[MFS_DISPLAY_DIGITS])
{
    int32_t rounded_c;
    uint32_t magnitude;

    if ((snapshot == NULL) || !snapshot->valid) {
        out[0] = ((snapshot != NULL) && snapshot->dma_fault) ? 'D' : 'A';
        out[1] = 'E';
        out[2] = 'R';
        out[3] = 'R';
        return;
    }

    rounded_c = (snapshot->temp_centi_c >= 0)
                        ? ((snapshot->temp_centi_c + 50) / 100)
                        : ((snapshot->temp_centi_c - 50) / 100);

    if (rounded_c < -9) {
        rounded_c = -9;
    } else if (rounded_c > 99) {
        rounded_c = 99;
    }

    if (rounded_c < 0) {
        magnitude = (uint32_t)(-rounded_c);
        out[0] = '-';
        out[1] = (char)('0' + magnitude);
    } else {
        magnitude = (uint32_t)rounded_c;
        out[0] = (magnitude >= 10U) ? (char)('0' + (magnitude / 10U)) : ' ';
        out[1] = (char)('0' + (magnitude % 10U));
    }

    out[2] = 'C';
    out[3] = ' ';
}

static void report_snapshot(const temp_sensor_snapshot_t *snapshot)
{
    debug_uart_write("adc-temp ");

    if ((snapshot == NULL) || !snapshot->valid) {
        debug_uart_write("status=ERR adc_fault=");
        debug_uart_write_u32((snapshot != NULL && snapshot->adc_fault) ? 1U : 0U);
        debug_uart_write(" dma_fault=");
        debug_uart_write_u32((snapshot != NULL && snapshot->dma_fault) ? 1U : 0U);
        debug_uart_write("\n");
        return;
    }

    debug_uart_write("status=OK raw_temp=");
    debug_uart_write_u32(snapshot->temp_raw);
    debug_uart_write(" raw_vref=");
    debug_uart_write_u32(snapshot->vref_raw);
    debug_uart_write(" vdda_mv=");
    debug_uart_write_u32(snapshot->vdda_mv);
    debug_uart_write(" temp_c=");
    debug_uart_write_i32(snapshot->temp_centi_c / 100);
    debug_uart_putc('.');
    debug_uart_write_u32((uint32_t)(snapshot->temp_centi_c < 0
                                            ? -(snapshot->temp_centi_c % 100)
                                            : (snapshot->temp_centi_c % 100)));
    debug_uart_write("\n");
}

static void boot_board(void)
{
    SystemCoreClockUpdate();
    mfs_display_init();
    timebase_init();
    debug_uart_init(USART_BAUD);

    if (!temp_sensor_init()) {
        debug_uart_write("adc-temp boot: ADC bring-up failed; display will show AERR\n");
        return;
    }

    debug_uart_write("adc-temp boot: ADC1 channel 17 + VREFINT, DMA1 channel 1 circular\n");
}

int main(void)
{
    uint8_t scan_digit = 0U;
    uint32_t last_refresh_ms = 0U;
    uint32_t last_display_update_ms = 0U;
    uint32_t last_serial_update_ms = 0U;
    char display_text[MFS_DISPLAY_DIGITS] = {'A', 'D', 'C', ' '};
    temp_sensor_snapshot_t snapshot = {0U, 0U, 0U, 0, false, false, false};

    boot_board();

    while (true) {
        const uint32_t current_ms = timebase_now_ms();

        if ((uint32_t)(current_ms - last_display_update_ms) >= DISPLAY_UPDATE_MS) {
            last_display_update_ms = current_ms;
            if (temp_sensor_read(&snapshot)
                    || snapshot.adc_fault
                    || snapshot.dma_fault) {
                format_temperature_text(&snapshot, display_text);
            }
        }

        if ((uint32_t)(current_ms - last_refresh_ms) >= REFRESH_MS) {
            last_refresh_ms = current_ms;
            mfs_display_show_digit(scan_digit, display_text[scan_digit]);
            scan_digit = (uint8_t)((scan_digit + 1U) % MFS_DISPLAY_DIGITS);
        }

        if ((uint32_t)(current_ms - last_serial_update_ms) >= SERIAL_UPDATE_MS) {
            last_serial_update_ms = current_ms;
            (void)temp_sensor_read(&snapshot);
            report_snapshot(&snapshot);
        }

        __WFI();
    }
}
