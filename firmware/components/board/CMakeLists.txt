idf_component_register(
    SRCS "board.c" "board_display.c" "board_input.c" "board_power.c" "board_math.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES lvgl
    PRIV_REQUIRES esp_driver_gpio esp_driver_spi esp_driver_i2c esp_lcd esp_adc esp_timer)
