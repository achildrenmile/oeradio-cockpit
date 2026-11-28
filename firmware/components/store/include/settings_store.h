// Settings persisted in NVS, shared between the UI and the network tasks.
#pragma once

#include "esp_err.h"
#include "settings.h"

// Needs nvs_flash_init(). Missing or invalid values fall back to the defaults.
esp_err_t settings_store_init(void);
void settings_store_get(settings_t *out);
esp_err_t settings_store_save(const settings_t *settings);
