# c_blink: C Port of `led_blink`

## Goal
Port the `led_blink/src/main.S` behavior to C while preserving:

- Same hardware mapping (Multi Function Shield D1..D4 LEDs on NUCLEO-L476RG).
- Same active-low LED semantics.
- Same timer architecture (TIM2..TIM5, one timer per LED).
- Same timing model (runtime PSC from current RCC clock tree, target timer tick `32 kHz`).
- Same ARR values and resulting blink rates.

## Source layout

- `src/main.c`: Application loop and per-channel state machine.
- `src/mfs_led_driver.c`: Multifunction shield LED driver (GPIO init, set/toggle/off).
- `src/blink_timebase.c`: Timer clock enable, PSC computation, timer start, update-flag consume.
- `include/mfs_led_driver.h`: Public LED driver API.
- `include/blink_timebase.h`: Public timebase/timer API.

## Porting equivalence with assembly

1. GPIOA/GPIOB clocks are enabled before any pin configuration.
2. LEDs are forced OFF before changing pin modes to avoid startup glitches.
3. PA5/PA6/PA7/PB6 are configured as outputs.
4. TIM2/TIM3/TIM4/TIM5 clocks are enabled.
5. `PSC = (TIMCLK1 / 32000) - 1` is computed from current RCC configuration.
6. Timers are started with:
   - `TIM2 ARR=15999` (D1 -> 1 blink/s)
   - `TIM3 ARR=7999`  (D2 -> 2 blinks/s)
   - `TIM4 ARR=5332`  (D3 -> 3 blinks/s approx)
   - `TIM5 ARR=3999`  (D4 -> 4 blinks/s)
7. Main loop polls `UIF`, clears it, toggles software state, and updates each LED.
