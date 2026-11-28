// Touch (GT911), keyboard (ESP32-C3 on I2C) and trackball (four pulse lines and a button).
#include <stdatomic.h>

#include "board_internal.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "pins.h"

#define I2C_TIMEOUT_MS 20
#define TRACKBALL_DEBOUNCE_US 1000
#define TRACKBALL_PULSES_PER_STEP 2
#define CLICK_DEBOUNCE_US 20000
#define KEY_BACKSPACE 0x08
#define KEY_ENTER 0x0D

static const char *TAG = "input";

// ---- touch

void board_touch_init(i2c_master_bus_handle_t bus, board_t *board)
{
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    // The address depends on the INT level at power-on, so ask both.
    if (i2c_master_probe(bus, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS, I2C_TIMEOUT_MS) != ESP_OK) {
        io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP;
        if (i2c_master_probe(bus, io_config.dev_addr, I2C_TIMEOUT_MS) != ESP_OK) {
            ESP_LOGW(TAG, "no GT911 touch controller");
            return;
        }
    }
    io_config.scl_speed_hz = 400000;

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_touch_handle_t touch = NULL;
    // Panel coordinates before the swap. This driver mirrors first and swaps afterwards,
    // so "mirror x" here equals LilyGO's "swap, then mirror y".
    const esp_lcd_touch_config_t config = {
        .x_max = 240,
        .y_max = 320,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = PIN_TOUCH_INT,
        .flags = {.swap_xy = 1, .mirror_x = 1, .mirror_y = 0},
    };
    if (esp_lcd_new_panel_io_i2c(bus, &io_config, &io) != ESP_OK ||
        esp_lcd_touch_new_i2c_gt911(io, &config, &touch) != ESP_OK) {
        ESP_LOGW(TAG, "touch init failed");
        return;
    }
    const lvgl_port_touch_cfg_t touch_config = {.disp = board->display, .handle = touch};
    board->touch = lvgl_port_add_touch(&touch_config);
}

// ---- keyboard

static i2c_master_dev_handle_t s_keyboard;
static atomic_uint s_last_key;

static void keypad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    static uint32_t key;
    static bool release_pending;

    data->key = key;
    data->state = LV_INDEV_STATE_RELEASED;
    // The keyboard reports characters, not key states: emit a release after every press.
    if (release_pending) {
        release_pending = false;
        return;
    }
    uint8_t value = 0;
    if (i2c_master_receive(s_keyboard, &value, 1, I2C_TIMEOUT_MS) != ESP_OK || value == 0) {
        return;
    }
    atomic_store(&s_last_key, value);
    key = value == KEY_ENTER ? LV_KEY_ENTER : value == KEY_BACKSPACE ? LV_KEY_BACKSPACE : value;
    release_pending = true;
    data->key = key;
    data->state = LV_INDEV_STATE_PRESSED;
}

void board_keyboard_init(i2c_master_bus_handle_t bus, board_t *board)
{
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = KEYBOARD_I2C_ADDR,
        .scl_speed_hz = 100000,
    };
    if (i2c_master_probe(bus, KEYBOARD_I2C_ADDR, I2C_TIMEOUT_MS) != ESP_OK ||
        i2c_master_bus_add_device(bus, &config, &s_keyboard) != ESP_OK) {
        ESP_LOGW(TAG, "no keyboard");
        s_keyboard = NULL;
        return;
    }
    if (lvgl_port_lock(0)) {
        board->keypad = lv_indev_create();
        lv_indev_set_type(board->keypad, LV_INDEV_TYPE_KEYPAD);
        lv_indev_set_read_cb(board->keypad, keypad_read);
        lvgl_port_unlock();
    }
}

esp_err_t board_keyboard_backlight_set(uint8_t duty)
{
    if (!s_keyboard) {
        return ESP_ERR_INVALID_STATE;
    }
    const uint8_t command[] = {KEYBOARD_CMD_BRIGHTNESS, duty};
    return i2c_master_transmit(s_keyboard, command, sizeof(command), I2C_TIMEOUT_MS);
}

uint32_t board_last_key(void)
{
    return atomic_load(&s_last_key);
}

// ---- trackball

enum { UP, DOWN, LEFT, RIGHT, DIRECTIONS };
static const int s_pins[DIRECTIONS] = {PIN_TRACKBALL_UP, PIN_TRACKBALL_DOWN, PIN_TRACKBALL_LEFT,
                                       PIN_TRACKBALL_RIGHT};
static atomic_uint s_pulses[DIRECTIONS];
static atomic_uint s_clicks;
static atomic_int s_encoder;

static void IRAM_ATTR trackball_isr(void *arg)
{
    static int64_t last_us[DIRECTIONS];
    const int direction = (int)(intptr_t)arg;
    const int64_t now = esp_timer_get_time();
    if (now - last_us[direction] < TRACKBALL_DEBOUNCE_US) {
        return;
    }
    last_us[direction] = now;
    atomic_fetch_add(&s_pulses[direction], 1);
    // Down and right move forward through the widgets, up and left backward.
    atomic_fetch_add(&s_encoder, direction == DOWN || direction == RIGHT ? 1 : -1);
}

static void encoder_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    static bool pressed, candidate;
    static int64_t changed_us;

    // Keep the remainder, so slow movements still add up.
    const int steps = atomic_load(&s_encoder) / TRACKBALL_PULSES_PER_STEP;
    atomic_fetch_sub(&s_encoder, steps * TRACKBALL_PULSES_PER_STEP);
    data->enc_diff = steps;

    const bool level = gpio_get_level(PIN_TRACKBALL_CLICK) == 0;
    const int64_t now = esp_timer_get_time();
    if (level != candidate) {
        candidate = level;
        changed_us = now;
    } else if (level != pressed && now - changed_us >= CLICK_DEBOUNCE_US) {
        pressed = level;
        if (pressed) {
            atomic_fetch_add(&s_clicks, 1);
        }
    }
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void board_trackball_init(board_t *board)
{
    gpio_config_t config = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    for (int i = 0; i < DIRECTIONS; i++) {
        config.pin_bit_mask |= 1ULL << s_pins[i];
    }
    ESP_ERROR_CHECK(gpio_config(&config));
    config.pin_bit_mask = 1ULL << PIN_TRACKBALL_CLICK;
    config.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&config));

    const esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
    for (int i = 0; i < DIRECTIONS; i++) {
        ESP_ERROR_CHECK(gpio_isr_handler_add(s_pins[i], trackball_isr, (void *)(intptr_t)i));
    }
    if (lvgl_port_lock(0)) {
        board->encoder = lv_indev_create();
        lv_indev_set_type(board->encoder, LV_INDEV_TYPE_ENCODER);
        lv_indev_set_read_cb(board->encoder, encoder_read);
        lvgl_port_unlock();
    }
}

void board_trackball_counts(board_trackball_counts_t *counts)
{
    *counts = (board_trackball_counts_t){
        .up = atomic_load(&s_pulses[UP]),
        .down = atomic_load(&s_pulses[DOWN]),
        .left = atomic_load(&s_pulses[LEFT]),
        .right = atomic_load(&s_pulses[RIGHT]),
        .click = atomic_load(&s_clicks),
    };
}
