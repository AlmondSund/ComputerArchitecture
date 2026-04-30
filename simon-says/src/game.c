#include "game.h"

#include <stddef.h>

enum {
    GAME_ALL_SEGMENTS = 0x7FU,
    GAME_LFSR_FALLBACK_SEED = 0x1U
};

static void game_display_clear(Game *game)
{
    size_t index;

    for (index = 0U; index < GAME_DISPLAY_WIDTH; ++index) {
        game->display.text[index] = ' ';
    }

    game->display.text[GAME_DISPLAY_WIDTH] = '\0';
    game->display.raw_override = false;
    game->display.raw_segments = 0U;
}

static void game_display_value(Game *game, uint8_t value)
{
    game_display_clear(game);
    game->display.text[1] = (char)('0' + value);
}

static void game_display_progress(Game *game)
{
    size_t index;

    game_display_clear(game);
    for (index = 0U; index < GAME_DISPLAY_WIDTH; ++index) {
        if (index < game->input_index) {
            game->display.text[index] = '*';
        } else if (index < game->length) {
            game->display.text[index] = '_';
        }
    }
}

static uint32_t game_next_rng(Game *game)
{
    uint32_t lsb = game->rng & 1U;

    game->rng >>= 1U;
    if (lsb != 0U) {
        game->rng ^= 0x80200003UL;
    }

    if (game->rng == 0U) {
        game->rng = GAME_LFSR_FALLBACK_SEED;
    }

    return game->rng;
}

static void game_append_value(Game *game)
{
    if (game->length >= GAME_SEQUENCE_CAPACITY) {
        return;
    }

    game->sequence[game->length] = (uint8_t)((game_next_rng(game) % 3U) + 1U);
    game->length++;
}

static void game_start_show(Game *game)
{
    game->phase = GAME_PHASE_SHOW_VALUE;
    game->show_index = 0U;
    game->input_index = 0U;
    game->phase_elapsed_ms = 0U;
    game_display_value(game, game->sequence[0]);
}

void game_init(Game *game, uint32_t seed)
{
    if (game == NULL) {
        return;
    }

    game->length = 0U;
    game->show_index = 0U;
    game->input_index = 0U;
    game->phase_elapsed_ms = 0U;
    game->rng = (seed != 0U) ? seed : GAME_LFSR_FALLBACK_SEED;
    game->phase = GAME_PHASE_SHOW_VALUE;
    game_display_clear(game);

    game_append_value(game);
    game_start_show(game);
}

void game_tick(Game *game, uint32_t elapsed_ms)
{
    if ((game == NULL) || (elapsed_ms == 0U)) {
        return;
    }

    game->phase_elapsed_ms += elapsed_ms;

    switch (game->phase) {
    case GAME_PHASE_SHOW_VALUE:
        if (game->phase_elapsed_ms >= GAME_SHOW_VALUE_MS) {
            game->phase = GAME_PHASE_SHOW_GAP;
            game->phase_elapsed_ms = 0U;
            game_display_clear(game);
        }
        break;

    case GAME_PHASE_SHOW_GAP:
        if (game->phase_elapsed_ms >= GAME_SHOW_GAP_MS) {
            game->show_index++;
            game->phase_elapsed_ms = 0U;

            if (game->show_index < game->length) {
                game->phase = GAME_PHASE_SHOW_VALUE;
                game_display_value(game, game->sequence[game->show_index]);
            } else {
                game->phase = GAME_PHASE_WAIT_INPUT;
                game_display_progress(game);
            }
        }
        break;

    case GAME_PHASE_SUCCESS_PAUSE:
        if (game->phase_elapsed_ms >= GAME_SUCCESS_PAUSE_MS) {
            game_start_show(game);
        }
        break;

    case GAME_PHASE_GAME_OVER:
        game->display.raw_override = true;
        if (((game->phase_elapsed_ms / GAME_BLINK_MS) % 2U) == 0U) {
            game->display.raw_segments = GAME_ALL_SEGMENTS;
        } else {
            game->display.raw_segments = 0U;
        }
        break;

    case GAME_PHASE_WAIT_INPUT:
    default:
        break;
    }
}

bool game_press(Game *game, uint8_t value)
{
    if ((game == NULL) || (value < 1U) || (value > 3U)) {
        return false;
    }

    if (game->phase != GAME_PHASE_WAIT_INPUT) {
        return false;
    }

    if (game->sequence[game->input_index] != value) {
        game->phase = GAME_PHASE_GAME_OVER;
        game->phase_elapsed_ms = 0U;
        game->display.raw_override = true;
        game->display.raw_segments = GAME_ALL_SEGMENTS;
        return true;
    }

    game->input_index++;
    if (game->input_index < game->length) {
        game_display_progress(game);
        return true;
    }

    game_append_value(game);
    game->phase = GAME_PHASE_SUCCESS_PAUSE;
    game->phase_elapsed_ms = 0U;
    game_display_clear(game);
    game->display.text[1] = 'O';
    game->display.text[2] = 'K';
    return true;
}
