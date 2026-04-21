# Safe

Safe is an embedded access-control project built around an **STM32 NUCLEO-L476RG**, an **Arduino Multi-Function Shield**, and a **4x4 matrix keypad (ZRX543)**.

The project turns these components into a small but structured electronic safe interface with PIN entry, lockout logic, menu navigation, audible feedback, visual status indicators, and an extensible firmware architecture based on finite state machines.

---

## Overview

This project is not just a simple "type the correct code and unlock" demo.

The goal is to build a compact embedded system that integrates:

- a **4x4 keypad** as the main human input device
- a **4-digit 7-segment display** as the main visual interface
- **LEDs** for state indication
- a **buzzer** for acoustic feedback
- optional **local buttons** on the shield for service, diagnostics, or maintenance features

The result is a didactic yet serious embedded project that teaches:

- matrix keypad scanning
- GPIO design
- debouncing
- event-driven programming
- finite state machines
- non-blocking timing
- user-interface design under hardware constraints
- basic access-control logic

---

## Main Features

### 1. PIN-based access control
The user enters a numeric PIN through the ZRX543 keypad.  
The system validates the sequence and grants or denies access.

### 2. Lockout after repeated failures
After a configurable number of invalid attempts, the system enters a temporary lockout state.  
A countdown is shown on the display and a warning pattern is activated through LEDs and buzzer.

### 3. Administrative menu
A master PIN can unlock an administrator mode where it is possible to:

- change the user PIN
- change the lockout duration
- enable or disable sound
- inspect the failed-attempt counter
- run diagnostics

### 4. Real-time feedback
The Multi-Function Shield provides immediate feedback:

- 7-segment display shows status, menus, and countdowns
- LEDs show success, error, or armed state
- buzzer confirms keypresses and signals alarms

### 5. Extensible software architecture
The firmware is designed in modules, so it can later be extended with:

- relay or servo lock actuation
- persistent storage in Flash/EEPROM emulation
- event logs
- one-time codes
- UART diagnostics
- low-power modes

---

## Hardware

### Required Components

- **STM32 NUCLEO-L476RG**
- **Arduino Multi-Function Shield**
- **4x4 matrix keypad ZRX543**
- jumper wires
- optional breadboard
- optional relay/servo for physical locking

### Core Hardware Roles

#### STM32 NUCLEO-L476RG
Main controller.  
Responsible for keypad scanning, state logic, display handling, timing, and output control.

#### Multi-Function Shield
Provides:

- 4-digit 7-segment display
- LEDs
- buzzer
- local pushbuttons

#### ZRX543 keypad
Provides the main input interface.  
It is a passive 4x4 matrix keypad with 8 lines: 4 rows and 4 columns.

---

## System Concept

At the logical level, the system can be viewed as:

1. **Input acquisition**
   - keypad scan
   - button sampling
   - debounce
   - event generation

2. **Control layer**
   - state machine
   - PIN buffer
   - timing rules
   - lockout policy
   - admin operations

3. **Output layer**
   - display update
   - LED patterns
   - buzzer patterns
   - optional lock actuator

This separation is fundamental because it prevents the project from degenerating into a large `main.c` full of blocking delays and intertwined logic.

---

## Finite State Machine

The project is structured around a finite state machine.

### Main states

- `STATE_IDLE`
- `STATE_ENTER_PIN`
- `STATE_VALIDATE_PIN`
- `STATE_ACCESS_GRANTED`
- `STATE_ACCESS_DENIED`
- `STATE_LOCKOUT`
- `STATE_ADMIN_AUTH`
- `STATE_ADMIN_MENU`
- `STATE_CHANGE_PIN`
- `STATE_SET_LOCKOUT`
- `STATE_DIAGNOSTICS`

### State descriptions

#### `STATE_IDLE`
System waits for user activity.  
Display shows idle pattern such as `SAFE`, `----`, or `0000`.

#### `STATE_ENTER_PIN`
User is entering digits from the keypad.  
The display shows masked or partial progress.

#### `STATE_VALIDATE_PIN`
The entered sequence is compared against the stored PIN.

#### `STATE_ACCESS_GRANTED`
Correct code entered.  
Green indication, success tone, optional unlock action.

#### `STATE_ACCESS_DENIED`
Incorrect code entered.  
Red indication, error tone, attempt counter increment.

#### `STATE_LOCKOUT`
Too many failed attempts.  
User input is ignored until the countdown expires.

#### `STATE_ADMIN_AUTH`
Entry point for administrative mode.

#### `STATE_ADMIN_MENU`
Allows configuration and diagnostics.

#### `STATE_CHANGE_PIN`
Receives a new PIN and stores it.

#### `STATE_SET_LOCKOUT`
Changes the lockout duration parameter.

#### `STATE_DIAGNOSTICS`
Shows raw input/output tests and internal status.

