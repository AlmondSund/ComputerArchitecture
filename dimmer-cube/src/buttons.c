#include "buttons.h"

void buttons_init(ButtonQueue *queue)
{
    if (queue == NULL) {
        return;
    }

    queue->head = 0U;
    queue->tail = 0U;
    queue->count = 0U;
}

bool buttons_push(ButtonQueue *queue, ButtonEvent button)
{
    if ((queue == NULL) || (button > BUTTON_SELECT_LONG) ||
        (queue->count >= BUTTON_QUEUE_CAPACITY)) {
        return false;
    }

    queue->queue[queue->tail] = button;
    queue->tail = (queue->tail + 1U) % BUTTON_QUEUE_CAPACITY;
    queue->count++;
    return true;
}

bool buttons_pop(ButtonQueue *queue, ButtonEvent *button)
{
    if ((queue == NULL) || (button == NULL) || (queue->count == 0U)) {
        return false;
    }

    *button = queue->queue[queue->head];
    queue->head = (queue->head + 1U) % BUTTON_QUEUE_CAPACITY;
    queue->count--;
    return true;
}
