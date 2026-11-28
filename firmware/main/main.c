#include "board.h"
#include "esp_err.h"
#include "esp_log.h"
#include "ui/pages/page_hwtest.h"

void app_main(void)
{
    static board_t board;
    ESP_ERROR_CHECK(board_init(&board));

    if (board_lvgl_lock(0)) {
        page_hwtest_create(&board);
        board_lvgl_unlock();
    }
    ESP_LOGI("main", "touch=%d keyboard=%d battery=%dmV", board.touch != NULL,
             board.keypad != NULL, board_battery_mv());
}
