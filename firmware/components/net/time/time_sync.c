#include <stdatomic.h>
#include <stdlib.h>
#include <time.h>

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "net_internal.h"

static const char *TAG = "time";
static atomic_bool s_valid;

static void synced(struct timeval *tv)
{
    atomic_store(&s_valid, true);
    ESP_LOGI(TAG, "time synchronised: %lld", (long long)tv->tv_sec);
}

void net_time_start(void)
{
    // lwIP retries by itself, so this also works before Wi-Fi is up.
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(
        2, ESP_SNTP_SERVER_LIST("pool.ntp.org", "at.pool.ntp.org"));
    config.sync_cb = synced;
    if (esp_netif_sntp_init(&config) != ESP_OK) {
        ESP_LOGE(TAG, "SNTP init failed");
    }
}

void net_time_apply_timezone(const char *tz)
{
    setenv("TZ", tz, 1);
    tzset();
}

bool net_time_valid(void)
{
    return atomic_load(&s_valid);
}
