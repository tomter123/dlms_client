import codecs

with codecs.open('main/main.c', 'r', 'utf-8') as f:
    c = f.read()

# Add get_cached_reading function
get_cached = """
dlms_reading_t get_cached_reading(const char *obis) {
    dlms_reading_t rdg = {0};
    if (g_readings_mutex) {
        xSemaphoreTake(g_readings_mutex, portMAX_DELAY);
        for (int i = 0; i < DLMS_MAX_OBIS_ENTRIES; i++) {
            if ((g_dashboard_readings[i].valid || g_dashboard_readings[i].data_type != 0) &&
                strcmp(g_dashboard_readings[i].obis_str, obis) == 0) {
                rdg = g_dashboard_readings[i];
                break;
            }
        }
        xSemaphoreGive(g_readings_mutex);
    }
    return rdg;
}
"""
c = c.replace('void get_dashboard_json(char *buf, size_t max_len)', get_cached + '\nvoid get_dashboard_json(char *buf, size_t max_len)')

with codecs.open('main/main.c', 'w', 'utf-8') as f:
    f.write(c)
