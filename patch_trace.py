import codecs

with open('main/main.c', 'r', encoding='utf-8') as f:
    c = f.read()

replacement = """    cJSON *root = cJSON_CreateObject();
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
        cJSON_AddStringToObject(root, "trace", trace_str);
        free(trace_str);
    }
"""

c = c.replace('    cJSON *root = cJSON_CreateObject();\\n    cJSON_AddStringToObject(root, "hex", hex_str);', replacement)

with open('main/main.c', 'w', encoding='utf-8') as f:
    f.write(c)
