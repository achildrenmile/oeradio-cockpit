#include "parse.h"

#include <math.h>
#include <string.h>

#include "cJSON.h"
#include "text.h"

// The contract nests three levels deep. cJSON recurses, so refuse deeper input early.
#define MAX_DEPTH 8

static const char *const s_names[TOPIC_COUNT] = {"status", "solar", "pota", "dx", "mesh", "news"};

topic_t parse_topic(const char *topic, size_t length)
{
    const size_t prefix = strlen(MODEL_TOPIC_PREFIX);
    if (length <= prefix || memcmp(topic, MODEL_TOPIC_PREFIX, prefix) != 0) {
        return TOPIC_COUNT;
    }
    for (int i = 0; i < TOPIC_COUNT; i++) {
        if (strlen(s_names[i]) == length - prefix &&
            memcmp(topic + prefix, s_names[i], length - prefix) == 0) {
            return (topic_t)i;
        }
    }
    return TOPIC_COUNT;
}

const char *parse_topic_name(topic_t topic)
{
    return topic < TOPIC_COUNT ? s_names[topic] : "?";
}

static bool depth_ok(const char *json, size_t length)
{
    int depth = 0;
    bool in_string = false;
    for (size_t i = 0; i < length; i++) {
        const char c = json[i];
        if (in_string) {
            if (c == '\\') {
                i++;
            } else if (c == '"') {
                in_string = false;
            }
        } else if (c == '"') {
            in_string = true;
        } else if (c == '{' || c == '[') {
            if (++depth > MAX_DEPTH) {
                return false;
            }
        } else if (c == '}' || c == ']') {
            depth--;
        }
    }
    return true;
}

// ---- field helpers: "req" clears *ok if the field is missing, all clear it on a wrong type.

static const cJSON *field(const cJSON *object, const char *name)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsNull(item) ? NULL : item;
}

static bool opt_int(const cJSON *object, const char *name, int64_t *out, bool *ok)
{
    const cJSON *item = field(object, name);
    if (!item) {
        return false;
    }
    // Limit to the range a double holds exactly.
    if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble) || fabs(item->valuedouble) > 9e15) {
        *ok = false;
        return false;
    }
    *out = (int64_t)item->valuedouble;
    return true;
}

static int64_t req_int(const cJSON *object, const char *name, bool *ok)
{
    int64_t value = 0;
    if (!opt_int(object, name, &value, ok)) {
        *ok = false;
    }
    return value;
}

static int32_t req_int32(const cJSON *object, const char *name, bool *ok)
{
    const int64_t value = req_int(object, name, ok);
    if (value < INT32_MIN || value > INT32_MAX) {
        *ok = false;
        return 0;
    }
    return (int32_t)value;
}

static bool opt_small(const cJSON *object, const char *name, int *out, bool *ok)
{
    int64_t value = 0;
    if (!opt_int(object, name, &value, ok)) {
        return false;
    }
    if (value < INT32_MIN || value > INT32_MAX) {
        *ok = false;
        return false;
    }
    *out = (int)value;
    return true;
}

static bool opt_str(const cJSON *object, const char *name, char *dst, size_t size, bool *ok)
{
    const cJSON *item = field(object, name);
    if (!item) {
        return false;
    }
    if (!cJSON_IsString(item)) {
        *ok = false;
        return false;
    }
    text_copy(dst, size, item->valuestring);
    return true;
}

static void req_str(const cJSON *object, const char *name, char *dst, size_t size, bool *ok)
{
    if (!opt_str(object, name, dst, size, ok)) {
        *ok = false;
    }
}

#define REQ_STR(object, name, dst, ok) req_str(object, name, dst, sizeof(dst), ok)
#define OPT_STR(object, name, dst, ok) opt_str(object, name, dst, sizeof(dst), ok)

static bool opt_bool(const cJSON *object, const char *name, bool *out, bool *ok)
{
    const cJSON *item = field(object, name);
    if (!item) {
        return false;
    }
    if (!cJSON_IsBool(item)) {
        *ok = false;
        return false;
    }
    *out = cJSON_IsTrue(item);
    return true;
}

