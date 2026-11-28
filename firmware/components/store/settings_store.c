#include "settings_store.h"

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

#define NAMESPACE "cockpit"

static const char *TAG = "settings";
static settings_t s_settings;
static SemaphoreHandle_t s_mutex;

static void load_string(nvs_handle_t nvs, const char *key, char *dst, size_t size)
{
    size_t length = 0;
    // Keep the default if the key is missing or does not fit.
    if (nvs_get_str(nvs, key, NULL, &length) == ESP_OK && length <= size) {
        nvs_get_str(nvs, key, dst, &length);
    }
}

#define LOAD_STRING(nvs, key, dst) load_string(nvs, key, dst, sizeof(dst))

static void load(nvs_handle_t nvs, settings_t *out)
{
    LOAD_STRING(nvs, "call", out->callsign);
    LOAD_STRING(nvs, "locator", out->locator);
    LOAD_STRING(nvs, "tz", out->timezone);
    LOAD_STRING(nvs, "broker", out->broker_uri);
    LOAD_STRING(nvs, "prefixes", out->prefixes);

    size_t size = sizeof(out->wifi);
    if (nvs_get_blob(nvs, "wifi", out->wifi, &size) == ESP_OK && size % sizeof(out->wifi[0]) == 0) {
        out->wifi_count = (uint8_t)(size / sizeof(out->wifi[0]));
        for (int i = 0; i < out->wifi_count; i++) {
            out->wifi[i].ssid[sizeof(out->wifi[i].ssid) - 1] = '\0';
            out->wifi[i].password[sizeof(out->wifi[i].password) - 1] = '\0';
        }
    } else {
        memset(out->wifi, 0, sizeof(out->wifi));
    }

    uint8_t byte = 0;
    nvs_get_u16(nvs, "bands", &out->band_mask);
    nvs_get_u8(nvs, "modes", &out->mode_mask);
    if (nvs_get_u8(nvs, "bright", &byte) == ESP_OK && byte >= SETTINGS_MIN_BRIGHTNESS &&
        byte <= 100) {
        out->brightness = byte;
    }
    if (nvs_get_u8(nvs, "alarm", &byte) == ESP_OK) {
        out->alarm = byte != 0;
    }
}

esp_err_t settings_store_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        return ESP_ERR_NO_MEM;
    }
    settings_defaults(&s_settings);
    nvs_handle_t nvs;
    const esp_err_t err = nvs_open(NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;  // first start
    }
    ESP_RETURN_ON_ERROR(err, TAG, "open");
    load(nvs, &s_settings);
    nvs_close(nvs);
    return ESP_OK;
}

void settings_store_get(settings_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_settings;
    xSemaphoreGive(s_mutex);
}

static esp_err_t write_all(nvs_handle_t nvs, const settings_t *in)
{
    ESP_RETURN_ON_ERROR(nvs_set_str(nvs, "call", in->callsign), TAG, "call");
    ESP_RETURN_ON_ERROR(nvs_set_str(nvs, "locator", in->locator), TAG, "locator");
    ESP_RETURN_ON_ERROR(nvs_set_str(nvs, "tz", in->timezone), TAG, "tz");
    ESP_RETURN_ON_ERROR(nvs_set_str(nvs, "broker", in->broker_uri), TAG, "broker");
    ESP_RETURN_ON_ERROR(nvs_set_str(nvs, "prefixes", in->prefixes), TAG, "prefixes");
    if (in->wifi_count) {
        ESP_RETURN_ON_ERROR(
            nvs_set_blob(nvs, "wifi", in->wifi, in->wifi_count * sizeof(in->wifi[0])), TAG, "wifi");
    } else {
        const esp_err_t err = nvs_erase_key(nvs, "wifi");
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            return err;
        }
    }
    ESP_RETURN_ON_ERROR(nvs_set_u16(nvs, "bands", in->band_mask), TAG, "bands");
    ESP_RETURN_ON_ERROR(nvs_set_u8(nvs, "modes", in->mode_mask), TAG, "modes");
    ESP_RETURN_ON_ERROR(nvs_set_u8(nvs, "bright", in->brightness), TAG, "bright");
    ESP_RETURN_ON_ERROR(nvs_set_u8(nvs, "alarm", in->alarm), TAG, "alarm");
    return nvs_commit(nvs);
}

esp_err_t settings_store_save(const settings_t *settings)
{
    nvs_handle_t nvs;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_settings = *settings;
    esp_err_t err = nvs_open(NAMESPACE, NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        err = write_all(nvs, settings);
        nvs_close(nvs);
    }
    xSemaphoreGive(s_mutex);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
    }
    return err;
}
