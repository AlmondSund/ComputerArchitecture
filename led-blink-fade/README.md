# led-blink-fade

PlatformIO project for the `nucleo_l476rg` board using STM32Cube HAL.

## Architecture

- `src/platform`: system clock, error handlers, MSP setup, and interrupt handlers.
- `src/bsp`: board support for LD2 on `PA5` using `TIM2_CH1` PWM.
- `src/service`: perceptual brightness conversion through a gamma 2.2 table.
- `src/app`: cooperative application task and fade state machine.

## Behavior

The LED performs a one-second fade cycle:

- 0 ms to 500 ms: visual brightness rises from 0 to 64.
- 500 ms to 1000 ms: visual brightness falls from 64 to 0.

The application task runs cooperatively every 10 ms. PWM runs at 1 kHz.

## Omarchy Build And Upload

This repository currently has PlatformIO available through the `turn-signals`
virtual environment. From this directory, use:

```sh
../turn-signals/.venv/bin/pio run -e nucleo_l476rg
```

To upload through the Nucleo board's on-board ST-LINK:

```sh
../turn-signals/.venv/bin/pio run -e nucleo_l476rg -t upload
```

The project explicitly uses `upload_protocol = stlink` and `debug_tool = stlink`
in `platformio.ini`.

If upload fails with `OpenOCD init failed`, first verify that the host sees the
ST-LINK:

```sh
lsusb | grep -i st-link
st-info --probe
```

`lsusb` seeing `0483:374b ST-LINK/V2.1` but `st-info --probe` reporting zero
programmers means the project built correctly, but Linux cannot talk to the
target through the ST-LINK debug interface. Replug the board, try a different
USB data cable or port, and check that the Nucleo target-power/debug jumpers are
installed as expected.
