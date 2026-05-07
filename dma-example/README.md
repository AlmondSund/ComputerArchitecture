# DMA Example using nucleo l476rg board with a Multifunction shield connected on top of it

This project demonstrates DMA on the STM32L476RG while refreshing the
Multi-Function Shield four-digit seven-segment display.

The display still uses the shield's Arduino-compatible shift-register wiring:

- `D4` / `PB5`: latch
- `D7` / `PA8`: shift clock
- `D8` / `PA9`: shift data

Each display refresh prepares a two-byte frame in memory:

1. active-low segment byte
2. digit-select byte

TIM2 selects the next digit every 2 ms and starts a DMA1 Channel 1 memory-to-memory
transfer from the prepared frame into the active display frame. The DMA interrupt
then writes the completed frame to the shield's shift registers using the same
GPIO timing used by the other MFS projects in this repository.

Expected visible behavior: the shield display scrolls `dMA` across the four
digits. If the DMA controller reports a transfer error, the display switches to
`EEEE`.

Build:

```sh
turn-signals/.venv/bin/pio run -d dma-example -e nucleo_l476rg
```
