// Thread safe holder of the latest message per topic.
#pragma once

#include "esp_err.h"
#include "model.h"

esp_err_t store_init(void);

// Parses and stores a received message. Called from the MQTT task only.
void store_ingest(const char *topic, size_t topic_length, const char *payload, size_t length);

// Increments with every stored message, 0 means nothing received yet.
// The UI redraws only when it changes.
uint32_t store_version(topic_t topic);

// Copies the struct of the topic (e.g. dx_msg_t). Returns false if nothing arrived yet.
bool store_get(topic_t topic, void *out);

bool store_server_offline(void);
// True once a message with a newer schema version was seen.
bool store_update_recommended(void);
