#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "web_server.h"

static const char *TAG = "web_server";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

static const dlms_reading_t *s_readings = NULL;
static uint8_t *s_readings_count = NULL;
static SemaphoreHandle_t s_readings_mutex = NULL;

static char s_dlms_state[32] = "Unknown";
static int8_t s_wifi_rssi = 0;
static bool s_mqtt_connected = false;
static SemaphoreHandle_t s_status_mutex = NULL;

void web_server_set_readings_source(const dlms_reading_t *readings, uint8_t *count, SemaphoreHandle_t mutex)
{
    s_readings = readings;
    s_readings_count = count;
    s_readings_mutex = mutex;
}

void web_server_set_status_info(const char *dlms_state, int8_t wifi_rssi, bool mqtt_connected)
{
    if (s_status_mutex) {
        xSemaphoreTake(s_status_mutex, portMAX_DELAY);
        if (dlms_state) {
            strncpy(s_dlms_state, dlms_state, sizeof(s_dlms_state) - 1);
            s_dlms_state[sizeof(s_dlms_state) - 1] = '\0';
        }
        s_wifi_rssi = wifi_rssi;
        s_mqtt_connected = mqtt_connected;
        xSemaphoreGive(s_status_mutex);
    }
}

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    const size_t len = index_html_end - index_html_start;
    return httpd_resp_send(req, (const char *)index_html_start, len);
}

static esp_err_t api_readings_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    cJSON *root = cJSON_CreateArray();

    if (s_readings && s_readings_count && s_readings_mutex) {
        if (xSemaphoreTake(s_readings_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            for (uint8_t i = 0; i < *s_readings_count; i++) {
                if (s_readings[i].valid) {
                    cJSON *item = cJSON_CreateObject();
                    cJSON_AddStringToObject(item, "obis", s_readings[i].obis_str);
                    cJSON_AddStringToObject(item, "name", s_readings[i].name);
                    cJSON_AddNumberToObject(item, "value", s_readings[i].scaled_value);
                    
                    const char *unit_str = "";
                    switch (s_readings[i].unit) {
                        case DLMS_UNIT_ACTIVE_POWER_W: unit_str = "W"; break;
                        case DLMS_UNIT_ACTIVE_ENERGY: unit_str = "Wh"; break;
                        case DLMS_UNIT_VOLTAGE: unit_str = "V"; break;
                        case DLMS_UNIT_CURRENT: unit_str = "A"; break;
                        case DLMS_UNIT_FREQUENCY: unit_str = "Hz"; break;
                        case DLMS_UNIT_APPARENT_POWER: unit_str = "VA"; break;
                        case DLMS_UNIT_REACTIVE_POWER: unit_str = "var"; break;
                        case DLMS_UNIT_APPARENT_ENERGY: unit_str = "VAh"; break;
                        case DLMS_UNIT_REACTIVE_ENERGY: unit_str = "varh"; break;
                        default: unit_str = ""; break;
                    }
                    cJSON_AddStringToObject(item, "unit", unit_str);
                    cJSON_AddNumberToObject(item, "timestamp", s_readings[i].timestamp_ms);
                    
                    cJSON_AddItemToArray(root, item);
                }
            }
            xSemaphoreGive(s_readings_mutex);
        }
    }

    const char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
    
    free((void *)json_str);
    cJSON_Delete(root);
    
    return ESP_OK;
}

static esp_err_t api_status_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    cJSON *root = cJSON_CreateObject();
    
    int64_t uptime_sec = esp_timer_get_time() / 1000000;
    cJSON_AddNumberToObject(root, "uptime_sec", uptime_sec);
    
    if (s_status_mutex) {
        xSemaphoreTake(s_status_mutex, portMAX_DELAY);
        cJSON_AddStringToObject(root, "dlms_state", s_dlms_state);
        cJSON_AddNumberToObject(root, "wifi_rssi", s_wifi_rssi);
        cJSON_AddBoolToObject(root, "mqtt_connected", s_mqtt_connected);
        xSemaphoreGive(s_status_mutex);
    }
    
    cJSON_AddNumberToObject(root, "free_heap", esp_get_free_heap_size());

    const char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
    
    free((void *)json_str);
    cJSON_Delete(root);
    
    return ESP_OK;
}

esp_err_t web_server_init(void)
{
    if (s_status_mutex == NULL) {
        s_status_mutex = xSemaphoreCreateMutex();
    }

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_LOGI(TAG, "Starting web server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t index_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = index_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &index_uri);

        httpd_uri_t readings_uri = {
            .uri       = "/api/v1/meter/readings",
            .method    = HTTP_GET,
            .handler   = api_readings_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &readings_uri);

        httpd_uri_t status_uri = {
            .uri       = "/api/v1/meter/status",
            .method    = HTTP_GET,
            .handler   = api_status_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &status_uri);

        return ESP_OK;
    }

    ESP_LOGE(TAG, "Error starting web server!");
    return ESP_FAIL;
}
