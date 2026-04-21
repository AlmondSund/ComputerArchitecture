#ifndef HW_H
#define HW_H

#include <stdint.h>

#include "app_fsm.h"

/*
 * STM32 NUCLEO-L476RG + Multi-Function Shield hardware boundary.
 * The keypad is wired to free Morpho pins rather than Arduino header pins.
 */
void hw_init(void);
uint32_t hw_now_ms(void);
void hw_poll_inputs(App *app, uint32_t now_ms);
void hw_apply_outputs(const App *app, uint32_t now_ms);
void hw_wait(void);

#endif
