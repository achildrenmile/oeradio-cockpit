#include "board.h"
#include "esp_err.h"
#include "esp_log.h"
#include "net.h"
#include "nvs_flash.h"
#include "settings_store.h"
#include "store.h"
#include "ui.h"

static void nvs_start(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void app_main(void)
{
    static board_t board;
    static settings_t settings;

    nvs_start();
    ESP_ERROR_CHECK(settings_store_init());
    ESP_ERROR_CHECK(store_init());
    ESP_ERROR_CHECK(board_init(&board));
    settings_store_get(&settings);
    board_backlight_set(settings.brightness);
    ESP_ERROR_CHECK(net_start());

    if (board_lvgl_lock(0)) {
        ui_start(&board);
        ui_show(settings_complete(&settings) ? PAGE_CLOCK : PAGE_WIZARD_CALLSIGN);
        board_lvgl_unlock();
    }
    ESP_LOGI("main", "touch=%d keyboard=%d battery=%dmV", board.touch != NULL,
             board.keypad != NULL, board_battery_mv());
}
