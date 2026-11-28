idf_component_register(
    SRCS "main.c" "ui/ui.c" "ui/pages/page_clock.c" "ui/pages/page_settings.c"
         "ui/pages/page_wifi.c" "ui/pages/page_hwtest.c"
    INCLUDE_DIRS "." "ui"
    REQUIRES board net store nvs_flash)
