#include "settings.h"

#include <ctype.h>
#include <string.h>

#include "text.h"

const char *const settings_band_names[SETTINGS_BAND_COUNT] = {
    "160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m", "4m", "2m", "70cm"};
const char *const settings_mode_names[SETTINGS_MODE_COUNT] = {"CW",   "SSB", "FT8", "FT4",
                                                             "RTTY", "PSK", "FM"};

void settings_defaults(settings_t *settings)
{
    memset(settings, 0, sizeof(*settings));
    text_copy(settings->timezone, sizeof(settings->timezone), SETTINGS_DEFAULT_TZ);
    text_copy(settings->broker_uri, sizeof(settings->broker_uri), SETTINGS_DEFAULT_BROKER);
    settings->brightness = 100;
}

bool settings_complete(const settings_t *settings)
{
    return settings->callsign[0] && settings->locator[0];
}

bool settings_normalize_callsign(char *value)
{
    const size_t length = strlen(value);
    if (length < 3 || length > 12) {
        return false;
    }
    bool letter = false, digit = false;
    for (char *c = value; *c; c++) {
        *c = (char)toupper((unsigned char)*c);
        if (*c >= 'A' && *c <= 'Z') {
            letter = true;
        } else if (*c >= '0' && *c <= '9') {
            digit = true;
        } else if (*c != '/') {
            return false;
        }
    }
    return letter && digit && value[0] != '/' && value[length - 1] != '/';
}

bool settings_normalize_locator(char *value)
{
    if (strlen(value) != 6) {
        return false;
    }
    for (int i = 0; i < 6; i++) {
        const unsigned char c = (unsigned char)value[i];
        value[i] = (char)(i < 2 ? toupper(c) : tolower(c));
    }
    return value[0] >= 'A' && value[0] <= 'R' && value[1] >= 'A' && value[1] <= 'R' &&
           isdigit((unsigned char)value[2]) && isdigit((unsigned char)value[3]) &&
           value[4] >= 'a' && value[4] <= 'x' && value[5] >= 'a' && value[5] <= 'x';
}

bool settings_normalize_prefixes(char *value)
{
    char *write = value;
    for (const char *read = value; *read; read++) {
        const unsigned char c = (unsigned char)*read;
        if (c == ' ') {
            continue;
        }
        if (c == ',') {
            if (write > value && write[-1] != ',') {
                *write++ = ',';
            }
            continue;
        }
        if (!isalnum(c)) {
            return false;
        }
        *write++ = (char)toupper(c);
    }
    if (write > value && write[-1] == ',') {
        write--;
    }
    *write = '\0';
    return true;
}

bool settings_valid_wifi(const char *ssid, const char *password)
{
    const size_t name = strlen(ssid);
    const size_t secret = strlen(password);
    if (name < 1 || name > 32) {
        return false;
    }
    // WPA passphrase, or 64 hex digits as a raw key.
    if (secret == 64) {
        for (size_t i = 0; i < secret; i++) {
            if (!isxdigit((unsigned char)password[i])) {
                return false;
            }
        }
        return true;
    }
    return secret == 0 || (secret >= 8 && secret <= 63);
}

bool settings_valid_broker_uri(const char *uri)
{
    static const char *const schemes[] = {"wss://", "ws://", "mqtts://", "mqtt://"};
    if (strlen(uri) >= sizeof(((settings_t *)0)->broker_uri) || strchr(uri, ' ')) {
        return false;
    }
    for (size_t i = 0; i < sizeof(schemes) / sizeof(schemes[0]); i++) {
        const size_t length = strlen(schemes[i]);
        if (strncmp(uri, schemes[i], length) == 0) {
            return uri[length] != '\0' && uri[length] != '/';
        }
    }
    return false;
}

bool settings_valid_timezone(const char *tz)
{
    const size_t length = strlen(tz);
    return length >= 3 && length < sizeof(((settings_t *)0)->timezone) && !strchr(tz, ' ');
}

bool settings_wifi_put(settings_t *settings, const char *ssid, const char *password)
{
    if (!settings_valid_wifi(ssid, password)) {
        return false;
    }
    size_t index = 0;
    while (index < settings->wifi_count && strcmp(settings->wifi[index].ssid, ssid) != 0) {
        index++;
    }
    if (index == SETTINGS_WIFI_MAX) {
        return false;
    }
    if (index == settings->wifi_count) {
        settings->wifi_count++;
    }
    wifi_entry_t *entry = &settings->wifi[index];
    memset(entry, 0, sizeof(*entry));
    text_copy(entry->ssid, sizeof(entry->ssid), ssid);
    text_copy(entry->password, sizeof(entry->password), password);
    return true;
}

void settings_wifi_remove(settings_t *settings, size_t index)
{
    if (index >= settings->wifi_count) {
        return;
    }
    memmove(&settings->wifi[index], &settings->wifi[index + 1],
            (settings->wifi_count - index - 1) * sizeof(settings->wifi[0]));
    settings->wifi_count--;
    memset(&settings->wifi[settings->wifi_count], 0, sizeof(settings->wifi[0]));
}

void settings_wifi_raise(settings_t *settings, size_t index)
{
    if (index == 0 || index >= settings->wifi_count) {
        return;
    }
    const wifi_entry_t entry = settings->wifi[index];
    settings->wifi[index] = settings->wifi[index - 1];
    settings->wifi[index - 1] = entry;
}
