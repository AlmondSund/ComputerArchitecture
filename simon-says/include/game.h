#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    GAME_DISPLAY_WIDTH = 4U,
    GAME_SEQUENCE_CAPACITY = 32U,
    GAME_SHOW_VALUE_MS = 1200U,
    GAME_SHOW_GAP_MS = 450U,
    GAME_ROUND_PAUSE_MS = 500U,
    GAME_SUCCESS_PAUSE_MS = 900U,
    GAME_BLINK_MS = 600U
};

typedef enum {
    GAME_PHASE_SHOW_VALUE = 0,
    GAME_PHASE_SHOW_GAP,
    GAME_PHASE_WAIT_INPUT,
    GAME_PHASE_SUCCESS_PAUSE,
    GAME_PHASE_GAME_OVER
} GamePhase;

typedef struct {
    char text[GAME_DISPLAY_WIDTH + 1U];
    bool raw_override;
    uint8_t raw_segments;
} GameDisplay;

typedef struct {
    uint8_t sequence[GAME_SEQUENCE_CAPACITY];
    size_t length;
    size_t show_index;
    size_t input_index;
    uint32_t phase_elapsed_ms;
    uint32_t rng;
    GamePhase phase;
    GameDisplay display;
} Game;

void game_init(Game *game, uint32_t seed);
void game_tick(Game *game, uint32_t elapsed_ms);
bool game_press(Game *game, uint8_t value);

#endif
