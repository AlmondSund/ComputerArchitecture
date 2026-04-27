# dimmer-cube

This project replicates the `dimmer-cmsis` algorithm on the same STM32
NUCLEO-L476RG + Multi-Function Shield hardware, but the board support is
implemented with STM32Cube HAL instead of direct CMSIS register access.

## Controls

- `S1 / A1`: decrease brightness in dimming mode, or move to the previous diode in selection mode
- `S2 / A2`: short press enters selection mode, long press selects the highlighted diode
- `S3 / A3`: increase brightness in dimming mode, or move to the next diode in selection mode

## Behavior

- The firmware starts with `D1` as the active LED.
- In `DIMMING` state, `S1` and `S3` decrease or increase only the active LED brightness.
- A short press on `S2` enters `SELECT_LED`.
- In `SELECT_LED`, `S1` and `S3` move through `D1` to `D4` with wraparound.
- A long press on `S2` commits the highlighted diode and returns to `DIMMING`.
- Each LED keeps its last brightness when another LED becomes active.
- Brightness is stored as perceptual percent in `5%` steps and mapped through
  a gamma-2 transfer so the displayed percentage better matches perceived
  brightness than raw linear PWM duty would.

## Architecture

- `src/main.c`: deterministic scheduler for input scan, FSM tick, and output refresh
- `src/app_fsm.c`: application state machine for LED selection and dimming policy
- `src/buttons.c`: bounded queue of debounced button events, including short/long `S2` actions
- `src/display.c`: logical 4-character display model
- `src/hw.c`: STM32Cube HAL adapter for button timing, display multiplexing, and LED PWM output

The hardware adapter uses the same verified shield mapping as `dimmer-cmsis`:

- LEDs: `PA5`, `PA6`, `PA7`, `PB6`
- Buttons: `PA1`, `PA4`, `PB0`
- Display shift-register lines: `PB5`, `PA8`, `PA9`

## Validation

- Host-side FSM test:
  `gcc -std=c11 -Wall -Wextra -pedantic -Iinclude test/test_dimmer_logic.c src/app_fsm.c src/buttons.c src/display.c -o /tmp/dimmer_cube_logic_test && /tmp/dimmer_cube_logic_test`
- Firmware build:
  `/home/mitin/.platformio/penv/bin/pio run -e nucleo_l476rg`
