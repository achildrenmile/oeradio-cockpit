// Wi-Fi, time and MQTT. All functions are thread safe.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "net_logic.h"

#define NET_SCAN_MAX 20

typedef enum { WIFI_STATE_NONE_SAVED, WIFI_STATE_SEARCHING, WIFI_STATE_CONNECTING,
               WIFI_STATE_CONNECTED } wifi_state_t;

typedef struct {
    wifi_state_t wifi;
    char ssid[33];
    int rssi;
    bool mqtt_connected;
    int mqtt_retry_in_s;  // seconds until the next attempt, 0 if none is scheduled
    bool time_valid;
} net_status_t;

// Starts all network tasks. Needs the stores to be initialised.
esp_err_t net_start(void);
void net_status_get(net_status_t *out);
// Call after saving settings: applies time zone, Wi-Fi list and broker.
void net_settings_changed(void);

void net_scan_request(void);
// Copies the latest scan result. "generation" increments with every finished scan.
size_t net_scan_results(wifi_ap_t *out, size_t max, uint32_t *generation);
