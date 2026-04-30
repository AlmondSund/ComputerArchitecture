# Simon Says
This project contains a "simon says" game using a Multifunction Shield connected to a nucleol476rg board.

## How it works
on the BCD screen will appear a random sequence of numbers in {1,2,3}, 1 number will be added to the sequence between rounds.

- Shows the initial sequence
- The user pushes the corresponding button:
-- S1-A1 = 1
-- S2-A2 = 2
-- S3-A3 = 3
Following the given sequence

- Check if the usr sequence is right
- If is right, adds a new number to the sequence and the process repeats
- If not, all the segments of the BCD start blinking until the reset button is pressed and the game restarts

# REQUIREMENTS
Absolutely all the programm MUST use interruptions 

## Implementation notes

- The main loop only sleeps with `__WFI()`.
- `SysTick_Handler` drives the game clock and display multiplexing.
- `EXTI0_IRQHandler`, `EXTI1_IRQHandler`, and `EXTI4_IRQHandler` handle S3, S1, and S2 button presses respectively.
- The game state machine is isolated in `src/game.c` so it can be tested on the host before building the STM32 firmware.
