#include "board_math.h"

#include <stddef.h>

int board_math_backlight_step(int percent)
{
    if (percent <= 0) {
        return 0;
    }
    if (percent > 100) {
        percent = 100;
    }
    return (percent * BACKLIGHT_STEPS + 99) / 100;
}

int board_math_backlight_pulses(int from_step, int to_step)
{
    // Each pulse lowers the brightness by one step and wraps from 1 to 16.
    return (from_step - to_step + BACKLIGHT_STEPS) % BACKLIGHT_STEPS;
}

int board_math_battery_percent(int millivolts)
{
    static const struct {
        int mv, percent;
    } curve[] = {{3300, 0},  {3600, 5},  {3700, 15}, {3750, 30}, {3800, 45},
                 {3850, 60}, {3950, 75}, {4050, 88}, {4150, 100}};
    const size_t points = sizeof(curve) / sizeof(curve[0]);

    if (millivolts <= curve[0].mv) {
        return 0;
    }
    for (size_t i = 1; i < points; i++) {
        if (millivolts <= curve[i].mv) {
            return curve[i - 1].percent + (millivolts - curve[i - 1].mv) *
                                              (curve[i].percent - curve[i - 1].percent) /
                                              (curve[i].mv - curve[i - 1].mv);
        }
    }
    return 100;
}
