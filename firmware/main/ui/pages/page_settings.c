// Settings menu, station, filter, display and advanced pages, and the first two wizard steps.
#include <string.h>

#include "net.h"
#include "pages.h"
#include "settings_store.h"
#include "strings.h"
#include "text.h"
#include "ui.h"

// Only one page exists at a time, so they share the working copy and the widgets.
static settings_t s_settings;
static lv_obj_t *s_first, *s_second, *s_error;
static lv_obj_t *s_bands[SETTINGS_BAND_COUNT], *s_modes[SETTINGS_MODE_COUNT];

static void go(lv_event_t *event)
{
    ui_show((page_t)(uintptr_t)lv_event_get_user_data(event));
}

static void nav_button(lv_obj_t *parent, const char *text, page_t page)
{
    lv_obj_t *button = ui_button(parent, text, go, (void *)(uintptr_t)page);
    lv_obj_set_width(button, LV_PCT(100));
}

static bool save_and_apply(void)
{
    if (settings_store_save(&s_settings) != ESP_OK) {
        lv_label_set_text(s_error, STR_SAVE_FAILED);
        return false;
    }
    net_settings_changed();
    return true;
}

// Copies the trimmed text of an input field.
static void input_value(lv_obj_t *input, char *out, size_t size)
{
    const char *text = lv_textarea_get_text(input);
    while (*text == ' ') {
        text++;
    }
    text_copy(out, size, text);
    size_t length = strlen(out);
    while (length && out[length - 1] == ' ') {
        out[--length] = '\0';
    }
}

// ---- menu

void page_menu_create(lv_obj_t *content)
{
    nav_button(content, STR_MENU_STATION, PAGE_STATION);
    nav_button(content, STR_MENU_WIFI, PAGE_WIFI);
    nav_button(content, STR_MENU_FILTER, PAGE_FILTER);
    nav_button(content, STR_MENU_DISPLAY, PAGE_DISPLAY);
    nav_button(content, STR_MENU_ADVANCED, PAGE_ADVANCED);
    nav_button(content, STR_MENU_HWTEST, PAGE_HWTEST);
    nav_button(content, STR_BACK, PAGE_CLOCK);
}

// ---- station and wizard

// Reads and validates one field into the working copy.
static bool take_callsign(lv_obj_t *input)
{
    char value[sizeof(s_settings.callsign)];
    input_value(input, value, sizeof(value));
    if (!settings_normalize_callsign(value)) {
        lv_label_set_text(s_error, STR_BAD_CALLSIGN);
        return false;
    }
    memcpy(s_settings.callsign, value, sizeof(value));
    return true;
}

static bool take_locator(lv_obj_t *input)
{
    char value[sizeof(s_settings.locator)];
    input_value(input, value, sizeof(value));
    if (!settings_normalize_locator(value)) {
        lv_label_set_text(s_error, STR_BAD_LOCATOR);
        return false;
    }
    memcpy(s_settings.locator, value, sizeof(value));
    return true;
}

static void save_station(lv_event_t *event)
{
    if (take_callsign(s_first) && take_locator(s_second) && save_and_apply()) {
        ui_show(PAGE_MENU);
    }
}

void page_station_create(lv_obj_t *content)
{
    settings_store_get(&s_settings);
    s_first = ui_input(content, STR_CALLSIGN, s_settings.callsign,
                       sizeof(s_settings.callsign) - 1, save_station);
    s_second = ui_input(content, STR_LOCATOR, s_settings.locator, sizeof(s_settings.locator) - 1,
                        save_station);
    s_error = ui_error_label(content);
    lv_obj_t *row = ui_row(content);
    ui_button(row, STR_SAVE, save_station, NULL);
    ui_button(row, STR_BACK, go, (void *)(uintptr_t)PAGE_MENU);
}

static void wizard_callsign_done(lv_event_t *event)
{
    // The locator step saves both, so keep the callsign until then.
    if (take_callsign(s_first)) {
        ui_show(PAGE_WIZARD_LOCATOR);
    }
}

void page_wizard_callsign_create(lv_obj_t *content)
{
    if (!ui_wizard()) {
        settings_store_get(&s_settings);
        ui_wizard_set(true);
    }
    ui_label(content, STR_WIZARD_CALLSIGN);
    s_first = ui_input(content, STR_CALLSIGN, s_settings.callsign,
                       sizeof(s_settings.callsign) - 1, wizard_callsign_done);
    s_error = ui_error_label(content);
    ui_button(content, STR_NEXT, wizard_callsign_done, NULL);
    // Reachable even if keyboard input does not work yet.
    ui_button(content, STR_MENU_HWTEST, go, (void *)(uintptr_t)PAGE_HWTEST);
}

static void wizard_locator_done(lv_event_t *event)
{
    if (take_locator(s_first) && save_and_apply()) {
        ui_show(PAGE_WIFI);
    }
}

void page_wizard_locator_create(lv_obj_t *content)
{
    ui_label(content, STR_WIZARD_LOCATOR);
    s_first = ui_input(content, STR_LOCATOR, s_settings.locator, sizeof(s_settings.locator) - 1,
                       wizard_locator_done);
    s_error = ui_error_label(content);
    lv_obj_t *row = ui_row(content);
    ui_button(row, STR_NEXT, wizard_locator_done, NULL);
    ui_button(row, STR_BACK, go, (void *)(uintptr_t)PAGE_WIZARD_CALLSIGN);
}

