idf_component_register(
    SRCS "net.c" "net_logic.c" "wifi/wifi.c" "time/time_sync.c" "mqtt/mqtt.c"
    INCLUDE_DIRS "include"
    REQUIRES store
    PRIV_REQUIRES esp_wifi esp_netif esp_event esp_timer lwip mqtt mbedtls)
