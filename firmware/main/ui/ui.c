#include "ui.h"

#include <time.h>

#include "net.h"
#include "pages.h"
#include "store.h"
#include "strings.h"

#define BAR_HEIGHT 20

static void (*const s_pages[PAGE_COUNT])(lv_obj_t *) = {
    [PAGE_CLOCK] = page_clock_create,
    [PAGE_MENU] = page_menu_create,
    [PAGE_STATION] = page_station_create,
    [PAGE_WIZARD_CALLSIGN] = page_wizard_callsign_create,
    [PAGE_WIZARD_LOCATOR] = page_wizard_locator_create,
    [PAGE_WIFI] = page_wifi_create,
    [PAGE_WIFI_ENTRY] = page_wifi_entry_create,
    [PAGE_WIFI_SCAN] = page_wifi_scan_create,
    [PAGE_WIFI_PASSWORD] = page_wifi_password_create,
    [PAGE_FILTER] = page_filter_create,
    [PAGE_DISPLAY] = page_display_create,
    [PAGE_ADVANCED] = page_advanced_create,
    [PAGE_HWTEST] = page_hwtest_create,
};

static const board_t *s_board;
static lv_group_t *s_group;
static lv_timer_t *s_page_timer;
static lv_obj_t *s_screen;
static lv_obj_t *s_bar_wifi, *s_bar_mqtt, *s_bar_time, *s_bar_battery;
static bool s_wizard;

// The bar lives on the top layer, so it survives page changes.
static void bar_refresh(lv_timer_t *timer)
{
    static unsigned ticks;
    net_status_t status;
    net_status_get(&status);

    switch (status.wifi) {
    case WIFI_STATE_NONE_SAVED:
        lv_label_set_text(s_bar_wifi, STR_BAR_NO_WIFI);
        break;
    case WIFI_STATE_SEARCHING:
        lv_label_set_text(s_bar_wifi, STR_BAR_SEARCHING);
        break;
    case WIFI_STATE_CONNECTING:
        lv_label_set_text_fmt(s_bar_wifi, STR_BAR_CONNECTING_FMT, status.ssid);
        break;
    case WIFI_STATE_CONNECTED:
        lv_label_set_text_fmt(s_bar_wifi, STR_BAR_WIFI_FMT, status.ssid, status.rssi);
        break;
    }

    if (store_update_recommended()) {
        lv_label_set_text(s_bar_mqtt, STR_BAR_UPDATE);
    } else if (status.mqtt_connected && store_server_offline()) {
        lv_label_set_text(s_bar_mqtt, STR_BAR_SERVER_OFFLINE);
    } else if (status.mqtt_connected) {
        lv_label_set_text(s_bar_mqtt, STR_BAR_MQTT_OK);
    } else if (status.mqtt_retry_in_s > 0) {
        lv_label_set_text_fmt(s_bar_mqtt, STR_BAR_MQTT_RETRY_FMT, status.mqtt_retry_in_s);
    } else {
        lv_label_set_text(s_bar_mqtt, STR_BAR_MQTT_OFF);
    }

    if (status.time_valid) {
        const time_t now = time(NULL);
        struct tm utc;
        gmtime_r(&now, &utc);
        lv_label_set_text_fmt(s_bar_time, STR_BAR_TIME_FMT, utc.tm_hour, utc.tm_min);
    } else {
        lv_label_set_text(s_bar_time, STR_BAR_NO_TIME);
    }

    if (ticks++ % 30 == 0) {
        const int millivolts = board_battery_mv();
        if (millivolts) {
            lv_label_set_text_fmt(s_bar_battery, STR_BAR_BATTERY_FMT,
                                  board_battery_percent(millivolts));
        } else {
            lv_label_set_text(s_bar_battery, "");
        }
    }
}

static lv_obj_t *bar_label(lv_obj_t *bar, lv_align_t align, int x)
{
    lv_obj_t *label = lv_label_create(bar);
    lv_label_set_text(label, "");
    lv_obj_align(label, align, x, 0);
    return label;
}

