#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "dlms_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and start the HTTP server
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t web_server_init(void);

/**
 * @brief Set the source data for meter readings API
 * 
 * @param readings Pointer to shared readings array
 * @param count Pointer to number of valid readings
 * @param mutex Mutex to protect access to the readings array
 */
void web_server_set_readings_source(const dlms_reading_t *readings, uint8_t *count, SemaphoreHandle_t mutex);

/**
 * @brief Update the internal status information served on /api/v1/meter/status
 * 
 * @param dlms_state String describing current DLMS connection state
 * @param wifi_rssi Current Wi-Fi RSSI in dBm
 * @param mqtt_connected True if connected to MQTT broker
 */
void web_server_set_status_info(const char *dlms_state, int8_t wifi_rssi, bool mqtt_connected);

#ifdef __cplusplus
}
#endif
