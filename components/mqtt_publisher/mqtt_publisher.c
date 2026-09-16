#include "mqtt_publisher.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_timer.h"
#include <string.h>

static const char *TAG = "mqtt_publisher";

static esp_mqtt_client_handle_t s_client = NULL;
static bool s_is_connected = false;
static char s_topic_prefix[64] = "dlms/meter";

static char s_mqtt_tpl[1024] = {0};
extern dlms_reading_t get_cached_reading(const char *obis); // We will define this in main.c


static void replace_dots(char *str) {
    for (char *p = str; *p; p++) {
        if (*p == '.') *p = '_';
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    (void)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected");
            s_is_connected = true;
            // publish online status
            char topic[128];
            snprintf(topic, sizeof(topic), "%s/status", s_topic_prefix);
            esp_mqtt_client_publish(s_client, topic, "online", 0, 1, 1);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected");
            s_is_connected = false;
            break;
        default:
            break;
    }
}

esp_err_t mqtt_publisher_init(const char *broker_uri, const char *username, const char *password, const char *topic_prefix) {
    if (topic_prefix && strlen(topic_prefix) > 0) {
        strncpy(s_topic_prefix, topic_prefix, sizeof(s_topic_prefix)-1);
    }

    
    nvs_handle_t nvs;
    if (nvs_open("config", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(s_mqtt_tpl);
        if (nvs_get_str(nvs, "mqtt_tpl", s_mqtt_tpl, &len) != ESP_OK || len < 5) {
            strcpy(s_mqtt_tpl, "{\"device_id\": \"{0.0.42.0.0.255}\", \"power_active\": {1.0.1.7.0.255}, \"power_reactive\": {1.0.3.7.0.255}, \"voltage_l1\": {1.0.32.7.0.255}, \"voltage_l2\": {1.0.52.7.0.255}, \"voltage_l3\": {1.0.72.7.0.255}, \"current_l1\": {1.0.31.7.0.255}, \"current_l2\": {1.0.51.7.0.255}, \"current_l3\": {1.0.71.7.0.255}}");
        }
        nvs_close(nvs);
    } else {
        strcpy(s_mqtt_tpl, "{\"device_id\": \"{0.0.42.0.0.255}\", \"power_active\": {1.0.1.7.0.255}, \"power_reactive\": {1.0.3.7.0.255}, \"voltage_l1\": {1.0.32.7.0.255}, \"voltage_l2\": {1.0.52.7.0.255}, \"voltage_l3\": {1.0.72.7.0.255}, \"current_l1\": {1.0.31.7.0.255}, \"current_l2\": {1.0.51.7.0.255}, \"current_l3\": {1.0.71.7.0.255}}");
    }

    char lwt_topic[128];
    snprintf(lwt_topic, sizeof(lwt_topic), "%s/status", s_topic_prefix);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri,
        .credentials.username = username,
        .credentials.authentication.password = password,
        .session.last_will = {
            .topic = lwt_topic,
            .msg = "offline",
            .qos = 1,
            .retain = 1
        }
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_client) {
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    return ESP_OK;
}

esp_err_t mqtt_publisher_start(void) {
    if (!s_client) return ESP_ERR_INVALID_STATE;
    return esp_mqtt_client_start(s_client);
}

bool mqtt_publisher_is_connected(void) {
    return s_is_connected;
}

esp_err_t mqtt_publish_readings(const dlms_reading_t *readings, uint8_t count) {
    if (!s_is_connected) return ESP_ERR_INVALID_STATE;

    char *out_str = malloc(4096);
    if (!out_str) return ESP_ERR_NO_MEM;
    out_str[0] = '\0';
    
    int o = 0;
    for (int i = 0; s_mqtt_tpl[i] != '\0' && o < 4095; ) {
        if (s_mqtt_tpl[i] == '{' && s_mqtt_tpl[i+1] != '{') { // possible OBIS code
            int j = i + 1;
            while (s_mqtt_tpl[j] && s_mqtt_tpl[j] != '}' && (j - i) < 32) j++;
            if (s_mqtt_tpl[j] == '}') {
                char obis[32] = {0};
                strncpy(obis, s_mqtt_tpl + i + 1, j - i - 1);
                
                // Map names to OBIS if needed
                const char* mapped_obis = obis;
                if (strcmp(obis, "power_active") == 0) mapped_obis = "1.0.1.7.0.255";
                else if (strcmp(obis, "power_active_export") == 0) mapped_obis = "1.0.2.7.0.255";
                else if (strcmp(obis, "power_reactive") == 0) mapped_obis = "1.0.3.7.0.255";
                else if (strcmp(obis, "power_reactive_export") == 0) mapped_obis = "1.0.4.7.0.255";
                else if (strcmp(obis, "current_l1") == 0) mapped_obis = "1.0.31.7.0.255";
                else if (strcmp(obis, "current_l2") == 0) mapped_obis = "1.0.51.7.0.255";
                else if (strcmp(obis, "current_l3") == 0) mapped_obis = "1.0.71.7.0.255";
                else if (strcmp(obis, "voltage_l1") == 0) mapped_obis = "1.0.32.7.0.255";
                else if (strcmp(obis, "voltage_l2") == 0) mapped_obis = "1.0.52.7.0.255";
                else if (strcmp(obis, "voltage_l3") == 0) mapped_obis = "1.0.72.7.0.255";
                else if (strcmp(obis, "device_id") == 0) mapped_obis = "0.0.96.1.2.255";
                else if (strcmp(obis, "logical_device_name") == 0) mapped_obis = "0.0.42.0.0.255";

                // Get reading
                dlms_reading_t rdg = get_cached_reading(mapped_obis);
                if (rdg.valid || rdg.data_type != 0) {
                    if (rdg.data_type == AXDR_OCTET_STRING || rdg.data_type == AXDR_VISIBLE_STRING || rdg.data_type == AXDR_UTF8_STRING) {
                        o += snprintf(out_str + o, 4096 - o, "%s", rdg.str_val);
                    } else {
                        o += snprintf(out_str + o, 4096 - o, "%.3f", rdg.scaled_value != 0 ? rdg.scaled_value : rdg.float_val);
                    }
                } else {
                    o += snprintf(out_str + o, 4096 - o, "null");
                }
                i = j + 1;
                continue;
            }
        }
        out_str[o++] = s_mqtt_tpl[i++];
    }
    out_str[o] = '\0';

    char topic[128];
    snprintf(topic, sizeof(topic), "%s/state", s_topic_prefix);
    esp_mqtt_client_publish(s_client, topic, out_str, 0, 0, 0);
    free(out_str);
    return ESP_OK;
}

esp_err_t mqtt_publish_ha_discovery(const dlms_obis_entry_t *entries, uint8_t count, const char *topic_prefix) {
    if (!s_is_connected) return ESP_ERR_INVALID_STATE;
    const char *ha_prefix = "homeassistant";
    const char *prefix = topic_prefix ? topic_prefix : s_topic_prefix;

    for (uint8_t i = 0; i < count; i++) {
        char obis_key[DLMS_OBIS_STR_LEN];
        strncpy(obis_key, entries[i].obis_str, sizeof(obis_key));
        obis_key[sizeof(obis_key)-1] = '\0';
        replace_dots(obis_key);

        cJSON *root = cJSON_CreateObject();
        if (!root) continue;

        cJSON_AddStringToObject(root, "name", entries[i].name);
        
        char unique_id[64];
        snprintf(unique_id, sizeof(unique_id), "dlms_%s", obis_key);
        cJSON_AddStringToObject(root, "unique_id", unique_id);

        char state_topic[128];
        snprintf(state_topic, sizeof(state_topic), "%s/state", prefix);
        cJSON_AddStringToObject(root, "state_topic", state_topic);

        char val_tpl[64];
        snprintf(val_tpl, sizeof(val_tpl), "{{ value_json.%s }}", obis_key);
        cJSON_AddStringToObject(root, "value_template", val_tpl);

        if (strlen(entries[i].unit_str) > 0) {
            cJSON_AddStringToObject(root, "unit_of_measurement", entries[i].unit_str);
        }

        if (strlen(entries[i].device_class) > 0) {
            cJSON_AddStringToObject(root, "device_class", entries[i].device_class);
            if (strcmp(entries[i].device_class, "energy") == 0) {
                cJSON_AddStringToObject(root, "state_class", "total_increasing");
            } else {
                cJSON_AddStringToObject(root, "state_class", "measurement");
            }
        }

        cJSON *dev = cJSON_CreateObject();
        cJSON *ids = cJSON_CreateArray();
        cJSON_AddItemToArray(ids, cJSON_CreateString("dlms_meter"));
        cJSON_AddItemToObject(dev, "identifiers", ids);
        cJSON_AddStringToObject(dev, "name", "DLMS Meter");
        cJSON_AddStringToObject(dev, "model", "ESP32-C3 DLMS Bridge");
        cJSON_AddItemToObject(root, "device", dev);

        char *json_str = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);

        if (json_str) {
            char topic[256];
            snprintf(topic, sizeof(topic), "%s/sensor/dlms_%s/config", ha_prefix, obis_key);
            esp_mqtt_client_publish(s_client, topic, json_str, 0, 1, 1);
            free(json_str);
        }
    }
    return ESP_OK;
}
