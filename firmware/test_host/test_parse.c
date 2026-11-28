// Parser tests. The valid payloads are the examples of docs/mqtt-topics.md.
// Usage: test_parse <docs/mqtt-topics.md>
//        test_parse --payloads <topic> <file> ...   (checks captured payloads)
#include <stdint.h>
#include <string.h>

#include "check.h"
#include "parse.h"
#include "text.h"

static topic_data_t s_out;
static char s_doc[64 * 1024];

static parse_result_t parse(topic_t topic, const char *json)
{
    return parse_message(topic, json, strlen(json), &s_out);
}

static size_t read_file(const char *path, char *buffer, size_t size)
{
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL);
    const size_t length = fread(buffer, 1, size - 1, file);
    CHECK(length < size - 1);
    fclose(file);
    buffer[length] = '\0';
    return length;
}

// The first ```json block below the heading of the topic.
static parse_result_t parse_example(topic_t topic)
{
    char heading[80];
    snprintf(heading, sizeof(heading), "### `%s%s`", MODEL_TOPIC_PREFIX, parse_topic_name(topic));
    const char *start = strstr(s_doc, heading);
    CHECK(start != NULL);
    start = strstr(start, "```json\n");
    CHECK(start != NULL);
    start += strlen("```json\n");
    const char *end = strstr(start, "```");
    CHECK(end != NULL);
    return parse_message(topic, start, (size_t)(end - start), &s_out);
}

static void test_documented_examples(void)
{
    CHECK(parse_example(TOPIC_STATUS) == PARSE_OK);
    CHECK(!s_out.status.offline && s_out.status.ok_solar && !s_out.status.ok_mesh);
    CHECK(strcmp(s_out.status.agg, "0.1.0") == 0);
    CHECK(strcmp(s_out.status.motd, "Willkommen bei oeradio.at") == 0);

    CHECK(parse_example(TOPIC_SOLAR) == PARSE_OK);
    CHECK(s_out.solar.sfi == 142 && s_out.solar.has_k && s_out.solar.k == 2);
    CHECK(s_out.solar.band_count == 4 && s_out.solar.bands[1].night == COND_FAIR);
    CHECK(!s_out.solar.aurora && strcmp(s_out.solar.es_eu, "closed") == 0);

    CHECK(parse_example(TOPIC_POTA) == PARSE_OK);
    CHECK(s_out.pota.count == 1 && s_out.pota.spots[0].freq_khz == 14285);
    CHECK(strcmp(s_out.pota.spots[0].loc, "AT-KA") == 0);

    CHECK(parse_example(TOPIC_DX) == PARSE_OK);
    CHECK(s_out.dx.count == 1 && strcmp(s_out.dx.spots[0].dx, "VP8LP") == 0);

    CHECK(parse_example(TOPIC_MESH) == PARSE_OK);
    CHECK(s_out.mesh.nodes_online == 37 && s_out.mesh.recent_count == 1);
    CHECK(strcmp(s_out.mesh.recent[0].from, "OE8YML-B\xc3\xbcro") == 0);
    CHECK(s_out.mesh.repeater_count == 1);

    CHECK(parse_example(TOPIC_NEWS) == PARSE_OK);
    CHECK(s_out.news.count == 1 && s_out.news.items[0].t == 1764200000);
}

static void test_topics(void)
{
    const char *dx = "oeradio/cockpit/v1/dx";
    CHECK(parse_topic(dx, strlen(dx)) == TOPIC_DX);
    CHECK(parse_topic(dx, strlen(dx) - 1) == TOPIC_COUNT);
    CHECK(parse_topic("oeradio/cockpit/v2/dx", 21) == TOPIC_COUNT);
    CHECK(parse_topic("oeradio/cockpit/v1/", 19) == TOPIC_COUNT);
}

