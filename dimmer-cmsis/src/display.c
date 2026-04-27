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
        const uint32_t digit = value / divisor;

        value %= divisor;
        divisor /= 10U;

        if ((digit != 0U) || (index == (DISPLAY_WIDTH - 1U))) {
            non_zero_seen = true;
        }

        buffer[index] = non_zero_seen ? (char)('0' + digit) : ' ';
    }

    buffer[DISPLAY_WIDTH] = '\0';
    display_show_text(display, buffer);
}
