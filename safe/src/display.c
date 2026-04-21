#include "display.h"

#include <stddef.h>

static void display_clear(Display *display)
{
    size_t index;

    if (display == NULL) {
        return;
    }

    for (index = 0U; index < DISPLAY_WIDTH; ++index) {
        display->text[index] = ' ';
    }

    display->text[DISPLAY_WIDTH] = '\0';
}

void display_init(Display *display)
{
    display_clear(display);
}

void display_show_text(Display *display, const char *text)
{
    size_t index;

    if ((display == NULL) || (text == NULL)) {
        return;
    }

    display_clear(display);

    for (index = 0U; (index < DISPLAY_WIDTH) && (text[index] != '\0'); ++index) {
        display->text[index] = text[index];
    }
}

void display_show_number(Display *display, uint32_t value, bool zero_pad)
{
    char buffer[DISPLAY_WIDTH + 1U];
    size_t index;
    uint32_t divisor;
    bool non_zero_seen;

    if (display == NULL) {
        return;
    }

    divisor = 1000U;
    non_zero_seen = zero_pad;

    for (index = 0U; index < DISPLAY_WIDTH; ++index) {
        uint32_t digit = value / divisor;
        value %= divisor;
        divisor /= 10U;

        if ((digit != 0U) || (index == (DISPLAY_WIDTH - 1U))) {
            non_zero_seen = true;
        }

        if (non_zero_seen) {
            buffer[index] = (char)('0' + digit);
        } else {
            buffer[index] = ' ';
        }
    }

    buffer[DISPLAY_WIDTH] = '\0';
    display_show_text(display, buffer);
}

void display_show_progress(Display *display, size_t filled, size_t total)
{
    char buffer[DISPLAY_WIDTH + 1U];
    size_t index;
    size_t capped_total = total;

    if (display == NULL) {
        return;
    }

    if (capped_total > DISPLAY_WIDTH) {
        capped_total = DISPLAY_WIDTH;
    }

    if (filled > capped_total) {
        filled = capped_total;
    }

    for (index = 0U; index < DISPLAY_WIDTH; ++index) {
        if (index < filled) {
            buffer[index] = '*';
        } else if (index < capped_total) {
            buffer[index] = '_';
        } else {
            buffer[index] = ' ';
        }
    }

    buffer[DISPLAY_WIDTH] = '\0';
    display_show_text(display, buffer);
}