static void bar_create(void)
{
    lv_obj_t *bar = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, 320, BAR_HEIGHT);
    lv_obj_set_style_bg_color(bar, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    s_bar_wifi = bar_label(bar, LV_ALIGN_LEFT_MID, 4);
    s_bar_mqtt = bar_label(bar, LV_ALIGN_LEFT_MID, 118);
    s_bar_time = bar_label(bar, LV_ALIGN_RIGHT_MID, -42);
    s_bar_battery = bar_label(bar, LV_ALIGN_RIGHT_MID, -4);
    lv_timer_create(bar_refresh, 1000, NULL);
    bar_refresh(NULL);
}

static void show_now(void *argument)
{
    const page_t page = (page_t)(uintptr_t)argument;
    if (s_page_timer) {
        lv_timer_delete(s_page_timer);
        s_page_timer = NULL;
    }
    lv_group_remove_all_objs(s_group);

    lv_obj_t *old = s_screen;
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *content = lv_obj_create(s_screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_pos(content, 0, BAR_HEIGHT);
    lv_obj_set_size(content, 320, 240 - BAR_HEIGHT);
    lv_obj_set_style_pad_all(content, 8, 0);
    lv_obj_set_style_pad_row(content, 6, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);

    s_pages[page](content);
    lv_screen_load(s_screen);
    if (old) {
        lv_obj_delete(old);
    }
}

void ui_show(page_t page)
{
    // An Enter press that caused the change must not click something on the new page.
    lv_indev_t *indev = lv_indev_active();
    if (indev) {
        lv_indev_wait_release(indev);
    }
    lv_async_call(show_now, (void *)(uintptr_t)page);
}

void ui_start(const board_t *board)
{
    s_board = board;
    s_group = lv_group_create();
    lv_group_set_default(s_group);
    if (board->keypad) {
        lv_indev_set_group(board->keypad, s_group);
    }
    if (board->encoder) {
        lv_indev_set_group(board->encoder, s_group);
    }
    bar_create();
}

const board_t *ui_board(void)
{
    return s_board;
}

bool ui_wizard(void)
{
    return s_wizard;
}

void ui_wizard_set(bool active)
{
    s_wizard = active;
}

void ui_page_timer(lv_timer_cb_t callback, uint32_t period_ms)
{
    s_page_timer = lv_timer_create(callback, period_ms, NULL);
    lv_timer_ready(s_page_timer);
}

lv_obj_t *ui_label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_text(label, text);
    return label;
}

lv_obj_t *ui_button(lv_obj_t *parent, const char *text, lv_event_cb_t clicked, void *user_data)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_add_event_cb(button, clicked, LV_EVENT_CLICKED, user_data);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    return button;
}

// The trackball must keep moving the focus, so text fields never enter encoder edit mode.
static void keep_navigating(lv_event_t *event)
{
    lv_group_t *group = lv_obj_get_group(lv_event_get_target_obj(event));
    if (group && lv_group_get_editing(group)) {
        lv_group_set_editing(group, false);
    }
}

lv_obj_t *ui_input(lv_obj_t *parent, const char *caption, const char *value, uint32_t max_length,
                   lv_event_cb_t ready)
{
    if (caption[0]) {
        ui_label(parent, caption);
    }
    lv_obj_t *input = lv_textarea_create(parent);
    lv_textarea_set_one_line(input, true);
    lv_textarea_set_max_length(input, max_length);
    lv_textarea_set_text(input, value);
    lv_obj_set_width(input, LV_PCT(100));
    lv_obj_add_event_cb(input, keep_navigating, LV_EVENT_FOCUSED, NULL);
    if (ready) {
        lv_obj_add_event_cb(input, ready, LV_EVENT_READY, NULL);
    }
    return input;
}

lv_obj_t *ui_checkbox(lv_obj_t *parent, const char *text, bool checked)
{
    lv_obj_t *box = lv_checkbox_create(parent);
    lv_checkbox_set_text(box, text);
    if (checked) {
        lv_obj_add_state(box, LV_STATE_CHECKED);
    }
    lv_obj_add_flag(box, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    return box;
}

lv_obj_t *ui_row(lv_obj_t *parent)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_gap(row, 8, 0);
    lv_obj_set_style_pad_all(row, 3, 0);  // room for the focus outline
    return row;
}

lv_obj_t *ui_error_label(lv_obj_t *parent)
{
    lv_obj_t *label = ui_label(parent, "");
    lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_RED), 0);
    return label;
}
