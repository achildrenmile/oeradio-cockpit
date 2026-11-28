// Placeholder start page, the full clock page follows in phase 07.
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "net.h"
#include "pages.h"
#include "parse.h"
#include "settings_store.h"
#include "store.h"
#include "strings.h"
#include "ui.h"

static lv_obj_t *s_utc, *s_local, *s_data, *s_motd;
static uint32_t s_motd_version;

static void refresh(lv_timer_t *timer)
{
    net_status_t status;
    net_status_get(&status);
    if (status.time_valid) {
        const time_t now = time(NULL);
        struct tm utc, local;
        gmtime_r(&now, &utc);
        localtime_r(&now, &local);
        lv_label_set_text_fmt(s_utc, STR_CLOCK_UTC_FMT, utc.tm_hour, utc.tm_min, utc.tm_sec);
        lv_label_set_text_fmt(s_local, STR_CLOCK_LOCAL_FMT, local.tm_hour, local.tm_min,
                              local.tm_mday, local.tm_mon + 1, local.tm_year + 1900);
    } else {
        lv_label_set_text(s_utc, STR_BAR_NO_TIME);
        lv_label_set_text(s_local, STR_CLOCK_WAITING);
    }

    char topics[64] = "";
    for (int i = 0; i < TOPIC_COUNT; i++) {
        if (store_version((topic_t)i)) {
            const size_t used = strlen(topics);
            snprintf(topics + used, sizeof(topics) - used, "%s%s", used ? " " : "",
                     parse_topic_name((topic_t)i));
        }
    }
    lv_label_set_text_fmt(s_data, STR_CLOCK_DATA_FMT, topics[0] ? topics : STR_CLOCK_NO_DATA);

    const uint32_t version = store_version(TOPIC_STATUS);
    if (version != s_motd_version) {
        static status_msg_t message;
        if (store_get(TOPIC_STATUS, &message)) {
            lv_label_set_text(s_motd, message.motd);
        }
        s_motd_version = version;
    }
}

static void open_menu(lv_event_t *event)
{
    ui_show(PAGE_MENU);
}

void page_clock_create(lv_obj_t *content)
{
    static settings_t settings;
    settings_store_get(&settings);

    s_utc = ui_label(content, "");
    lv_obj_set_style_text_font(s_utc, &lv_font_montserrat_28, 0);
    s_local = ui_label(content, "");
    lv_obj_t *station = ui_label(content, "");
    lv_label_set_text_fmt(station, STR_CLOCK_STATION_FMT, settings.callsign, settings.locator);
    s_data = ui_label(content, "");
    s_motd = ui_label(content, "");
    s_motd_version = 0;
    ui_button(content, STR_SETTINGS, open_menu, NULL);
    ui_page_timer(refresh, 500);
}
