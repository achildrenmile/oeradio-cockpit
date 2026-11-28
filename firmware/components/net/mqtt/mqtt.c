// Read only MQTT client with its own reconnect schedule.
// One task controls the client; the esp-mqtt event handler only reports to it.
#include <stdatomic.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "net_internal.h"
#include "settings_store.h"
#include "store.h"

#define KEEPALIVE_S 45  // below the 100 s WebSocket idle timeout of Cloudflare
#define STABLE_US (60 * 1000000LL)

typedef enum { CMD_NET_UP, CMD_NET_DOWN, CMD_SETTINGS, CMD_CONNECTED, CMD_DISCONNECTED } command_t;

static const char *TAG = "mqtt";

static QueueHandle_t s_queue;
static assembler_t s_assembler;
static char s_client_id[32];
static atomic_bool s_connected;
static _Atomic int64_t s_retry_at_us;  // 0 if no attempt is scheduled

// Control task state
static esp_mqtt_client_handle_t s_client;
static char s_uri[sizeof(((settings_t *)0)->broker_uri)];
static bool s_network;
static bool s_started;
static unsigned s_attempt;
static int64_t s_connected_since_us;

static void post(command_t command)
{
    if (xQueueSend(s_queue, &command, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "command queue full");
    }
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    esp_mqtt_event_handle_t event = data;
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "connected");
        esp_mqtt_client_subscribe_single(event->client, MODEL_TOPIC_PREFIX "#", 1);
        post(CMD_CONNECTED);
        break;
    case MQTT_EVENT_DISCONNECTED:
        post(CMD_DISCONNECTED);
        break;
    case MQTT_EVENT_DATA:
        switch (assembler_feed(&s_assembler, event->topic, event->topic_len, event->data,
                               event->data_len, event->current_data_offset,
                               event->total_data_len)) {
        case ASSEMBLER_COMPLETE:
            store_ingest(s_assembler.topic, s_assembler.topic_length, s_assembler.buffer,
                         s_assembler.total);
            break;
        case ASSEMBLER_TOO_LARGE:
            ESP_LOGW(TAG, "message of %d bytes dropped (limit %d)", event->total_data_len,
                     MODEL_MAX_MESSAGE);
            break;
        default:
            break;
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "error type %d", event->error_handle->error_type);
        break;
    default:
        break;
    }
}

static void destroy_client(void)
{
    if (!s_client) {
        return;
    }
    if (s_started) {
        esp_mqtt_client_stop(s_client);
        s_started = false;
    }
    esp_mqtt_client_destroy(s_client);
    s_client = NULL;
    atomic_store(&s_connected, false);
    atomic_store(&s_retry_at_us, 0);
}

static void create_client(void)
{
    const esp_mqtt_client_config_t config = {
        .broker.address.uri = s_uri,
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
        .credentials.client_id = s_client_id,
        .session.keepalive = KEEPALIVE_S,
        .network.disable_auto_reconnect = true,
        .network.timeout_ms = 15000,
        .buffer.size = 2048,  // larger messages arrive in fragments
        .task.stack_size = 8192,
    };
    s_client = esp_mqtt_client_init(&config);
    if (!s_client ||
        esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, on_event, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "client init failed for %s", s_uri);
        destroy_client();
    }
}

static void schedule_retry(void)
{
    const uint32_t delay = backoff_ms(s_attempt++, esp_random());
    ESP_LOGI(TAG, "reconnect in %u ms (attempt %u)", (unsigned)delay, s_attempt);
    atomic_store(&s_retry_at_us, esp_timer_get_time() + delay * 1000LL);
}

static void connect_now(void)
{
    atomic_store(&s_retry_at_us, 0);
    if (atomic_load(&s_connected)) {
        return;
    }
    if (!s_client) {
        create_client();
    }
    if (!s_client) {
        return;
    }
    if (!s_started) {
        s_started = esp_mqtt_client_start(s_client) == ESP_OK;
        if (!s_started) {
            schedule_retry();
        }
    } else if (esp_mqtt_client_reconnect(s_client) != ESP_OK) {
        schedule_retry();  // the client had not finished closing
    }
}

