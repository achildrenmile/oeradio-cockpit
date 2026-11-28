#pragma once

#include "board.h"
#include "driver/i2c_master.h"

esp_err_t board_display_init(lv_display_t **display);
void board_touch_init(i2c_master_bus_handle_t bus, board_t *board);
void board_keyboard_init(i2c_master_bus_handle_t bus, board_t *board);
void board_trackball_init(board_t *board);
void board_backlight_init(void);
void board_battery_init(void);
