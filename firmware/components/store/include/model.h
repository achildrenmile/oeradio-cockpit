// One struct per topic, derived from schemas/v1. Pure C, also used by host tests.
// String buffers hold the character limits of the contract as UTF-8 (up to 4 bytes each).
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MODEL_SCHEMA_VERSION 1
#define MODEL_TOPIC_PREFIX "oeradio/cockpit/v1/"
#define MODEL_MAX_MESSAGE 8192

#define SOLAR_MAX_BANDS 8
#define POTA_MAX_SPOTS 20
#define DX_MAX_SPOTS 40
#define MESH_MAX_RECENT 10
#define MESH_MAX_REPEATERS 5
#define NEWS_MAX_ITEMS 8

typedef enum {
    TOPIC_STATUS,
    TOPIC_SOLAR,
    TOPIC_POTA,
    TOPIC_DX,
    TOPIC_MESH,
    TOPIC_NEWS,
    TOPIC_COUNT,
} topic_t;

typedef struct {
    int64_t ts;
    int64_t src_ts;  // 0 if absent
} msg_header_t;

typedef struct {
    msg_header_t head;
    bool offline;  // set by the last will of the aggregator
    char agg[16];
    bool ok_solar, ok_pota, ok_dx, ok_mesh, ok_news;
    char motd[321];
} status_msg_t;

typedef enum { COND_GOOD, COND_FAIR, COND_POOR } condition_t;

typedef struct {
    char band[16];
    condition_t day, night;
} band_condition_t;

typedef struct {
    msg_header_t head;
    int sfi;
    bool has_ssn, has_a, has_k, has_xray, has_wind;
    int ssn, a, k, wind;
    char xray[12];
    band_condition_t bands[SOLAR_MAX_BANDS];
    size_t band_count;
    bool aurora;
    char es_eu[32];
} solar_msg_t;

typedef struct {
    int64_t id, t;
    int32_t freq_khz;
    char activator[16], ref[16], park[161], loc[64], band[8], mode[12], spotter[16], comment[121];
} pota_spot_t;

typedef struct {
    msg_header_t head;
    pota_spot_t spots[POTA_MAX_SPOTS];
    size_t count;
} pota_msg_t;

typedef struct {
    int64_t t;
    int32_t freq_khz;
    char dx[16], band[8], mode[12], de[16], comment[121], dxcc[48];
} dx_spot_t;

typedef struct {
    msg_header_t head;
    dx_spot_t spots[DX_MAX_SPOTS];
    size_t count;
} dx_msg_t;

typedef struct {
    int64_t t;
    char from[64], channel[48], text[321];
} mesh_message_t;

typedef struct {
    char name[64];
    int64_t seen;
} mesh_repeater_t;

typedef struct {
    msg_header_t head;
    int nodes_online, repeaters_online;
    mesh_message_t recent[MESH_MAX_RECENT];
    size_t recent_count;
    mesh_repeater_t repeaters[MESH_MAX_REPEATERS];
    size_t repeater_count;
} mesh_msg_t;

typedef struct {
    int64_t t;
    char title[361], url[256];
} news_item_t;

typedef struct {
    msg_header_t head;
    news_item_t items[NEWS_MAX_ITEMS];
    size_t count;
} news_msg_t;

typedef union {
    status_msg_t status;
    solar_msg_t solar;
    pota_msg_t pota;
    dx_msg_t dx;
    mesh_msg_t mesh;
    news_msg_t news;
} topic_data_t;