static void handle(command_t command)
{
    static settings_t settings;
    switch (command) {
    case CMD_NET_UP:
        s_network = true;
        s_attempt = 0;
        // Spread the first attempt a little, many devices may return at once.
        atomic_store(&s_retry_at_us, esp_timer_get_time() + (esp_random() % 2000) * 1000LL);
        break;
    case CMD_NET_DOWN:
        s_network = false;
        atomic_store(&s_retry_at_us, 0);
        if (atomic_load(&s_connected)) {
            esp_mqtt_client_disconnect(s_client);  // do not wait for the keepalive
        }
        break;
    case CMD_SETTINGS:
        settings_store_get(&settings);
        if (strcmp(settings.broker_uri, s_uri) != 0) {
            strlcpy(s_uri, settings.broker_uri, sizeof(s_uri));
            destroy_client();
            s_attempt = 0;
            if (s_network) {
                connect_now();
            }
        }
        break;
    case CMD_CONNECTED:
        atomic_store(&s_connected, true);
        atomic_store(&s_retry_at_us, 0);
        s_connected_since_us = esp_timer_get_time();
        break;
    case CMD_DISCONNECTED:
        // Reset the backoff only after a stable connection.
        if (atomic_exchange(&s_connected, false) &&
            esp_timer_get_time() - s_connected_since_us > STABLE_US) {
            s_attempt = 0;
        }
        ESP_LOGW(TAG, "disconnected");
        if (s_network) {
            schedule_retry();
        }
        break;
    }
}

static void control_task(void *arg)
{
    while (true) {
        const int64_t retry_at = atomic_load(&s_retry_at_us);
        TickType_t wait = portMAX_DELAY;
        if (retry_at) {
            const int64_t left = retry_at - esp_timer_get_time();
            wait = left > 0 ? pdMS_TO_TICKS(left / 1000) + 1 : 0;
        }
        command_t command;
        if (xQueueReceive(s_queue, &command, wait) == pdTRUE) {
            handle(command);
        } else if (s_network && retry_at && esp_timer_get_time() >= retry_at) {
            connect_now();
        }
    }
}

esp_err_t net_mqtt_start(void)
{
    s_queue = xQueueCreate(8, sizeof(command_t));
    char *buffer = heap_caps_malloc(MODEL_MAX_MESSAGE, MALLOC_CAP_SPIRAM);
    if (!s_queue || !buffer) {
        return ESP_ERR_NO_MEM;
    }
    assembler_init(&s_assembler, buffer, MODEL_MAX_MESSAGE);

    static settings_t settings;
    settings_store_get(&settings);
    strlcpy(s_uri, settings.broker_uri, sizeof(s_uri));

    // Client IDs are broker wide: the random part keeps others from guessing the ID
    // and disconnecting this device.
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_client_id, sizeof(s_client_id), "oec-%02x%02x%02x-%08x", mac[3], mac[4], mac[5],
             (unsigned)esp_random());

    return xTaskCreate(control_task, "mqtt_ctl", 4096, NULL, 5, NULL) == pdPASS ? ESP_OK
                                                                                : ESP_ERR_NO_MEM;
}

void net_mqtt_network_up(void)
{
    post(CMD_NET_UP);
}

void net_mqtt_network_down(void)
{
    post(CMD_NET_DOWN);
}

void net_mqtt_settings_changed(void)
{
    post(CMD_SETTINGS);
}

void net_mqtt_status(net_status_t *out)
{
    out->mqtt_connected = atomic_load(&s_connected);
    const int64_t retry_at = atomic_load(&s_retry_at_us);
    if (retry_at) {
        const int64_t left = retry_at - esp_timer_get_time();
        out->mqtt_retry_in_s = left > 0 ? (int)(left / 1000000) + 1 : 0;
    }
}
