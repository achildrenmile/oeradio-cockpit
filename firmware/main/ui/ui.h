// Page switching, status bar and shared widgets. All calls need the LVGL lock.
#pragma once

#include <stdbool.h>

#include "board.h"
#include "lvgl.h"

typedef enum {
    PAGE_CLOCK,
    PAGE_MENU,
    PAGE_STATION,
    PAGE_WIZARD_CALLSIGN,
    PAGE_WIZARD_LOCATOR,
    PAGE_WIFI,
    PAGE_WIFI_ENTRY,
    PAGE_WIFI_SCAN,
    PAGE_WIFI_PASSWORD,
    PAGE_FILTER,
    PAGE_DISPLAY,
    PAGE_ADVANCED,
    PAGE_HWTEST,
    PAGE_COUNT,
} page_t;

void ui_start(const board_t *board);
const board_t *ui_board(void);

// Switches the page after the current event has been handled.
void ui_show(page_t page);

// True while the first run wizard is active.
bool ui_wizard(void);
void ui_wizard_set(bool active);

// A timer that is removed when the page changes. One per page.
void ui_page_timer(lv_timer_cb_t callback, uint32_t period_ms);

// Widgets are added to the focus group in creation order.
lv_obj_t *ui_label(lv_obj_t *parent, const char *text);
lv_obj_t *ui_button(lv_obj_t *parent, const char *text, lv_event_cb_t clicked, void *user_data);
// Labelled one line input. "ready" runs when Enter is pressed.
lv_obj_t *ui_input(lv_obj_t *parent, const char *caption, const char *value, uint32_t max_length,
                   lv_event_cb_t ready);
lv_obj_t *ui_checkbox(lv_obj_t *parent, const char *text, bool checked);
lv_obj_t *ui_row(lv_obj_t *parent);  // wrapping horizontal container
// Empty line for validation errors.
lv_obj_t *ui_error_label(lv_obj_t *parent);
