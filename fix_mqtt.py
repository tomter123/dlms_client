import codecs

with codecs.open('components/mqtt_publisher/mqtt_publisher.c', 'r', 'utf-8') as f:
    c = f.read()

# Add global for template
tpl_global = """
static char s_mqtt_tpl[1024] = {0};
extern dlms_reading_t get_cached_reading(const char *obis); // We will define this in main.c
"""
c = c.replace('static char s_topic_prefix[64] = "dlms/meter";', 'static char s_topic_prefix[64] = "dlms/meter";\n' + tpl_global)

# Add NVS read in init
init_add = """
    nvs_handle_t nvs;
    if (nvs_open("config", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(s_mqtt_tpl);
        if (nvs_get_str(nvs, "mqtt_tpl", s_mqtt_tpl, &len) != ESP_OK || len < 5) {
            strcpy(s_mqtt_tpl, "{\\"device_id\\": \\"{0.0.42.0.0.255}\\", \\"power_active\\": {1.0.1.7.0.255}, \\"power_reactive\\": {1.0.3.7.0.255}, \\"voltage_l1\\": {1.0.32.7.0.255}, \\"voltage_l2\\": {1.0.52.7.0.255}, \\"voltage_l3\\": {1.0.72.7.0.255}, \\"current_l1\\": {1.0.31.7.0.255}, \\"current_l2\\": {1.0.51.7.0.255}, \\"current_l3\\": {1.0.71.7.0.255}}");
        }
        nvs_close(nvs);
    } else {
        strcpy(s_mqtt_tpl, "{\\"device_id\\": \\"{0.0.42.0.0.255}\\", \\"power_active\\": {1.0.1.7.0.255}, \\"power_reactive\\": {1.0.3.7.0.255}, \\"voltage_l1\\": {1.0.32.7.0.255}, \\"voltage_l2\\": {1.0.52.7.0.255}, \\"voltage_l3\\": {1.0.72.7.0.255}, \\"current_l1\\": {1.0.31.7.0.255}, \\"current_l2\\": {1.0.51.7.0.255}, \\"current_l3\\": {1.0.71.7.0.255}}");
    }
"""
c = c.replace('char lwt_topic[128];', init_add + '\n    char lwt_topic[128];')

# Rewrite mqtt_publish_readings
publish_old = """
esp_err_t mqtt_publish_readings(const dlms_reading_t *readings, uint8_t count) {
    if (!s_is_connected) return ESP_ERR_INVALID_STATE;

    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;

    for (uint8_t i = 0; i < count; i++) {
        if (!readings[i].valid && readings[i].data_type == 0) continue;

        char key[DLMS_OBIS_STR_LEN];
        strncpy(key, readings[i].obis_str, sizeof(key));
        key[sizeof(key)-1] = '\\0';
        replace_dots(key);

        if (readings[i].data_type == AXDR_OCTET_STRING || readings[i].data_type == AXDR_VISIBLE_STRING || readings[i].data_type == AXDR_UTF8_STRING) {
            cJSON_AddStringToObject(root, key, readings[i].str_val);
        } else {
            cJSON_AddNumberToObject(root, key, readings[i].float_val);
        }
    }
    
    int64_t uptime_ms = esp_timer_get_time() / 1000;
    cJSON_AddNumberToObject(root, "uptime_ms", uptime_ms);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str) {
        char topic[128];
        snprintf(topic, sizeof(topic), "%s/state", s_topic_prefix);
        esp_mqtt_client_publish(s_client, topic, json_str, 0, 0, 0);
        free(json_str);
        return ESP_OK;
    }
    
    return ESP_FAIL;
}
"""

publish_new = """
esp_err_t mqtt_publish_readings(const dlms_reading_t *readings, uint8_t count) {
    if (!s_is_connected) return ESP_ERR_INVALID_STATE;

    char *out_str = malloc(4096);
    if (!out_str) return ESP_ERR_NO_MEM;
    out_str[0] = '\\0';
    
    int o = 0;
    for (int i = 0; s_mqtt_tpl[i] != '\\0' && o < 4095; ) {
        if (s_mqtt_tpl[i] == '{' && s_mqtt_tpl[i+1] != '{') { // possible OBIS code
            int j = i + 1;
            while (s_mqtt_tpl[j] && s_mqtt_tpl[j] != '}' && (j - i) < 32) j++;
            if (s_mqtt_tpl[j] == '}') {
                char obis[32] = {0};
                strncpy(obis, s_mqtt_tpl + i + 1, j - i - 1);
                
                // Get reading
                dlms_reading_t rdg = get_cached_reading(obis);
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
    out_str[o] = '\\0';

    char topic[128];
    snprintf(topic, sizeof(topic), "%s/state", s_topic_prefix);
    esp_mqtt_client_publish(s_client, topic, out_str, 0, 0, 0);
    free(out_str);
    return ESP_OK;
}
"""

c = c.replace(publish_old.strip(), publish_new.strip())

with codecs.open('components/mqtt_publisher/mqtt_publisher.c', 'w', 'utf-8') as f:
    f.write(c)

