// Hardware free network decisions. Pure C, also used by host tests.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "settings.h"

#define BACKOFF_MIN_MS 2000
#define BACKOFF_MAX_MS 300000

// Delay before reconnect attempt number "attempt" (0 based): 2 s doubling up to 5 min,
// with a jitter of +-30 % taken from "random".
uint32_t backoff_ms(unsigned attempt, uint32_t random);

typedef struct {
    char ssid[33];
    int8_t rssi;
    bool secure;
} wifi_ap_t;

// Picks the saved network to connect to from a scan result. Signal strength and list
// position (priority) both count; weak networks are used only if nothing else is there.
// Bits in "skip_mask" exclude saved networks that just failed. Returns -1 if none fits.
int wifi_select(const wifi_entry_t *saved, size_t saved_count, const wifi_ap_t *aps,
                size_t ap_count, uint32_t skip_mask);

// Collects an MQTT message that arrives in several fragments.
#define ASSEMBLER_TOPIC_MAX 64

typedef struct {
    char topic[ASSEMBLER_TOPIC_MAX];
    size_t topic_length;
    char *buffer;  // provided by the caller
    size_t capacity, received, total;
    bool active;
} assembler_t;

typedef enum {
    ASSEMBLER_MORE,
    ASSEMBLER_COMPLETE,
    ASSEMBLER_TOO_LARGE,  // reported once per message, the other fragments are ignored
    ASSEMBLER_IGNORED,
} assembler_result_t;

void assembler_init(assembler_t *assembler, char *buffer, size_t capacity);
// "topic" is only given with the first fragment (offset 0).
assembler_result_t assembler_feed(assembler_t *assembler, const char *topic, size_t topic_length,
                                  const char *data, size_t length, size_t offset, size_t total);
