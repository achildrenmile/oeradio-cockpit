#include "store.h"

#include <stdatomic.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "parse.h"

static const char *TAG = "store";

static const size_t s_sizes[TOPIC_COUNT] = {
    sizeof(status_msg_t), sizeof(solar_msg_t), sizeof(pota_msg_t),
    sizeof(dx_msg_t),     sizeof(mesh_msg_t),  sizeof(news_msg_t),
};

static void *s_data[TOPIC_COUNT];
static topic_data_t *s_scratch;
static SemaphoreHandle_t s_mutex;
static atomic_uint s_versions[TOPIC_COUNT];
static atomic_bool s_server_offline;
static atomic_bool s_update_recommended;

esp_err_t store_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    s_scratch = heap_caps_malloc(sizeof(*s_scratch), MALLOC_CAP_SPIRAM);
    if (!s_mutex || !s_scratch) {
        return ESP_ERR_NO_MEM;
    }
    for (int i = 0; i < TOPIC_COUNT; i++) {
        s_data[i] = heap_caps_calloc(1, s_sizes[i], MALLOC_CAP_SPIRAM);
        if (!s_data[i]) {
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

static void log_summary(topic_t topic, const topic_data_t *data, size_t length)
{
    int count = -1;
    switch (topic) {
    case TOPIC_SOLAR:
        count = (int)data->solar.band_count;
        break;
    case TOPIC_POTA:
        count = (int)data->pota.count;
        break;
    case TOPIC_DX:
        count = (int)data->dx.count;
        break;
    case TOPIC_MESH:
        count = (int)data->mesh.recent_count;
        break;
    case TOPIC_NEWS:
        count = (int)data->news.count;
        break;
    default:
        break;
    }
    ESP_LOGI(TAG, "%s: %u bytes, ts=%lld, entries=%d", parse_topic_name(topic), (unsigned)length,
             (long long)data->status.head.ts, count);
}

void store_ingest(const char *topic_name, size_t topic_length, const char *payload, size_t length)
{
    const topic_t topic = parse_topic(topic_name, topic_length);
    if (topic == TOPIC_COUNT) {
        return;
    }
    switch (parse_message(topic, payload, length, s_scratch)) {
    case PARSE_NEWER_VERSION:
        atomic_store(&s_update_recommended, true);
        ESP_LOGW(TAG, "%s: newer schema version, firmware update recommended",
                 parse_topic_name(topic));
        return;
    case PARSE_INVALID:
        ESP_LOGW(TAG, "%s: invalid message dropped", parse_topic_name(topic));
        return;
    case PARSE_OK:
        break;
    }
    if (topic == TOPIC_STATUS) {
        atomic_store(&s_server_offline, s_scratch->status.offline);
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memcpy(s_data[topic], s_scratch, s_sizes[topic]);
    atomic_fetch_add(&s_versions[topic], 1);
    xSemaphoreGive(s_mutex);
    log_summary(topic, s_scratch, length);
}

uint32_t store_version(topic_t topic)
{
    return atomic_load(&s_versions[topic]);
}

bool store_get(topic_t topic, void *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const bool present = atomic_load(&s_versions[topic]) > 0;
    if (present) {
        memcpy(out, s_data[topic], s_sizes[topic]);
    }
    xSemaphoreGive(s_mutex);
    return present;
}

bool store_server_offline(void)
{
    return atomic_load(&s_server_offline);
}

bool store_update_recommended(void)
{
    return atomic_load(&s_update_recommended);
}
