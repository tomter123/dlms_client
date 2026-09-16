import codecs

with open('main/main.c', 'r', encoding='utf-8') as f:
    c = f.read()

cache_func = """void cache_readings(const dlms_reading_t *readings, uint8_t count) {
    if (g_readings_mutex) {
        xSemaphoreTake(g_readings_mutex, portMAX_DELAY);
        for (int i = 0; i < count; i++) {
            bool found = false;
            for (int j = 0; j < DLMS_MAX_OBIS_ENTRIES; j++) {
                if ((g_dashboard_readings[j].valid || g_dashboard_readings[j].data_type != 0) &&
                    strcmp(g_dashboard_readings[j].obis_str, readings[i].obis_str) == 0) {
                    g_dashboard_readings[j] = readings[i];
                    found = true;
                    break;
                }
            }
            if (!found) {
                for (int j = 0; j < DLMS_MAX_OBIS_ENTRIES; j++) {
                    if (!g_dashboard_readings[j].valid && g_dashboard_readings[j].data_type == 0) {
                        g_dashboard_readings[j] = readings[i];
                        break;
                    }
                }
            }
        }
        xSemaphoreGive(g_readings_mutex);
    }
}
"""

c = c.replace('dlms_reading_t get_cached_reading(const char *obis) {', cache_func + '\\ndlms_reading_t get_cached_reading(const char *obis) {')

c = c.replace('mqtt_publish_readings(readings, count);', 'mqtt_publish_readings(readings, count);\\n        cache_readings(readings, count);')

c = c.replace('mqtt_publish_readings(&rdg, 1);', 'mqtt_publish_readings(&rdg, 1);\\n                                          cache_readings(&rdg, 1);')

with open('main/main.c', 'w', encoding='utf-8') as f:
    f.write(c)