---

## Suggested Keypad Mapping

The keypad layout is assumed as:

| Row/Col | 1 | 2 | 3 | 4 |
|--------:|---|---|---|---|
| 1 | 1 | 2 | 3 | A |
| 2 | 4 | 5 | 6 | B |
| 3 | 7 | 8 | 9 | C |
| 4 | * | 0 | # | D |

### Suggested meanings

- `0..9` → numeric PIN entry
- `#` → confirm / enter
- `*` → clear / cancel / backspace
- `A` → admin mode
- `B` → menu next
- `C` → menu previous
- `D` → emergency clear / special action

This mapping can be changed in firmware.

---

## Example User Flow

### Normal access
1. User presses keys on the keypad.
2. Digits are appended to the entry buffer.
3. User confirms with `#`.
4. Firmware validates the PIN.
5. If valid:
   - success tone
   - green LED
   - display shows success message
   - optional unlock action
6. If invalid:
   - failure tone
   - red LED
   - failed-attempt counter increments

### Lockout flow
1. User exceeds maximum failed attempts.
2. System enters `STATE_LOCKOUT`.
3. Display shows remaining time.
4. LEDs and buzzer indicate warning.
5. Input is ignored until the timer expires.

### Admin flow
1. User enters admin command, for example `A`.
2. System requests master PIN.
3. On success, admin menu appears.
4. User can navigate and modify configuration.

---

## Software Architecture

The project should be split into modules.

### `app_fsm.c / app_fsm.h`
Contains the main finite state machine and all state transitions.

### `keypad.c / keypad.h`
Responsible for:

- row/column scan
- debounce
- key decode
- key events

### `display.c / display.h`
Responsible for:

- 7-segment encoding
- display multiplexing
- text and numeric rendering
- refresh scheduling

### `buzzer.c / buzzer.h`
Responsible for:

- simple tones
- beeps
- alarm patterns
- timing control

### `indicators.c / indicators.h`
Responsible for LEDs and visual patterns.

### `storage.c / storage.h`
Responsible for persistent settings such as:

- user PIN
- admin PIN
- lockout duration
- sound enabled flag

### `buttons.c / buttons.h`
Optional service buttons from the shield.

### `main.c`
Only initializes hardware, starts the scheduler/timers, and dispatches the application.

---

## Non-Blocking Design Philosophy

A critical design goal is to avoid blocking code.

The system should **not** depend on long `HAL_Delay()` calls for normal operation, because that would break:

* keypad responsiveness
* display refresh
* buzzer timing
* lockout countdown accuracy
* menu usability

Instead, the firmware should use:

* timer ticks
* periodic scan/update functions
* event flags
* state-based behavior

This project is an excellent exercise in embedded time management.

---

## Keypad Scanning Principle

The keypad is a passive matrix, so it does not generate a number directly.

The MCU must:

1. configure rows as outputs
2. configure columns as inputs with pull-up or pull-down
3. drive one row active at a time
4. read column states
5. determine which switch intersection is closed
6. debounce the result
7. emit a stable key event

This is one of the central educational aspects of the project.

---

## Display Strategy

The 4-digit display is limited, so the interface must be concise.

Examples of what can be shown:

* `----` idle
* `1234` entered code
* `FAIL` invalid PIN
* `OPEN` or `PASS` valid access
* `LOCK` lockout active
* countdown values such as `0030`
* menu identifiers such as `A001`, `A002`

Letters on 7-segment displays are approximate, so UI conventions should remain simple and readable.

---

## Security Logic

This project is educational and not intended as a high-security commercial system.
Still, it includes meaningful access-control ideas:

* maximum retry count
* timed lockout
* admin authentication
* masked entry behavior
* optional event logging
* optional persistent configuration

Possible future improvements:

* hash-based PIN storage
* tamper detection
* randomized delays
* audit trail
* RTC time stamps
* secure boot considerations

---

## Example Configuration Parameters

```c
#define USER_PIN_LENGTH          4
#define ADMIN_PIN_LENGTH         6
#define MAX_FAILED_ATTEMPTS      3
#define DEFAULT_LOCKOUT_SECONDS  30
#define KEYPAD_DEBOUNCE_MS       30
#define DISPLAY_REFRESH_MS       2
#define BUZZER_BEEP_MS           80
```

---

## Suggested Pin Strategy

Because the Arduino Multi-Function Shield may already occupy many Arduino-compatible pins, the keypad is preferably connected through **free GPIOs from the NUCLEO Morpho headers**.

A practical firmware design should keep the keypad pin definitions isolated in one header, for example:

