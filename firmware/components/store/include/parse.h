// JSON payload -> model structs. Pure C on top of cJSON.
#pragma once

#include "model.h"

typedef enum {
    PARSE_OK,
    PARSE_INVALID,        // broken JSON, missing required field or wrong type
    PARSE_NEWER_VERSION,  // "v" is higher than this firmware supports
} parse_result_t;

// Returns TOPIC_COUNT for unknown topics. The topic need not be null terminated.
topic_t parse_topic(const char *topic, size_t length);
const char *parse_topic_name(topic_t topic);

// Fills the union member that belongs to the topic. Unknown fields are ignored.
// On any result other than PARSE_OK the output is not usable.
parse_result_t parse_message(topic_t topic, const char *json, size_t length, topic_data_t *out);
