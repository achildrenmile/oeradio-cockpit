idf_component_register(
    SRCS "text.c" "parse.c" "settings.c" "settings_store.c" "store.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash
    PRIV_REQUIRES json esp_timer)
