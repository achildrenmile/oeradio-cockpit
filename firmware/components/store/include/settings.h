// Device settings: data, defaults and validation. Pure C, also used by host tests.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SETTINGS_WIFI_MAX 5
#define SETTINGS_DEFAULT_TZ "CET-1CEST,M3.5.0,M10.5.0/3"
// Mosquitto accepts any WebSocket path; "/mqtt" is the usual one and what ws-smoke.py uses.
#define SETTINGS_DEFAULT_BROKER "wss://mqtt.oeradio.at/mqtt"
#define SETTINGS_MIN_BRIGHTNESS 5

#define SETTINGS_BAND_COUNT 14
#define SETTINGS_MODE_COUNT 7
extern const char *const settings_band_names[SETTINGS_BAND_COUNT];
extern const char *const settings_mode_names[SETTINGS_MODE_COUNT];

typedef struct {
    char ssid[33];
    char password[65];  // empty for open networks
} wifi_entry_t;

typedef struct {
    char callsign[13];
    char locator[7];
    char timezone[48];
    char broker_uri[128];
    wifi_entry_t wifi[SETTINGS_WIFI_MAX];  // order is priority, first is highest
    uint8_t wifi_count;
    uint16_t band_mask;  // bit per settings_band_names, 0 shows all
    uint8_t mode_mask;   // bit per settings_mode_names, 0 shows all
    char prefixes[64];   // country prefixes for POTA and DX, e.g. "OE,DL,I", empty shows all
    uint8_t brightness;  // percent
    bool alarm;
} settings_t;

void settings_defaults(settings_t *settings);
// True once the first run wizard has collected callsign and locator.
bool settings_complete(const settings_t *settings);

// The normalize functions rewrite the value in place and return false if it is invalid.
bool settings_normalize_callsign(char *value);  // upper case
bool settings_normalize_locator(char *value);   // "JN66ur"
bool settings_normalize_prefixes(char *value);  // "OE,DL", spaces removed
bool settings_valid_wifi(const char *ssid, const char *password);
bool settings_valid_broker_uri(const char *uri);
bool settings_valid_timezone(const char *tz);

// Replaces the password of a known SSID, else appends. False if invalid or the list is full.
bool settings_wifi_put(settings_t *settings, const char *ssid, const char *password);
void settings_wifi_remove(settings_t *settings, size_t index);
void settings_wifi_raise(settings_t *settings, size_t index);  // one step up in priority
