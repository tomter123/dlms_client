import re
with open('components/web_server/web_server.c', 'r') as f:
    text = f.read()

clear_old = """
static esp_err_t api_debug_handler(httpd_req_t *req) {
    char *buf = malloc(8192);
    if (!buf) return ESP_FAIL;
    get_debug_json(buf, 8192);
"""

clear_new = """
extern void clear_debug_buf(void);

static esp_err_t api_debug_handler(httpd_req_t *req) {
    char query[32];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (strstr(query, "clear=1")) {
            clear_debug_buf();
        }
    }

    char *buf = malloc(8192);
    if (!buf) return ESP_FAIL;
    get_debug_json(buf, 8192);
"""
text = text.replace(clear_old.strip(), clear_new.strip())
with open('components/web_server/web_server.c', 'w') as f:
    f.write(text)

with open('main/main.c', 'r') as f:
    text = f.read()
    
clear_func = """
void clear_debug_buf(void) {
    if (g_debug_mutex) {
        xSemaphoreTake(g_debug_mutex, portMAX_DELAY);
        g_debug_head = 0;
        g_debug_tail = 0;
        xSemaphoreGive(g_debug_mutex);
    }
}
"""
text = text.replace('void get_debug_json', clear_func + '\nvoid get_debug_json')
with open('main/main.c', 'w') as f:
    f.write(text)