static void test_rules(void)
{
    // Last will, unknown fields, null as absent.
    CHECK(parse(TOPIC_STATUS, "{\"v\":1,\"ts\":5,\"offline\":true,\"new\":[1]}") == PARSE_OK);
    CHECK(s_out.status.offline && s_out.status.head.ts == 5);
    CHECK(parse(TOPIC_STATUS, "{\"v\":1,\"ts\":5,\"motd\":null,\"ok\":null}") == PARSE_OK);

    CHECK(parse(TOPIC_STATUS, "{\"v\":2,\"ts\":5}") == PARSE_NEWER_VERSION);
    CHECK(parse(TOPIC_STATUS, "{\"v\":0,\"ts\":5}") == PARSE_INVALID);
    CHECK(parse(TOPIC_STATUS, "{\"ts\":5}") == PARSE_INVALID);
    CHECK(parse(TOPIC_STATUS, "{\"v\":1}") == PARSE_INVALID);
    CHECK(parse(TOPIC_STATUS, "{\"v\":1,\"ts\":\"5\"}") == PARSE_INVALID);
    CHECK(parse(TOPIC_STATUS, "{\"v\":1,\"ts\":1e400}") == PARSE_INVALID);
    CHECK(parse(TOPIC_STATUS, "{\"v\":1,\"ts\":5") == PARSE_INVALID);
    CHECK(parse(TOPIC_STATUS, "[1]") == PARSE_INVALID);
    CHECK(parse(TOPIC_STATUS, "") == PARSE_INVALID);

    // Optional solar values may be missing, required ones may not.
    CHECK(parse(TOPIC_SOLAR, "{\"v\":1,\"ts\":1,\"sfi\":100,\"bands\":[],"
                             "\"vhf\":{\"aurora\":true,\"es_eu\":\"open\"}}") == PARSE_OK);
    CHECK(!s_out.solar.has_ssn && !s_out.solar.has_xray && s_out.solar.aurora);
    CHECK(parse(TOPIC_SOLAR, "{\"v\":1,\"ts\":1,\"sfi\":100,\"bands\":[]}") == PARSE_INVALID);
    CHECK(parse(TOPIC_SOLAR, "{\"v\":1,\"ts\":1,\"sfi\":100,\"bands\":[{\"b\":\"x\",\"day\":"
                             "\"great\",\"night\":\"good\"}],\"vhf\":{\"aurora\":true,"
                             "\"es_eu\":\"x\"}}") == PARSE_INVALID);

    // A broken list entry discards the whole message.
    CHECK(parse(TOPIC_NEWS, "{\"v\":1,\"ts\":1,\"items\":[{\"t\":1,\"title\":\"a\"}]}") ==
          PARSE_INVALID);
    CHECK(parse(TOPIC_NEWS, "{\"v\":1,\"ts\":1,\"items\":[5]}") == PARSE_INVALID);
    CHECK(parse(TOPIC_NEWS, "{\"v\":1,\"ts\":1}") == PARSE_INVALID);
    CHECK(parse(TOPIC_NEWS, "{\"v\":1,\"ts\":1,\"items\":[]}") == PARSE_OK);

    // Deep nesting is refused before cJSON recurses into it.
    static char deep[4000];
    memset(deep, '[', sizeof(deep) - 1);
    CHECK(parse(TOPIC_NEWS, deep) == PARSE_INVALID);
    CHECK(parse(TOPIC_STATUS, "{\"v\":1,\"ts\":1,\"motd\":\"[[[[[[[[[[\\\"{{\"}") == PARSE_OK);
}

static void test_limits(void)
{
    // More entries than the array holds are cut off, not written past the end.
    static char json[MODEL_MAX_MESSAGE];
    size_t used = (size_t)snprintf(json, sizeof(json), "{\"v\":1,\"ts\":1,\"items\":[");
    for (int i = 0; i < NEWS_MAX_ITEMS + 5; i++) {
        used += (size_t)snprintf(json + used, sizeof(json) - used,
                                 "%s{\"t\":%d,\"title\":\"x\",\"url\":\"y\"}", i ? "," : "", i);
    }
    snprintf(json + used, sizeof(json) - used, "]}");
    CHECK(parse(TOPIC_NEWS, json) == PARSE_OK);
    CHECK(s_out.news.count == NEWS_MAX_ITEMS);

    // Long strings are cut at a character boundary.
    char small[4];
    text_copy(small, sizeof(small), "ab\xc3\xbc");
    CHECK(strcmp(small, "ab") == 0);
    text_copy(small, sizeof(small), "a\xc3\xbc");
    CHECK(strcmp(small, "a\xc3\xbc") == 0);

    static char big[MODEL_MAX_MESSAGE + 2];
    memset(big, ' ', sizeof(big) - 1);
    CHECK(parse(TOPIC_NEWS, big) == PARSE_INVALID);
}

int main(int argc, char **argv)
{
    if (argc >= 4 && strcmp(argv[1], "--payloads") == 0) {
        for (int i = 2; i + 1 < argc; i += 2) {
            const size_t length = read_file(argv[i + 1], s_doc, sizeof(s_doc));
            char topic[64];
            snprintf(topic, sizeof(topic), "%s%s", MODEL_TOPIC_PREFIX, argv[i]);
            const topic_t id = parse_topic(topic, strlen(topic));
            const parse_result_t result = parse_message(id, s_doc, length, &s_out);
            printf("%s: %zu bytes, result %d\n", argv[i], length, result);
            CHECK(result == PARSE_OK);
        }
        return 0;
    }
    CHECK(argc == 2);
    read_file(argv[1], s_doc, sizeof(s_doc));
    test_documented_examples();
    test_topics();
    test_rules();
    test_limits();
    puts("parse: ok");
    return 0;
}
