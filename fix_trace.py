import codecs

with open('main/main.c', 'r', encoding='utf-8') as f:
    c = f.read()

# I will replace the mangled function starting at oid get_debug_json
# The file has a known structure. The next function is oid e450_push_task
start = c.find('void get_debug_json(char *buf, size_t max_len)')
end = c.find('void e450_push_task(void *pvParameters);')

new_func = """void get_debug_json(char *buf, size_t max_len) {
    if (!g_debug_mutex) { snprintf(buf, max_len, "{\\"hex\\":\\"\\"}"); return; }
    
    char *hex_str = malloc(DEBUG_BUF_SIZE * 3 + 1);
    if (!hex_str) { snprintf(buf, max_len, "{}"); return; }
    
    hex_str[0] = '\\0';
    int pos = 0;
    
    xSemaphoreTake(g_debug_mutex, portMAX_DELAY);
    size_t curr = g_debug_tail;
    while (curr != g_debug_head && pos < (DEBUG_BUF_SIZE * 3 - 4)) {
        pos += sprintf(hex_str + pos, "%02X ", g_debug_buf[curr]);
        curr = (curr + 1) % DEBUG_BUF_SIZE;
    }
    xSemaphoreGive(g_debug_mutex);
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "hex", hex_str);
    
    char *trace_str = malloc(TRACE_BUF_SIZE + 1);
    if (trace_str) {
        portENTER_CRITICAL(&s_trace_mux);
        int tpos = 0;
        int tcurr = g_trace_tail;
        while (tcurr != g_trace_head && tpos < TRACE_BUF_SIZE) {
            trace_str[tpos++] = g_trace_buf[tcurr];
            tcurr = (tcurr + 1) % TRACE_BUF_SIZE;
        }
        trace_str[tpos] = '\\0';
        portEXIT_CRITICAL(&s_trace_mux);
        
        // Remove ANSI codes
        for (int i = 0; i < tpos; i++) {
            if (trace_str[i] == '\\033') {
                int j = i;
                while (j < tpos && trace_str[j] != 'm') j++;
                if (j < tpos) {
                    memmove(&trace_str[i], &trace_str[j+1], tpos - j);
                    tpos -= (j + 1 - i);
                    i--;
                }
            }
        }
        
        cJSON_AddStringToObject(root, "trace", trace_str);
        free(trace_str);
    }
    
    char *json_str = cJSON_PrintUnformatted(root);
    strncpy(buf, json_str, max_len-1);
    free(json_str);
    cJSON_Delete(root);
    free(hex_str);
}

"""

c = c[:start] + new_func + c[end:]

with open('main/main.c', 'w', encoding='utf-8') as f:
    f.write(c)