// ---- filter

static void save_filter(lv_event_t *event)
{
    char prefixes[sizeof(s_settings.prefixes)];
    input_value(s_first, prefixes, sizeof(prefixes));
    if (!settings_normalize_prefixes(prefixes)) {
        lv_label_set_text(s_error, STR_BAD_PREFIXES);
        return;
    }
    memcpy(s_settings.prefixes, prefixes, sizeof(prefixes));
    s_settings.band_mask = 0;
    for (int i = 0; i < SETTINGS_BAND_COUNT; i++) {
        if (lv_obj_has_state(s_bands[i], LV_STATE_CHECKED)) {
            s_settings.band_mask |= (uint16_t)(1u << i);
        }
    }
    s_settings.mode_mask = 0;
    for (int i = 0; i < SETTINGS_MODE_COUNT; i++) {
        if (lv_obj_has_state(s_modes[i], LV_STATE_CHECKED)) {
            s_settings.mode_mask |= (uint8_t)(1u << i);
        }
    }
    if (save_and_apply()) {
        ui_show(PAGE_MENU);
    }
}

void page_filter_create(lv_obj_t *content)
{
    settings_store_get(&s_settings);
    ui_label(content, STR_FILTER_BANDS);
    lv_obj_t *bands = ui_row(content);
    for (int i = 0; i < SETTINGS_BAND_COUNT; i++) {
        s_bands[i] =
            ui_checkbox(bands, settings_band_names[i], s_settings.band_mask & (1u << i));
    }
    ui_label(content, STR_FILTER_MODES);
    lv_obj_t *modes = ui_row(content);
    for (int i = 0; i < SETTINGS_MODE_COUNT; i++) {
        s_modes[i] =
            ui_checkbox(modes, settings_mode_names[i], s_settings.mode_mask & (1u << i));
    }
    s_first = ui_input(content, STR_FILTER_PREFIXES, s_settings.prefixes,
                       sizeof(s_settings.prefixes) - 1, save_filter);
    s_error = ui_error_label(content);
    lv_obj_t *row = ui_row(content);
    ui_button(row, STR_SAVE, save_filter, NULL);
    ui_button(row, STR_BACK, go, (void *)(uintptr_t)PAGE_MENU);
}

// ---- display and alarm

static void preview_brightness(lv_event_t *event)
{
    board_backlight_set((uint8_t)lv_slider_get_value(lv_event_get_target_obj(event)));
}

static void save_display(lv_event_t *event)
{
    s_settings.brightness = (uint8_t)lv_slider_get_value(s_first);
    s_settings.alarm = lv_obj_has_state(s_second, LV_STATE_CHECKED);
    if (save_and_apply()) {
        ui_show(PAGE_MENU);
    }
}

static void cancel_display(lv_event_t *event)
{
    settings_store_get(&s_settings);
    board_backlight_set(s_settings.brightness);
    ui_show(PAGE_MENU);
}

void page_display_create(lv_obj_t *content)
{
    settings_store_get(&s_settings);
    ui_label(content, STR_BRIGHTNESS);
    s_first = lv_slider_create(content);
    lv_slider_set_range(s_first, SETTINGS_MIN_BRIGHTNESS, 100);
    lv_slider_set_value(s_first, s_settings.brightness, LV_ANIM_OFF);
    lv_obj_set_width(s_first, LV_PCT(90));
    lv_obj_set_style_margin_all(s_first, 8, 0);
    lv_obj_add_event_cb(s_first, preview_brightness, LV_EVENT_VALUE_CHANGED, NULL);
    s_second = ui_checkbox(content, STR_ALARM, s_settings.alarm);
    s_error = ui_error_label(content);
    lv_obj_t *row = ui_row(content);
    ui_button(row, STR_SAVE, save_display, NULL);
    ui_button(row, STR_BACK, cancel_display, NULL);
}

// ---- advanced

static void save_advanced(lv_event_t *event)
{
    char timezone[sizeof(s_settings.timezone)];
    char broker[sizeof(s_settings.broker_uri)];
    input_value(s_first, timezone, sizeof(timezone));
    input_value(s_second, broker, sizeof(broker));
    if (!settings_valid_timezone(timezone)) {
        lv_label_set_text(s_error, STR_BAD_TIMEZONE);
        return;
    }
    if (!settings_valid_broker_uri(broker)) {
        lv_label_set_text(s_error, STR_BAD_BROKER);
        return;
    }
    memcpy(s_settings.timezone, timezone, sizeof(timezone));
    memcpy(s_settings.broker_uri, broker, sizeof(broker));
    if (save_and_apply()) {
        ui_show(PAGE_MENU);
    }
}

void page_advanced_create(lv_obj_t *content)
{
    settings_store_get(&s_settings);
    s_first = ui_input(content, STR_TIMEZONE, s_settings.timezone,
                       sizeof(s_settings.timezone) - 1, save_advanced);
    s_second = ui_input(content, STR_BROKER, s_settings.broker_uri,
                        sizeof(s_settings.broker_uri) - 1, save_advanced);
    s_error = ui_error_label(content);
    lv_obj_t *row = ui_row(content);
    ui_button(row, STR_SAVE, save_advanced, NULL);
    ui_button(row, STR_BACK, go, (void *)(uintptr_t)PAGE_MENU);
}
