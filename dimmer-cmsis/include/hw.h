#ifndef HW_H
#define HW_H

#include <stdint.h>

#include "app_fsm.h"

/*
 * STM32 NUCLEO-L476RG + Multi-Function Shield hardware boundary.
 * The potentiometer is read through the shield analog pin while the display and
 * LEDs reuse the verified MFS wiring used by sibling projects in this repo.
 */
void hw_init(void);
uint32_t hw_now_ms(void);
void hw_poll_inputs(App *app, uint32_t now_ms);
void hw_apply_outputs(const App *app, uint32_t now_ms);
void hw_wait(void);

#endif
