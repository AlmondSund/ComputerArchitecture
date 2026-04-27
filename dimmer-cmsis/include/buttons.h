#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdbool.h>
#include <stddef.h>

#include "config.h"

typedef enum {
    BUTTON_DECREASE = 0,
    BUTTON_SELECT_SHORT,
    BUTTON_INCREASE,
    BUTTON_SELECT_LONG
} ButtonEvent;

/*
 * Bounded queue of debounced button-press events. Hardware polling can push
 * edges here without coupling the app state machine to scan timing.
 */
typedef struct {
    ButtonEvent queue[BUTTON_QUEUE_CAPACITY];
    size_t head;
    size_t tail;
    size_t count;
} ButtonQueue;

void buttons_init(ButtonQueue *queue);
bool buttons_push(ButtonQueue *queue, ButtonEvent button);
bool buttons_pop(ButtonQueue *queue, ButtonEvent *button);

#endif
