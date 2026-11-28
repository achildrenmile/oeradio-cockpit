// Saved networks, scan results and password entry.
#include <string.h>

#include "net.h"
#include "pages.h"
#include "settings_store.h"
#include "strings.h"
#include "text.h"
#include "ui.h"

static settings_t s_settings;
static size_t s_entry;  // saved network being edited
static wifi_ap_t s_aps[NET_SCAN_MAX];
static wifi_ap_t s_selected;
static uint32_t s_generation;
static lv_obj_t *s_list, *s_password, *s_error;

static void go(lv_event_t *event)
{
    ui_show((page_t)(uintptr_t)lv_event_get_user_data(event));
}

static void full_width_button(lv_obj_t *parent, const char *text, lv_event_cb_t clicked,
                              void *user_data)
{
    lv_obj_set_width(ui_button(parent, text, clicked, user_data), LV_PCT(100));
}

static bool save_and_apply(void)
{
    if (settings_store_save(&s_settings) != ESP_OK) {
        return false;
    }
    net_settings_changed();
    return true;
}

// ---- saved networks

static void open_entry(lv_event_t *event)
{
    s_entry = (size_t)(uintptr_t)lv_event_get_user_data(event);
    ui_show(PAGE_WIFI_ENTRY);
}

static void leave(lv_event_t *event)
{
    const bool wizard = ui_wizard();
    ui_wizard_set(false);
    ui_show(wizard ? PAGE_CLOCK : PAGE_MENU);
}

void page_wifi_create(lv_obj_t *content)
{
    settings_store_get(&s_settings);
    ui_label(content, s_settings.wifi_count ? STR_WIFI_SAVED : STR_WIFI_NONE_SAVED);
    for (int i = 0; i < s_settings.wifi_count; i++) {
        full_width_button(content, s_settings.wifi[i].ssid, open_entry, (void *)(uintptr_t)i);
    }
    full_width_button(content, STR_WIFI_SCAN, go, (void *)(uintptr_t)PAGE_WIFI_SCAN);
    full_width_button(content, ui_wizard() ? STR_NEXT : STR_BACK, leave, NULL);
}

// ---- one saved network

static void raise_entry(lv_event_t *event)
{
    settings_wifi_raise(&s_settings, s_entry);
    save_and_apply();
    ui_show(PAGE_WIFI);
}

static void delete_entry(lv_event_t *event)
{
    settings_wifi_remove(&s_settings, s_entry);
    save_and_apply();
    ui_show(PAGE_WIFI);
}

void page_wifi_entry_create(lv_obj_t *content)
{
    settings_store_get(&s_settings);
    ui_label(content, s_entry < s_settings.wifi_count ? s_settings.wifi[s_entry].ssid : "");
    full_width_button(content, STR_WIFI_RAISE, raise_entry, NULL);
    full_width_button(content, STR_WIFI_DELETE, delete_entry, NULL);
    full_width_button(content, STR_BACK, go, (void *)(uintptr_t)PAGE_WIFI);
}

// ---- scan

static void select_network(lv_event_t *event)
{
    s_selected = s_aps[(size_t)(uintptr_t)lv_event_get_user_data(event)];
    ui_show(PAGE_WIFI_PASSWORD);
}

static bool listed_before(size_t index)
{
    for (size_t i = 0; i < index; i++) {
        if (strcmp(s_aps[i].ssid, s_aps[index].ssid) == 0) {
            return true;  // several access points of one network
        }
    }
    return false;
}

static void show_results(lv_timer_t *timer)
{
    uint32_t generation = 0;
    net_scan_results(s_aps, 0, &generation);
    if (generation == s_generation) {
        return;
    }
    // Show the first new result only, a later one would move the focus away.
    lv_timer_pause(timer);
    const size_t count = net_scan_results(s_aps, NET_SCAN_MAX, &generation);
    lv_obj_clean(s_list);
    if (!count) {
        ui_label(s_list, STR_WIFI_NOT_FOUND);
    }
    for (size_t i = 0; i < count; i++) {
        if (listed_before(i)) {
            continue;
        }
        lv_obj_t *button = ui_button(s_list, "", select_network, (void *)(uintptr_t)i);
        lv_obj_set_width(button, LV_PCT(100));
        lv_label_set_text_fmt(lv_obj_get_child(button, 0), STR_WIFI_AP_FMT, s_aps[i].ssid,
                              s_aps[i].rssi, s_aps[i].secure ? "" : STR_WIFI_OPEN);
    }
}

void page_wifi_scan_create(lv_obj_t *content)
{
    full_width_button(content, STR_BACK, go, (void *)(uintptr_t)PAGE_WIFI);
    s_list = lv_obj_create(content);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_size(s_list, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_list, 6, 0);
    ui_label(s_list, STR_WIFI_SCANNING);

    net_scan_results(s_aps, 0, &s_generation);
    net_scan_request();
    ui_page_timer(show_results, 300);
}

// ---- password

static void save_network(lv_event_t *event)
{
    const char *password = s_selected.secure ? lv_textarea_get_text(s_password) : "";
    if (!settings_valid_wifi(s_selected.ssid, password) ||
        (s_selected.secure && !password[0])) {
        lv_label_set_text(s_error, STR_WIFI_BAD_PASSWORD);
        return;
    }
    settings_store_get(&s_settings);
    if (!settings_wifi_put(&s_settings, s_selected.ssid, password)) {
        lv_label_set_text(s_error, STR_WIFI_FULL);
        return;
    }
    if (!save_and_apply()) {
        lv_label_set_text(s_error, STR_SAVE_FAILED);
        return;
    }
    ui_show(PAGE_WIFI);
}

static void toggle_password(lv_event_t *event)
{
    lv_textarea_set_password_mode(
        s_password, !lv_obj_has_state(lv_event_get_target_obj(event), LV_STATE_CHECKED));
}

void page_wifi_password_create(lv_obj_t *content)
{
    if (s_selected.secure) {
        lv_obj_t *caption = ui_label(content, "");
        lv_label_set_text_fmt(caption, STR_WIFI_PASSWORD_FMT, s_selected.ssid);
        s_password = ui_input(content, "", "", 64, save_network);
        lv_textarea_set_password_mode(s_password, true);
        lv_obj_t *show = ui_checkbox(content, STR_WIFI_SHOW_PASSWORD, false);
        lv_obj_add_event_cb(show, toggle_password, LV_EVENT_VALUE_CHANGED, NULL);
    } else {
        ui_label(content, s_selected.ssid);
        s_password = NULL;
    }
    s_error = ui_error_label(content);
    lv_obj_t *row = ui_row(content);
    ui_button(row, STR_SAVE, save_network, NULL);
    ui_button(row, STR_BACK, go, (void *)(uintptr_t)PAGE_WIFI_SCAN);
}
