// Pure functions without hardware access, tested on the host (test_host/).
#pragma once

#include <stdint.h>

#define BACKLIGHT_STEPS 16

// 0..100 % to a driver step 0..16. Any non-zero percentage stays visible.
int board_math_backlight_step(int percent);
// Pulses that take the dimmer from one step (1..16) to another (1..16).
int board_math_backlight_pulses(int from_step, int to_step);
// Rough state of charge of a single LiPo cell.
int board_math_battery_percent(int millivolts);
