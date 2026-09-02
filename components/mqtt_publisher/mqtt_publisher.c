#include "mqtt_publisher.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "mqtt_publisher";

static esp_mqtt_client_handle_t s_client = NULL;
static bool s_is_connected = false;

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
            snprintf(topic, sizeof(topic), "%s/status", CONFIG_MQTT_TOPIC_PREFIX);
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

esp_err_t mqtt_publisher_init(void) {
    char lwt_topic[128];
    snprintf(lwt_topic, sizeof(lwt_topic), "%s/status", CONFIG_MQTT_TOPIC_PREFIX);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URI,
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

    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;

    for (uint8_t i = 0; i < count; i++) {
        if (!readings[i].valid) continue;

        char key[DLMS_OBIS_STR_LEN];
        strncpy(key, readings[i].obis_str, sizeof(key));
        key[sizeof(key)-1] = '\0';
        replace_dots(key);

        cJSON_AddNumberToObject(root, key, readings[i].scaled_value);
    }
    
    int64_t uptime_ms = esp_timer_get_time() / 1000;
    cJSON_AddNumberToObject(root, "uptime_ms", uptime_ms);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str) {
        char topic[128];
        snprintf(topic, sizeof(topic), "%s/state", CONFIG_MQTT_TOPIC_PREFIX);
        esp_mqtt_client_publish(s_client, topic, json_str, 0, 0, 0);
        free(json_str);
        return ESP_OK;
    }
    
    return ESP_FAIL;
}

esp_err_t mqtt_publish_ha_discovery(const dlms_obis_entry_t *entries, uint8_t count) {
#ifdef CONFIG_MQTT_HA_DISCOVERY_ENABLED
    if (!s_is_connected) return ESP_ERR_INVALID_STATE;

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
        snprintf(state_topic, sizeof(state_topic), "%s/state", CONFIG_MQTT_TOPIC_PREFIX);
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
            snprintf(topic, sizeof(topic), "%s/sensor/dlms_%s/config", CONFIG_MQTT_HA_DISCOVERY_PREFIX, obis_key);
            esp_mqtt_client_publish(s_client, topic, json_str, 0, 1, 1);
            free(json_str);
        }
    }
#endif
    return ESP_OK;
}
