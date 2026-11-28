// Interfaces between the parts of this component.
#pragma once

#include "net.h"

esp_err_t net_wifi_start(void);
void net_wifi_settings_changed(void);
void net_wifi_status(net_status_t *out);
void net_wifi_scan_request(void);

void net_time_start(void);
void net_time_apply_timezone(const char *tz);
bool net_time_valid(void);

esp_err_t net_mqtt_start(void);
void net_mqtt_network_up(void);
void net_mqtt_network_down(void);
void net_mqtt_settings_changed(void);
void net_mqtt_status(net_status_t *out);
