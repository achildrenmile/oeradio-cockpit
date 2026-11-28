#include "net_logic.h"

#include <string.h>

#define WEAK_RSSI (-82)
#define PRIORITY_STEP_DB 10

uint32_t backoff_ms(unsigned attempt, uint32_t random)
{
    uint64_t base = BACKOFF_MIN_MS;
    while (attempt-- > 0 && base < BACKOFF_MAX_MS) {
        base *= 2;
    }
    if (base > BACKOFF_MAX_MS) {
        base = BACKOFF_MAX_MS;
    }
    return (uint32_t)(base * (700 + random % 601) / 1000);
}

int wifi_select(const wifi_entry_t *saved, size_t saved_count, const wifi_ap_t *aps,
                size_t ap_count, uint32_t skip_mask)
{
    int best = -1;
    int best_score = 0;
    for (size_t i = 0; i < saved_count; i++) {
        if (skip_mask & (1u << i)) {
            continue;
        }
        for (size_t a = 0; a < ap_count; a++) {
            if (strcmp(saved[i].ssid, aps[a].ssid) != 0) {
                continue;
            }
            // Never send a password to an open network that borrows a known name.
            if (saved[i].password[0] && !aps[a].secure) {
                continue;
            }
            int score = aps[a].rssi + (int)(saved_count - 1 - i) * PRIORITY_STEP_DB;
            if (aps[a].rssi < WEAK_RSSI) {
                score -= 1000;
            }
            if (best < 0 || score > best_score) {
                best = (int)i;
                best_score = score;
            }
        }
    }
    return best;
}

void assembler_init(assembler_t *assembler, char *buffer, size_t capacity)
{
    *assembler = (assembler_t){.buffer = buffer, .capacity = capacity};
}

assembler_result_t assembler_feed(assembler_t *assembler, const char *topic, size_t topic_length,
                                  const char *data, size_t length, size_t offset, size_t total)
{
    if (offset == 0) {
        assembler->active = false;
        if (total > assembler->capacity) {
            return ASSEMBLER_TOO_LARGE;
        }
        if (!topic || topic_length == 0 || topic_length > ASSEMBLER_TOPIC_MAX) {
            return ASSEMBLER_IGNORED;
        }
        memcpy(assembler->topic, topic, topic_length);
        assembler->topic_length = topic_length;
        assembler->total = total;
        assembler->received = 0;
        assembler->active = true;
    }
    if (!assembler->active || offset != assembler->received || total != assembler->total ||
        length > total - offset) {
        assembler->active = false;
        return ASSEMBLER_IGNORED;
    }
    memcpy(assembler->buffer + offset, data, length);
    assembler->received += length;
    if (assembler->received < assembler->total) {
        return ASSEMBLER_MORE;
    }
    assembler->active = false;
    return ASSEMBLER_COMPLETE;
}
