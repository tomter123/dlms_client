#pragma once

#include "esp_err.h"
#include "dlms_types.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t mqtt_publisher_init(void);
esp_err_t mqtt_publisher_start(void);
bool mqtt_publisher_is_connected(void);
esp_err_t mqtt_publish_readings(const dlms_reading_t *readings, uint8_t count);
esp_err_t mqtt_publish_ha_discovery(const dlms_obis_entry_t *entries, uint8_t count);

#ifdef __cplusplus
}
#endif
