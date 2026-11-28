// Diagnostics for display, touch, keyboard, trackball, backlight and battery.
#include <ctype.h>

#include "pages.h"
#include "settings_store.h"
#include "strings.h"
#include "ui.h"

static lv_obj_t *s_touch, *s_key, *s_trackball, *s_battery;

static void refresh(lv_timer_t *timer)
{
    static unsigned ticks;
    const board_t *board = ui_board();

    if (board->touch && lv_indev_get_state(board->touch) == LV_INDEV_STATE_PRESSED) {
        lv_point_t point;
        lv_indev_get_point(board->touch, &point);
        lv_label_set_text_fmt(s_touch, STR_TOUCH_FMT, (int)point.x, (int)point.y);
    }
    const uint32_t key = board_last_key();
    if (key) {
        lv_label_set_text_fmt(s_key, STR_KEY_FMT, isprint((int)key) ? (char)key : '.',
                              (unsigned)key);
    }
    board_trackball_counts_t counts;
    board_trackball_counts(&counts);
    lv_label_set_text_fmt(s_trackball, STR_TRACKBALL_FMT, (unsigned)counts.up,
                          (unsigned)counts.down, (unsigned)counts.left, (unsigned)counts.right,
                          (unsigned)counts.click);
    if (ticks++ % 20 == 0) {  // every two seconds
        const int millivolts = board_battery_mv();
        if (millivolts) {
            lv_label_set_text_fmt(s_battery, STR_BATTERY_FMT, millivolts,
                                  board_battery_percent(millivolts));
        } else {
            lv_label_set_text(s_battery, STR_BATTERY_NONE);
        }
    }
}

static void brightness_changed(lv_event_t *event)
{
    board_backlight_set((uint8_t)lv_slider_get_value(lv_event_get_target_obj(event)));
}

static void key_light_changed(lv_event_t *event)
{
    const bool on = lv_obj_has_state(lv_event_get_target_obj(event), LV_STATE_CHECKED);
    board_keyboard_backlight_set(on ? 127 : 0);
}

static void leave(lv_event_t *event)
{
    static settings_t settings;
    settings_store_get(&settings);
    board_backlight_set(settings.brightness);
    board_keyboard_backlight_set(0);
    ui_show(ui_wizard() ? PAGE_WIZARD_CALLSIGN : PAGE_MENU);
}

void page_hwtest_create(lv_obj_t *content)
{
    const board_t *board = ui_board();
    s_touch = ui_label(content, board->touch ? STR_TOUCH_NONE : STR_TOUCH_MISSING);
    s_key = ui_label(content, board->keypad ? STR_KEY_NONE : STR_KEY_MISSING);
    s_battery = ui_label(content, STR_BATTERY_NONE);
    s_trackball = ui_label(content, "");

    lv_obj_t *input = ui_input(content, "", "", 64, NULL);
    lv_textarea_set_placeholder_text(input, STR_INPUT_HINT);

    ui_label(content, STR_HWTEST_BRIGHTNESS);
    lv_obj_t *slider = lv_slider_create(content);
    lv_slider_set_range(slider, SETTINGS_MIN_BRIGHTNESS, 100);  // never fully dark
    lv_slider_set_value(slider, 100, LV_ANIM_OFF);
    lv_obj_set_width(slider, LV_PCT(90));
    lv_obj_set_style_margin_all(slider, 8, 0);
    lv_obj_add_event_cb(slider, brightness_changed, LV_EVENT_VALUE_CHANGED, NULL);

    ui_label(content, STR_KEY_LIGHT);
    lv_obj_t *toggle = lv_switch_create(content);
    lv_obj_add_event_cb(toggle, key_light_changed, LV_EVENT_VALUE_CHANGED, NULL);

    ui_button(content, STR_BACK, leave, NULL);
    ui_page_timer(refresh, 100);
}