static const cJSON *req_array(const cJSON *object, const char *name, bool *ok)
{
    const cJSON *item = field(object, name);
    if (!cJSON_IsArray(item)) {
        *ok = false;
        return NULL;
    }
    return item;
}

// Runs the body for each array element, up to the capacity of the target array.
#define FOR_ITEMS(item, array, count, max, ok)                                      \
    for (const cJSON *item = (array) ? (array)->child : NULL;                        \
         item && (count) < (max) && (cJSON_IsObject(item) || (*(ok) = false)); \
         item = item->next, (count)++)

// ---- topics

static void parse_status(const cJSON *root, status_msg_t *out, bool *ok)
{
    opt_bool(root, "offline", &out->offline, ok);
    OPT_STR(root, "agg", out->agg, ok);
    OPT_STR(root, "motd", out->motd, ok);
    const cJSON *sources = field(root, "ok");
    if (sources) {
        if (!cJSON_IsObject(sources)) {
            *ok = false;
            return;
        }
        opt_bool(sources, "solar", &out->ok_solar, ok);
        opt_bool(sources, "pota", &out->ok_pota, ok);
        opt_bool(sources, "dx", &out->ok_dx, ok);
        opt_bool(sources, "mesh", &out->ok_mesh, ok);
        opt_bool(sources, "news", &out->ok_news, ok);
    }
}

static condition_t req_condition(const cJSON *object, const char *name, bool *ok)
{
    char value[8] = "";
    REQ_STR(object, name, value, ok);
    if (strcmp(value, "good") == 0) {
        return COND_GOOD;
    }
    if (strcmp(value, "fair") == 0) {
        return COND_FAIR;
    }
    if (strcmp(value, "poor") != 0) {
        *ok = false;
    }
    return COND_POOR;
}

static void parse_solar(const cJSON *root, solar_msg_t *out, bool *ok)
{
    out->sfi = req_int32(root, "sfi", ok);
    out->has_ssn = opt_small(root, "ssn", &out->ssn, ok);
    out->has_a = opt_small(root, "a", &out->a, ok);
    out->has_k = opt_small(root, "k", &out->k, ok);
    out->has_wind = opt_small(root, "wind", &out->wind, ok);
    out->has_xray = OPT_STR(root, "xray", out->xray, ok);

    const cJSON *bands = req_array(root, "bands", ok);
    FOR_ITEMS(item, bands, out->band_count, SOLAR_MAX_BANDS, ok)
    {
        band_condition_t *band = &out->bands[out->band_count];
        REQ_STR(item, "b", band->band, ok);
        band->day = req_condition(item, "day", ok);
        band->night = req_condition(item, "night", ok);
    }

    const cJSON *vhf = field(root, "vhf");
    if (!cJSON_IsObject(vhf)) {
        *ok = false;
        return;
    }
    if (!opt_bool(vhf, "aurora", &out->aurora, ok)) {
        *ok = false;
    }
    REQ_STR(vhf, "es_eu", out->es_eu, ok);
}

static void parse_pota(const cJSON *root, pota_msg_t *out, bool *ok)
{
    const cJSON *spots = req_array(root, "spots", ok);
    FOR_ITEMS(item, spots, out->count, POTA_MAX_SPOTS, ok)
    {
        pota_spot_t *spot = &out->spots[out->count];
        spot->id = req_int(item, "id", ok);
        spot->t = req_int(item, "t", ok);
        spot->freq_khz = req_int32(item, "f", ok);
        REQ_STR(item, "act", spot->activator, ok);
        REQ_STR(item, "ref", spot->ref, ok);
        REQ_STR(item, "b", spot->band, ok);
        REQ_STR(item, "sp", spot->spotter, ok);
        OPT_STR(item, "park", spot->park, ok);
        OPT_STR(item, "loc", spot->loc, ok);
        OPT_STR(item, "m", spot->mode, ok);
        OPT_STR(item, "cmt", spot->comment, ok);
    }
}

