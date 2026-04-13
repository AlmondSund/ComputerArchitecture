# BCD Display Segment Test

This project exercises the four-digit seven-segment display on the
Multifunction Shield connected to an STM32 Nucleo L476RG.

The firmware continuously lights exactly one segment at a time. It sweeps
through segments `A` to `DP` on digit 0, then repeats the same segment walk on
digits 1, 2, and 3. This makes it easy to validate:

- segment wiring
- digit-select wiring
- shift-register ordering
- multiplex refresh on the target board

## Architecture

- `lib/bcd_core/src`: pure time-driven segment-test sequence
- `lib/bcd_app/src`: app loop that combines the core with the hardware boundary
- `include/bcd_hw.h`: hardware contract used by the app layer
- `src/hw_stm32_mfs.c`: STM32 + MFS shift-register adapter
- `src/main.c`: minimal entry point

## Hardware Notes

The display uses the MFS seven-segment chain driven from the Arduino-compatible
pins exposed by the Nucleo board:

- `D4` / `PB5`: latch
- `D7` / `PA8`: clock
- `D8` / `PA9`: data

## Validation

- Host-side core tests:
  `gcc -std=c11 -Wall -Wextra -pedantic -Ilib/bcd_core/src test/test_bcd_core.c lib/bcd_core/src/bcd_core.c -o /tmp/bcd_core_test && /tmp/bcd_core_test`
- Firmware build:
  `pio run -e nucleo_l476rg`
