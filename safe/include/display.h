#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

/*
 * Logical 4-digit display state. Hardware multiplexing can consume this model
 * later without changing the application state machine.
 */
typedef struct {
    char text[DISPLAY_WIDTH + 1U];
} Display;

void display_init(Display *display);
void display_show_text(Display *display, const char *text);
void display_show_number(Display *display, uint32_t value, bool zero_pad);
void display_show_progress(Display *display, size_t filled, size_t total);

#endif
