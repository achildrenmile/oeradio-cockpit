// Hardware abstraction for the LilyGO T-Deck v1.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"

typedef struct {
    lv_display_t *display;
    lv_indev_t *touch;     // NULL if the controller did not answer
    lv_indev_t *keypad;    // NULL if the keyboard did not answer
    lv_indev_t *encoder;   // trackball
} board_t;

typedef struct {
    uint32_t up, down, left, right, click;
} board_trackball_counts_t;

// Powers the peripherals and starts LVGL. Only a display failure is fatal.
esp_err_t board_init(board_t *board);

// Lock the LVGL mutex before calling lv_* from outside LVGL callbacks.
bool board_lvgl_lock(uint32_t timeout_ms);
void board_lvgl_unlock(void);

void board_backlight_set(uint8_t percent);
esp_err_t board_keyboard_backlight_set(uint8_t duty);

// Battery voltage in mV, 0 if it cannot be read. Shows the charge voltage on USB power.
int board_battery_mv(void);
int board_battery_percent(int millivolts);

// Raw values for diagnostics.
uint32_t board_last_key(void);
void board_trackball_counts(board_trackball_counts_t *counts);
