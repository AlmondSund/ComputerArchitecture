# Sense properly the temperature using the STM32 nucleo L476RG via DMA based on the examples of the official STM repo STM32CubeL4

This project samples the STM32L476RG internal temperature sensor with ADC1 and
DMA.

Runtime behavior:

- ADC1 scans two internal channels continuously:
  - channel 17: internal temperature sensor
  - channel 0: VREFINT
- DMA1 Channel 1 stores the ADC sequence into a circular sample buffer.
- The firmware averages the DMA buffer, compensates the temperature reading with
  VREFINT, and converts it using the factory calibration values stored in system
  memory.
- The Multi-Function Shield display shows the rounded temperature as `NN C`.
- USART2 on the ST-LINK virtual COM port prints one debug line per second.

Source layout:

- `src/main.c`: timing and application orchestration only
- `src/temp_sensor.c`: ADC1, DMA1 Channel 1, calibration, and temperature math
- `src/mfs_display.c`: Multi-Function Shield shift-register display driver
- `src/debug_uart.c`: USART2 debug output through the ST-LINK virtual COM port
- `src/timebase.c`: SysTick millisecond timebase

Debug output example:

```text
adc-temp status=OK raw_temp=960 raw_vref=1502 vdda_mv=3001 temp_c=27.34
```

Important STM32L476 calibration addresses used by the firmware:

- `TS_CAL1`: `0x1FFF75A8`, measured at 30 C
- `TS_CAL2`: `0x1FFF75CA`, measured at 110 C
- `VREFINT_CAL`: `0x1FFF75AA`, measured at 3.0 V

Build:

```sh
turn-signals/.venv/bin/pio run -d adc -e nucleo_l476rg
```

Upload:

```sh
turn-signals/.venv/bin/pio run -d adc -e nucleo_l476rg -t upload
```

Serial verification after upload:

```sh
python -m serial.tools.miniterm /dev/ttyACM0 115200
```

If the display shows `AERR`, ADC samples are not valid yet. If it shows `DERR`,
the DMA path reported an error.
