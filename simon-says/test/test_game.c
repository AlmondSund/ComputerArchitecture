#include "game.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

static void tick(Game *game, uint32_t elapsed_ms)
{
    uint32_t elapsed = 0U;

    while (elapsed < elapsed_ms) {
        game_tick(game, 10U);
        elapsed += 10U;
    }
}

static void finish_show_sequence(Game *game)
{
    size_t index;

    for (index = 0U; index < game->length; ++index) {
        tick(game, GAME_SHOW_VALUE_MS);
        tick(game, GAME_SHOW_GAP_MS);
    }
}

static void test_initial_sequence_waits_for_first_input(void)
{
    Game game;

    game_init(&game, 1U);
    assert(game.length == 1U);
    assert(game.phase == GAME_PHASE_SHOW_VALUE);

    finish_show_sequence(&game);

    assert(game.phase == GAME_PHASE_WAIT_INPUT);
    assert(strcmp(game.display.text, "_   ") == 0);
}

static void test_correct_round_adds_one_more_value(void)
{
    Game game;
    uint8_t first_value;

    game_init(&game, 1U);
    first_value = game.sequence[0];
    finish_show_sequence(&game);

    assert(game_press(&game, first_value));
    assert(game.phase == GAME_PHASE_SUCCESS_PAUSE);
    assert(game.length == 2U);

    tick(&game, GAME_SUCCESS_PAUSE_MS);
    assert(game.phase == GAME_PHASE_SHOW_VALUE);
}

static void test_wrong_input_blinks_all_segments_forever(void)
{
    Game game;
    uint8_t wrong_value;

    game_init(&game, 1U);
    finish_show_sequence(&game);

    wrong_value = (uint8_t)((game.sequence[0] % 3U) + 1U);
    assert(game_press(&game, wrong_value));
    assert(game.phase == GAME_PHASE_GAME_OVER);
    assert(game.display.raw_override);
    assert(game.display.raw_segments == 0x7FU);

    tick(&game, GAME_BLINK_MS);
    assert(game.phase == GAME_PHASE_GAME_OVER);
    assert(game.display.raw_segments == 0U);
}

static void test_input_ignored_while_sequence_is_showing(void)
{
    Game game;

    game_init(&game, 1U);
    assert(!game_press(&game, game.sequence[0]));
    assert(game.phase == GAME_PHASE_SHOW_VALUE);
    assert(game.input_index == 0U);
}

int main(void)
{
    test_initial_sequence_waits_for_first_input();
    test_correct_round_adds_one_more_value();
    test_wrong_input_blinks_all_segments_forever();
    test_input_ignored_while_sequence_is_showing();
    return 0;
}
