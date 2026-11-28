// One task owns the radio: it scans, picks the best saved network and reconnects.
#include <stdatomic.h>
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "net_internal.h"
#include "settings_store.h"
#include "text.h"

#define BIT_GOT_IP BIT0
#define BIT_LOST BIT1
#define BIT_SCAN BIT2
#define BIT_CHANGED BIT3

#define CONNECT_TIMEOUT_MS 20000
#define SEARCH_MIN_MS 3000
#define SEARCH_MAX_MS 30000

static const char *TAG = "wifi";

static EventGroupHandle_t s_events;
static SemaphoreHandle_t s_mutex;  // guards everything below
static wifi_state_t s_state = WIFI_STATE_SEARCHING;
static wifi_entry_t s_current;
static wifi_ap_t s_aps[NET_SCAN_MAX];
static size_t s_ap_count;
static uint32_t s_generation;

static void set_state(wifi_state_t state)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state = state;
    xSemaphoreGive(s_mutex);
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_events, BIT_GOT_IP);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *event = data;
        ESP_LOGW(TAG, "disconnected, reason %d", event->reason);
        xEventGroupSetBits(s_events, BIT_LOST);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_LOST_IP) {
        xEventGroupSetBits(s_events, BIT_LOST);
    }
}

// Blocking scan, publishes the result for the settings page.
static void scan(void)
{
    static wifi_ap_record_t records[NET_SCAN_MAX];
    uint16_t count = NET_SCAN_MAX;
    if (esp_wifi_scan_start(NULL, true) != ESP_OK ||
        esp_wifi_scan_get_ap_records(&count, records) != ESP_OK) {
        count = 0;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_ap_count = 0;
    for (int i = 0; i < count; i++) {
        const char *ssid = (const char *)records[i].ssid;
        if (!ssid[0]) {
            continue;  // hidden networks are not supported
        }
        wifi_ap_t *ap = &s_aps[s_ap_count++];
        text_copy(ap->ssid, sizeof(ap->ssid), ssid);
        ap->rssi = records[i].rssi;
        ap->secure = records[i].authmode != WIFI_AUTH_OPEN;
    }
    s_generation++;
    xSemaphoreGive(s_mutex);
}

static bool connect_to(const wifi_entry_t *entry)
{
    wifi_config_t config = {0};
    // Both fields may use their full length without a terminator.
    memcpy(config.sta.ssid, entry->ssid, strnlen(entry->ssid, sizeof(config.sta.ssid)));
    memcpy(config.sta.password, entry->password,
           strnlen(entry->password, sizeof(config.sta.password)));
    // No downgrade to open or WEP for a network that has a password.
    config.sta.threshold.authmode = entry->password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    config.sta.pmf_cfg.capable = true;
    config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    xEventGroupClearBits(s_events, BIT_GOT_IP | BIT_LOST);
    if (esp_wifi_set_config(WIFI_IF_STA, &config) != ESP_OK || esp_wifi_connect() != ESP_OK) {
        return false;
    }
    const EventBits_t bits = xEventGroupWaitBits(s_events, BIT_GOT_IP | BIT_LOST, pdFALSE, pdFALSE,
                                                 pdMS_TO_TICKS(CONNECT_TIMEOUT_MS));
    if (bits & BIT_GOT_IP) {
        return true;
    }
    esp_wifi_disconnect();
    return false;
}

static bool still_saved(const settings_t *settings, const wifi_entry_t *entry)
{
    for (int i = 0; i < settings->wifi_count; i++) {
        if (strcmp(settings->wifi[i].ssid, entry->ssid) == 0 &&
            strcmp(settings->wifi[i].password, entry->password) == 0) {
            return true;
        }
    }
    return false;
}

// Returns when the connection is lost or the network was removed from the settings.
static void stay_connected(settings_t *settings)
{
    set_state(WIFI_STATE_CONNECTED);
    ESP_LOGI(TAG, "connected to \"%s\"", s_current.ssid);
    net_mqtt_network_up();
    while (true) {
        const EventBits_t bits = xEventGroupWaitBits(s_events, BIT_LOST | BIT_SCAN | BIT_CHANGED,
                                                     pdFALSE, pdFALSE, portMAX_DELAY);
        if (bits & BIT_LOST) {
            break;
        }
        if (bits & BIT_SCAN) {
            xEventGroupClearBits(s_events, BIT_SCAN);
            scan();
        }
        if (bits & BIT_CHANGED) {
            xEventGroupClearBits(s_events, BIT_CHANGED);
            settings_store_get(settings);
            if (!still_saved(settings, &s_current)) {
                esp_wifi_disconnect();
                break;
            }
        }
    }
    net_mqtt_network_down();
}

static void wifi_task(void *arg)
{
    static settings_t settings;
    uint32_t failed = 0;
    uint32_t delay_ms = SEARCH_MIN_MS;

    while (true) {
        xEventGroupClearBits(s_events, BIT_SCAN | BIT_CHANGED);
        settings_store_get(&settings);
        set_state(settings.wifi_count ? WIFI_STATE_SEARCHING : WIFI_STATE_NONE_SAVED);
        if (settings.wifi_count) {
            scan();
            // Only this task writes the scan result, so reading it needs no lock.
            int choice =
                wifi_select(settings.wifi, settings.wifi_count, s_aps, s_ap_count, failed);
            if (choice < 0 && failed) {
                failed = 0;  // every candidate failed once, start over
                choice = wifi_select(settings.wifi, settings.wifi_count, s_aps, s_ap_count, 0);
            }
            if (choice >= 0) {
                xSemaphoreTake(s_mutex, portMAX_DELAY);
                s_current = settings.wifi[choice];
                s_state = WIFI_STATE_CONNECTING;
                xSemaphoreGive(s_mutex);
                ESP_LOGI(TAG, "connecting to \"%s\"", s_current.ssid);
                if (connect_to(&s_current)) {
                    failed = 0;
                    delay_ms = SEARCH_MIN_MS;
                    stay_connected(&settings);
                    continue;
                }
                ESP_LOGW(TAG, "could not connect to \"%s\"", s_current.ssid);
                failed |= 1u << choice;
                set_state(WIFI_STATE_SEARCHING);
            }
        }
        const EventBits_t bits =
            xEventGroupWaitBits(s_events, BIT_SCAN | BIT_CHANGED, pdFALSE, pdFALSE,
                                settings.wifi_count ? pdMS_TO_TICKS(delay_ms) : portMAX_DELAY);
        if (bits & BIT_CHANGED) {
            failed = 0;
            delay_ms = SEARCH_MIN_MS;
        } else if (bits & BIT_SCAN) {
            if (!settings.wifi_count) {
                scan();  // otherwise the next round scans anyway
            }
        } else {
            delay_ms = delay_ms * 2 > SEARCH_MAX_MS ? SEARCH_MAX_MS : delay_ms * 2;
        }
    }
}

esp_err_t net_wifi_start(void)
{
    s_events = xEventGroupCreate();
    s_mutex = xSemaphoreCreateMutex();
    if (!s_events || !s_mutex) {
        return ESP_ERR_NO_MEM;
    }
    esp_netif_create_default_wifi_sta();
    const wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&config), TAG, "init");
    // The settings store is the only place for credentials.
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "storage");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL), TAG, "events");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_event, NULL), TAG, "events");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "mode");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start");
    return xTaskCreate(wifi_task, "wifi", 4096, NULL, 4, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void net_wifi_settings_changed(void)
{
    xEventGroupSetBits(s_events, BIT_CHANGED);
}

void net_wifi_scan_request(void)
{
    xEventGroupSetBits(s_events, BIT_SCAN);
}

void net_wifi_status(net_status_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    out->wifi = s_state;
    if (s_state == WIFI_STATE_CONNECTED || s_state == WIFI_STATE_CONNECTING) {
        text_copy(out->ssid, sizeof(out->ssid), s_current.ssid);
    }
    xSemaphoreGive(s_mutex);
    wifi_ap_record_t ap;
    if (out->wifi == WIFI_STATE_CONNECTED && esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        out->rssi = ap.rssi;
    }
}

size_t net_scan_results(wifi_ap_t *out, size_t max, uint32_t *generation)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const size_t count = s_ap_count < max ? s_ap_count : max;
    memcpy(out, s_aps, count * sizeof(*out));
    *generation = s_generation;
    xSemaphoreGive(s_mutex);
    return count;
}
