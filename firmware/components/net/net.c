#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "net_internal.h"
#include "settings_store.h"

static const char *TAG = "net";

static void apply_timezone(void)
{
    static settings_t settings;
    settings_store_get(&settings);
    net_time_apply_timezone(settings.timezone);
}

esp_err_t net_start(void)
{
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop");
    apply_timezone();
    net_time_start();
    ESP_RETURN_ON_ERROR(net_mqtt_start(), TAG, "mqtt");
    return net_wifi_start();
}

void net_status_get(net_status_t *out)
{
    memset(out, 0, sizeof(*out));
    net_wifi_status(out);
    net_mqtt_status(out);
    out->time_valid = net_time_valid();
}

void net_settings_changed(void)
{
    apply_timezone();
    net_wifi_settings_changed();
    net_mqtt_settings_changed();
}

void net_scan_request(void)
{
    net_wifi_scan_request();
}
