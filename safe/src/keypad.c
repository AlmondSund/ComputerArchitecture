#include "keypad.h"

void keypad_init(Keypad *keypad)
{
    if (keypad == NULL) {
        return;
    }

    keypad->head = 0U;
    keypad->tail = 0U;
    keypad->count = 0U;
}

bool keypad_is_supported(char key)
{
    return ((key >= '0') && (key <= '9')) ||
           (key == '#') ||
           (key == '*') ||
           (key == 'A') ||
           (key == 'B') ||
           (key == 'C') ||
           (key == 'D');
}

bool keypad_push(Keypad *keypad, char key)
{
    if ((keypad == NULL) || !keypad_is_supported(key) ||
        (keypad->count >= KEYPAD_QUEUE_CAPACITY)) {
        return false;
    }

    keypad->queue[keypad->tail] = key;
    keypad->tail = (keypad->tail + 1U) % KEYPAD_QUEUE_CAPACITY;
    keypad->count++;
    return true;
}

bool keypad_pop(Keypad *keypad, char *key)
{
    if ((keypad == NULL) || (key == NULL) || (keypad->count == 0U)) {
        return false;
    }

    *key = keypad->queue[keypad->head];
    keypad->head = (keypad->head + 1U) % KEYPAD_QUEUE_CAPACITY;
    keypad->count--;
    return true;
}
