// Host test: cc -I../components/board test_board_math.c ../components/board/board_math.c
#include <assert.h>
#include <stdio.h>

#include "board_math.h"

// The dimmer starts at 16 and every pulse goes one step down, wrapping from 1 to 16.
static int dimmer_after(int step, int pulses)
{
    while (pulses--) {
        step = step == 1 ? BACKLIGHT_STEPS : step - 1;
    }
    return step;
}

int main(void)
{
    assert(board_math_backlight_step(-5) == 0);
    assert(board_math_backlight_step(0) == 0);
    assert(board_math_backlight_step(1) == 1);
    assert(board_math_backlight_step(50) == 8);
    assert(board_math_backlight_step(100) == 16);
    assert(board_math_backlight_step(255) == 16);

    for (int from = 1; from <= BACKLIGHT_STEPS; from++) {
        for (int to = 1; to <= BACKLIGHT_STEPS; to++) {
            assert(dimmer_after(from, board_math_backlight_pulses(from, to)) == to);
        }
    }

    assert(board_math_battery_percent(0) == 0);
    assert(board_math_battery_percent(3300) == 0);
    assert(board_math_battery_percent(4150) == 100);
    assert(board_math_battery_percent(4400) == 100);
    for (int mv = 3000, last = 0; mv <= 4300; mv++) {
        const int percent = board_math_battery_percent(mv);
        assert(percent >= last && percent <= 100);
        last = percent;
    }

    puts("board_math: ok");
    return 0;
}
