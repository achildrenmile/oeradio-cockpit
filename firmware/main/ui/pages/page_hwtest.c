#include "page_hwtest.h"

#include <ctype.h>

#include "strings.h"

static const board_t *s_board;
static lv_obj_t *s_touch, *s_key, *s_trackball, *s_battery;

static void refresh(lv_timer_t *timer)
{
    static int ticks;

    if (s_board->touch && lv_indev_get_state(s_board->touch) == LV_INDEV_STATE_PRESSED) {
        lv_point_t point;
        lv_indev_get_point(s_board->touch, &point);
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

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_pos(obj, x, y);
    return obj;
}

void page_hwtest_create(const board_t *board)
{
    s_board = board;

    // Widgets join the default group, which the trackball and keyboard navigate.
    lv_group_t *group = lv_group_create();
    lv_group_set_default(group);
    if (board->keypad) {
        lv_indev_set_group(board->keypad, group);
    }
    if (board->encoder) {
        lv_indev_set_group(board->encoder, group);
    }

    lv_obj_t *screen = lv_screen_active();
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    label(screen, STR_HWTEST_TITLE, 8, 6);
    s_touch = label(screen, board->touch ? STR_TOUCH_NONE : STR_TOUCH_MISSING, 8, 28);
    s_key = label(screen, board->keypad ? STR_KEY_NONE : STR_KEY_MISSING, 8, 48);
    s_battery = label(screen, STR_BATTERY_NONE, 8, 68);
    s_trackball = label(screen, "", 8, 88);

    lv_obj_t *input = lv_textarea_create(screen);
    lv_textarea_set_one_line(input, true);
    lv_textarea_set_placeholder_text(input, STR_INPUT_HINT);
    lv_obj_set_width(input, 304);
    lv_obj_set_pos(input, 8, 128);

    label(screen, STR_BRIGHTNESS, 8, 178);
    lv_obj_t *slider = lv_slider_create(screen);
    lv_slider_set_range(slider, 5, 100);  // never fully dark
    lv_slider_set_value(slider, 100, LV_ANIM_OFF);
    lv_obj_set_width(slider, 180);
    lv_obj_set_pos(slider, 110, 181);
    lv_obj_add_event_cb(slider, brightness_changed, LV_EVENT_VALUE_CHANGED, NULL);

    label(screen, STR_KEY_LIGHT, 8, 212);
    lv_obj_t *toggle = lv_switch_create(screen);
    lv_obj_set_pos(toggle, 110, 208);
    lv_obj_add_event_cb(toggle, key_light_changed, LV_EVENT_VALUE_CHANGED, NULL);

    lv_timer_create(refresh, 100, NULL);
}
