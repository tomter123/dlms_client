#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

esp_err_t wifi_manager_init(void);
bool wifi_manager_is_connected(void);
int8_t wifi_manager_get_rssi(void);
esp_err_t wifi_manager_get_ip_str(char *buf, size_t len);
EventGroupHandle_t wifi_manager_get_event_group(void);

#ifdef __cplusplus
}
#endif
