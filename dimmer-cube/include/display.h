#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

/*
 * Logical four-character display state consumed by the board adapter.
 */
typedef struct {
    char text[DISPLAY_WIDTH + 1U];
} Display;

void display_init(Display *display);
void display_show_text(Display *display, const char *text);
void display_show_number(Display *display, uint32_t value, bool zero_pad);

#endif
