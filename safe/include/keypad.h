#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdbool.h>
#include <stddef.h>

#include "config.h"

/*
 * Event queue for stable keypad events. The current prototype injects already
 * debounced keys; GPIO row/column scanning can feed the same queue later.
 */
typedef struct {
    char queue[KEYPAD_QUEUE_CAPACITY];
    size_t head;
    size_t tail;
    size_t count;
} Keypad;

void keypad_init(Keypad *keypad);
bool keypad_is_supported(char key);
bool keypad_push(Keypad *keypad, char key);
bool keypad_pop(Keypad *keypad, char *key);

#endif
