#include "board_internal.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pins.h"

static const char *TAG = "board";

static void output_high(int pin)
{
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 1);
}

static i2c_master_bus_handle_t i2c_init(void)
{
    const i2c_master_bus_config_t config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    if (i2c_new_master_bus(&config, &bus) != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed");
    }
    return bus;
}

esp_err_t board_init(board_t *board)
{
    *board = (board_t){0};

    output_high(PIN_POWERON);
    // Deselect everything on the shared SPI bus before it is used.
    output_high(PIN_SDCARD_CS);
    output_high(PIN_RADIO_CS);
    output_high(PIN_TFT_CS);
    // The keyboard has its own controller, which needs time to boot.
    vTaskDelay(pdMS_TO_TICKS(500));

    board_backlight_init();
    ESP_RETURN_ON_ERROR(board_display_init(&board->display), TAG, "display");

    i2c_master_bus_handle_t bus = i2c_init();
    if (bus) {
        board_touch_init(bus, board);
        board_keyboard_init(bus, board);
    }
    board_trackball_init(board);
    board_battery_init();

    board_backlight_set(100);
    return ESP_OK;
}

bool board_lvgl_lock(uint32_t timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void board_lvgl_unlock(void)
{
    lvgl_port_unlock();
}
