// Settings validation and network decisions.
#include <string.h>

#include "check.h"
#include "net_logic.h"
#include "settings.h"

static bool callsign(const char *in, const char *expected)
{
    char value[64];
    strcpy(value, in);
    return settings_normalize_callsign(value) && strcmp(value, expected) == 0;
}

static bool locator(const char *in, const char *expected)
{
    char value[64];
    strcpy(value, in);
    return settings_normalize_locator(value) && strcmp(value, expected) == 0;
}

static void test_validation(void)
{
    CHECK(callsign("oe8yml", "OE8YML"));
    CHECK(callsign("oe8yml/p", "OE8YML/P"));
    CHECK(!callsign("OE", "OE"));
    CHECK(!callsign("ABCDEF", "ABCDEF"));
    CHECK(!callsign("OE8 YML", "OE8 YML"));
    CHECK(!callsign("OE8YML/", "OE8YML/"));
    CHECK(!callsign("OE8YMLOE8YML1", ""));

    CHECK(locator("jn66UR", "JN66ur"));
    CHECK(!locator("JN66", "JN66"));
    CHECK(!locator("ZZ66ur", "ZZ66ur"));
    CHECK(!locator("JNxxur", "JNxxur"));
    CHECK(!locator("JN66yz", "JN66yz"));

    char prefixes[64] = " oe, dl ,,i, ";
    CHECK(settings_normalize_prefixes(prefixes) && strcmp(prefixes, "OE,DL,I") == 0);
    strcpy(prefixes, "OE;DL");
    CHECK(!settings_normalize_prefixes(prefixes));
    strcpy(prefixes, "");
    CHECK(settings_normalize_prefixes(prefixes) && prefixes[0] == '\0');

    CHECK(settings_valid_wifi("Shack", "12345678"));
    CHECK(settings_valid_wifi("Open", ""));
    CHECK(!settings_valid_wifi("", "12345678"));
    CHECK(!settings_valid_wifi("Shack", "short"));
    CHECK(settings_valid_wifi("Shack", "0123456789012345678901234567890123456789012345678901234567890123"));
    CHECK(!settings_valid_wifi("Shack", "z123456789012345678901234567890123456789012345678901234567890123"));

    CHECK(settings_valid_broker_uri(SETTINGS_DEFAULT_BROKER));
    CHECK(settings_valid_broker_uri("ws://192.168.1.10:9001"));
    CHECK(!settings_valid_broker_uri("https://example.org"));
    CHECK(!settings_valid_broker_uri("wss://"));
    CHECK(!settings_valid_broker_uri("ws://a b"));
    CHECK(settings_valid_timezone(SETTINGS_DEFAULT_TZ));
    CHECK(!settings_valid_timezone(""));
}

static void test_wifi_list(void)
{
    static settings_t settings;
    settings_defaults(&settings);
    CHECK(!settings_complete(&settings) && settings.brightness == 100);

    const char *names[] = {"a", "b", "c", "d", "e"};
    for (int i = 0; i < SETTINGS_WIFI_MAX; i++) {
        CHECK(settings_wifi_put(&settings, names[i], ""));
    }
    CHECK(!settings_wifi_put(&settings, "f", ""));
    CHECK(settings_wifi_put(&settings, "c", "newpassword"));  // known network: replace
    CHECK(settings.wifi_count == SETTINGS_WIFI_MAX);

    settings_wifi_raise(&settings, 2);
    CHECK(strcmp(settings.wifi[1].ssid, "c") == 0 && strcmp(settings.wifi[2].ssid, "b") == 0);
    settings_wifi_raise(&settings, 0);
    settings_wifi_remove(&settings, 0);
    CHECK(settings.wifi_count == 4 && strcmp(settings.wifi[0].ssid, "c") == 0);
    CHECK(settings.wifi[4].ssid[0] == '\0');
    settings_wifi_remove(&settings, 9);
    CHECK(settings.wifi_count == 4);
}

static void test_wifi_select(void)
{
    const wifi_entry_t saved[] = {{"shack", "password"}, {"phone", "password"}};
    wifi_ap_t aps[] = {{"shack", -70, true}, {"phone", -65, true}, {"other", -30, true}};

    CHECK(wifi_select(saved, 2, aps, 3, 0) == 0);  // priority beats 5 dB
    aps[0].rssi = -80;
    CHECK(wifi_select(saved, 2, aps, 3, 0) == 1);  // clearly stronger wins
    aps[0].rssi = -70;
    CHECK(wifi_select(saved, 2, aps, 3, 1u << 0) == 1);
    CHECK(wifi_select(saved, 2, aps, 3, 3) == -1);
    aps[0].rssi = -90;
    aps[1].rssi = -81;
    CHECK(wifi_select(saved, 2, aps, 3, 0) == 1);  // weak only as last resort
    CHECK(wifi_select(saved, 2, aps, 1, 0) == 0);
    aps[0].secure = false;  // open network with a known name
    CHECK(wifi_select(saved, 2, aps, 1, 0) == -1);
    CHECK(wifi_select(saved, 2, aps + 2, 1, 0) == -1);
}

static void test_backoff(void)
{
    // random % 601 == 300 is the middle of the jitter range
    CHECK(backoff_ms(0, 300) == 2000);
    CHECK(backoff_ms(1, 300) == 4000);
    CHECK(backoff_ms(7, 300) == 256000);
    CHECK(backoff_ms(8, 300) == 300000);
    CHECK(backoff_ms(100000, 300) == 300000);
    CHECK(backoff_ms(0, 0) == 1400);
    CHECK(backoff_ms(0, 600) == 2600);
    CHECK(backoff_ms(20, 600) == 390000);
}

static void test_assembler(void)
{
    static assembler_t assembler;
    char buffer[8];
    assembler_init(&assembler, buffer, sizeof(buffer));

    CHECK(assembler_feed(&assembler, "t", 1, "abc", 3, 0, 3) == ASSEMBLER_COMPLETE);
    CHECK(assembler_feed(&assembler, "t", 1, "abc", 3, 0, 8) == ASSEMBLER_MORE);
    CHECK(assembler_feed(&assembler, NULL, 0, "defgh", 5, 3, 8) == ASSEMBLER_COMPLETE);
    CHECK(memcmp(buffer, "abcdefgh", 8) == 0 && assembler.topic[0] == 't');

    CHECK(assembler_feed(&assembler, "t", 1, "abc", 3, 0, 9) == ASSEMBLER_TOO_LARGE);
    CHECK(assembler_feed(&assembler, NULL, 0, "def", 3, 3, 9) == ASSEMBLER_IGNORED);

    CHECK(assembler_feed(&assembler, "t", 1, "ab", 2, 0, 6) == ASSEMBLER_MORE);
    CHECK(assembler_feed(&assembler, NULL, 0, "ef", 2, 4, 6) == ASSEMBLER_IGNORED);  // gap
    CHECK(assembler_feed(&assembler, "t", 1, "ab", 2, 0, 4) == ASSEMBLER_MORE);
    CHECK(assembler_feed(&assembler, NULL, 0, "cdefg", 5, 2, 4) == ASSEMBLER_IGNORED);
}

int main(void)
{
    test_validation();
    test_wifi_list();
    test_wifi_select();
    test_backoff();
    test_assembler();
    puts("logic: ok");
    return 0;
}