static void parse_dx(const cJSON *root, dx_msg_t *out, bool *ok)
{
    const cJSON *spots = req_array(root, "spots", ok);
    FOR_ITEMS(item, spots, out->count, DX_MAX_SPOTS, ok)
    {
        dx_spot_t *spot = &out->spots[out->count];
        spot->t = req_int(item, "t", ok);
        spot->freq_khz = req_int32(item, "f", ok);
        REQ_STR(item, "dx", spot->dx, ok);
        REQ_STR(item, "b", spot->band, ok);
        REQ_STR(item, "de", spot->de, ok);
        OPT_STR(item, "m", spot->mode, ok);
        OPT_STR(item, "cmt", spot->comment, ok);
        OPT_STR(item, "dxcc", spot->dxcc, ok);
    }
}

static void parse_mesh(const cJSON *root, mesh_msg_t *out, bool *ok)
{
    out->nodes_online = req_int32(root, "nodes_online", ok);
    out->repeaters_online = req_int32(root, "repeaters_online", ok);
    const cJSON *recent = req_array(root, "recent", ok);
    FOR_ITEMS(item, recent, out->recent_count, MESH_MAX_RECENT, ok)
    {
        mesh_message_t *message = &out->recent[out->recent_count];
        message->t = req_int(item, "t", ok);
        REQ_STR(item, "from", message->from, ok);
        REQ_STR(item, "ch", message->channel, ok);
        REQ_STR(item, "txt", message->text, ok);
    }
    const cJSON *repeaters = req_array(root, "top_repeaters", ok);
    FOR_ITEMS(item, repeaters, out->repeater_count, MESH_MAX_REPEATERS, ok)
    {
        mesh_repeater_t *repeater = &out->repeaters[out->repeater_count];
        REQ_STR(item, "name", repeater->name, ok);
        repeater->seen = req_int(item, "seen", ok);
    }
}

static void parse_news(const cJSON *root, news_msg_t *out, bool *ok)
{
    const cJSON *items = req_array(root, "items", ok);
    FOR_ITEMS(item, items, out->count, NEWS_MAX_ITEMS, ok)
    {
        news_item_t *news = &out->items[out->count];
        news->t = req_int(item, "t", ok);
        REQ_STR(item, "title", news->title, ok);
        REQ_STR(item, "url", news->url, ok);
    }
}

parse_result_t parse_message(topic_t topic, const char *json, size_t length, topic_data_t *out)
{
    if (topic >= TOPIC_COUNT || length > MODEL_MAX_MESSAGE || !depth_ok(json, length)) {
        return PARSE_INVALID;
    }
    cJSON *root = cJSON_ParseWithLength(json, length);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return PARSE_INVALID;
    }
    memset(out, 0, sizeof(*out));

    bool ok = true;
    const int64_t version = req_int(root, "v", &ok);
    if (ok && version > MODEL_SCHEMA_VERSION) {
        cJSON_Delete(root);
        return PARSE_NEWER_VERSION;
    }
    ok = ok && version == MODEL_SCHEMA_VERSION;

    // Every struct starts with the header, so any member can be used to reach it.
    msg_header_t *head = &out->status.head;
    head->ts = req_int(root, "ts", &ok);
    opt_int(root, "src_ts", &head->src_ts, &ok);

    switch (topic) {
    case TOPIC_STATUS:
        parse_status(root, &out->status, &ok);
        break;
    case TOPIC_SOLAR:
        parse_solar(root, &out->solar, &ok);
        break;
    case TOPIC_POTA:
        parse_pota(root, &out->pota, &ok);
        break;
    case TOPIC_DX:
        parse_dx(root, &out->dx, &ok);
        break;
    case TOPIC_MESH:
        parse_mesh(root, &out->mesh, &ok);
        break;
    case TOPIC_NEWS:
        parse_news(root, &out->news, &ok);
        break;
    default:
        ok = false;
    }
    cJSON_Delete(root);
    return ok ? PARSE_OK : PARSE_INVALID;
}