```c
#define KP_ROW1_GPIO_Port   GPIOC
#define KP_ROW1_Pin         GPIO_PIN_x
#define KP_ROW2_GPIO_Port   GPIOC
#define KP_ROW2_Pin         GPIO_PIN_x
#define KP_ROW3_GPIO_Port   GPIOC
#define KP_ROW3_Pin         GPIO_PIN_x
#define KP_ROW4_GPIO_Port   GPIOD
#define KP_ROW4_Pin         GPIO_PIN_x

#define KP_COL1_GPIO_Port   GPIOC
#define KP_COL1_Pin         GPIO_PIN_x
#define KP_COL2_GPIO_Port   GPIOC
#define KP_COL2_Pin         GPIO_PIN_x
#define KP_COL3_GPIO_Port   GPIOC
#define KP_COL3_Pin         GPIO_PIN_x
#define KP_COL4_GPIO_Port   GPIOC
#define KP_COL4_Pin         GPIO_PIN_x
```

The exact mapping depends on the final wiring.

### Prototype wiring used by this firmware

This repository now assumes the following concrete wiring for bring-up on the
STM32 NUCLEO-L476RG:

- Multi-Function Shield display:
  - `D4 / PB5`  -> latch
  - `D7 / PA8`  -> clock
  - `D8 / PA9`  -> data
- Multi-Function Shield LEDs:
  - `D13 / PA5`, `D12 / PA6`, `D11 / PA7`, `D10 / PB6`
- Multi-Function Shield buttons:
  - `A1 / PA1` -> service shortcut mapped to keypad `A`
  - `A2 / PA4` -> service shortcut mapped to keypad `#`
  - `A3 / PB0` -> service shortcut mapped to keypad `*`
- Multi-Function Shield buzzer:
  - `D3 / PB3`
- 4x4 keypad on ST morpho headers:
  - rows: `PC10`, `PC11`, `PC12`, `PD2`
  - columns: `PC8`, `PC6`, `PC5`, `PC4`

If the keypad is wired differently, update the pin definitions in
`safe/src/hw.c`.

---

## Build Environment

Recommended toolchain:

* **STM32CubeIDE**
* **STM32 HAL**
* optional migration later to **LL drivers** for finer control

### Basic workflow

1. Create a new STM32CubeIDE project for `NUCLEO-L476RG`.
2. Configure GPIOs for:

   * keypad rows
   * keypad columns
   * shield-related peripherals
3. Generate code.
4. Add the project modules.
5. Implement periodic scanning and state machine update.
6. Flash and test incrementally.

---

## Development Plan

### Phase 1

Bring up the shield:

* display test
* LED test
* buzzer test
* button test

### Phase 2

Bring up the keypad:

* row/column continuity test
* key identification
* debounce validation

### Phase 3

Build the FSM:

* idle
* pin entry
* validation
* grant/deny
* lockout

### Phase 4

Add admin mode:

* admin authentication
* parameter editing
* configuration display

### Phase 5

Add persistence and diagnostics:

* save PIN in nonvolatile memory
* add event counters
* add hardware test menu

---

## Diagnostics Ideas

A dedicated diagnostic mode can be extremely useful during bring-up.

Examples:

* show raw row/column values
* show decoded key
* blink an LED on every stable keypress
* display attempt counter
* test buzzer patterns
* test each display digit individually

This mode can be entered via a local shield button or an admin menu option.

---

## Future Extensions

Safe can be expanded far beyond the initial prototype.

### Hardware extensions

* relay output
* servo latch
* magnetic lock driver
* RTC module
* EEPROM
* UART terminal
* LCD/OLED
* RFID or NFC
* fingerprint module

### Firmware extensions

* event history
* role-based PINs
* timed unlock windows
* duress PIN
* watchdog integration
* low-power standby
* remote configuration

---

## Educational Value

This project is especially valuable because it sits at the intersection of:

* digital electronics
* embedded firmware
* real-time logic
* human-machine interface
* system architecture

It is an ideal bridge between "basic peripheral exercises" and "small embedded products with real behavior."

---

## Project Status

```text
[ ] Shield bring-up
[ ] Keypad scan
[ ] Debounce
[ ] PIN buffer
[ ] Validation logic
[ ] Lockout state
[ ] Admin menu
[ ] Persistent storage
[ ] Optional actuator output
```

---

## License

This project is intended for educational and prototyping purposes.

You may use, modify, and extend it freely.

---

## Author

Developed by Martín Ramírez Espinosa as an STM32 embedded systems project based on:

* STM32 NUCLEO-L476RG
* Arduino Multi-Function Shield
* ZRX543 4x4 matrix keypad

---

## Final Goal

Safe is meant to be a compact embedded platform that demonstrates how a seemingly simple idea — a keypad-controlled safe — actually requires solid engineering in:

* interface design
* time management
* modular software
* hardware abstraction
* state-based reasoning

In that sense, the project is not just about opening a lock.
It is about learning how to build a coherent embedded system.
