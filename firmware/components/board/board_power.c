// Display backlight and battery voltage.
//
// The backlight driver is not PWM dimmable: it has 16 steps selected by short pulses.
// Sequence as in setBrightness() of LilyGO's UnitTest example (MIT).
#include "board_internal.h"
#include "board_math.h"
#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pins.h"

#define ADC_SAMPLES 8

static const char *TAG = "power";
static portMUX_TYPE s_pulse_lock = portMUX_INITIALIZER_UNLOCKED;
static int s_step;  // 0 = off

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_calibration;
static adc_channel_t s_channel;

void board_backlight_init(void)
{
    gpio_reset_pin(PIN_TFT_BACKLIGHT);
    gpio_set_direction(PIN_TFT_BACKLIGHT, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_TFT_BACKLIGHT, 0);
    vTaskDelay(pdMS_TO_TICKS(5));  // low for more than 3 ms resets the driver
    s_step = 0;
}

void board_backlight_set(uint8_t percent)
{
    const int step = board_math_backlight_step(percent);
    if (step == 0) {
        gpio_set_level(PIN_TFT_BACKLIGHT, 0);
        vTaskDelay(pdMS_TO_TICKS(5));
        s_step = 0;
        return;
    }
    // Interrupts off: a pulse that lasts too long would switch the driver off.
    portENTER_CRITICAL(&s_pulse_lock);
    if (s_step == 0) {
        gpio_set_level(PIN_TFT_BACKLIGHT, 1);  // starts at full brightness
        esp_rom_delay_us(30);
        s_step = BACKLIGHT_STEPS;
    }
    for (int i = board_math_backlight_pulses(s_step, step); i > 0; i--) {
        gpio_set_level(PIN_TFT_BACKLIGHT, 0);
        esp_rom_delay_us(1);
        gpio_set_level(PIN_TFT_BACKLIGHT, 1);
        esp_rom_delay_us(1);
    }
    s_step = step;
    portEXIT_CRITICAL(&s_pulse_lock);
}

void board_battery_init(void)
{
    adc_unit_t unit;
    if (adc_oneshot_io_to_channel(PIN_BATTERY_ADC, &unit, &s_channel) != ESP_OK) {
        return;
    }
    const adc_oneshot_unit_init_cfg_t unit_config = {.unit_id = unit};
    const adc_oneshot_chan_cfg_t channel_config = {.atten = ADC_ATTEN_DB_12,
                                                   .bitwidth = ADC_BITWIDTH_DEFAULT};
    const adc_cali_curve_fitting_config_t calibration_config = {
        .unit_id = unit,
        .chan = s_channel,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_oneshot_new_unit(&unit_config, &s_adc) != ESP_OK ||
        adc_oneshot_config_channel(s_adc, s_channel, &channel_config) != ESP_OK ||
        adc_cali_create_scheme_curve_fitting(&calibration_config, &s_calibration) != ESP_OK) {
        ESP_LOGW(TAG, "battery ADC unavailable");
        s_calibration = NULL;
    }
}

int board_battery_mv(void)
{
    if (!s_calibration) {
        return 0;
    }
    int sum = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        int raw = 0, millivolts = 0;
        if (adc_oneshot_read(s_adc, s_channel, &raw) != ESP_OK ||
            adc_cali_raw_to_voltage(s_calibration, raw, &millivolts) != ESP_OK) {
            return 0;
        }
        sum += millivolts;
    }
    return sum / ADC_SAMPLES * BATTERY_DIVIDER;
}

int board_battery_percent(int millivolts)
{
    return board_math_battery_percent(millivolts);
}
