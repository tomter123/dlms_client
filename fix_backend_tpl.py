import codecs

with codecs.open('components/web_server/web_server.c', 'r', 'utf-8') as f:
    c = f.read()

# 1. Add api_config_handler
api_config = """
static esp_err_t api_config_handler(httpd_req_t *req) {
    if (check_auth(req) != 2) return request_auth(req);
    
    char *buf = malloc(4096);
    if (!buf) return ESP_FAIL;
    
    nvs_handle_t nvs;
    char mqtt_tpl[1024] = {0};
    if (nvs_open("config", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(mqtt_tpl);
        nvs_get_str(nvs, "mqtt_tpl", mqtt_tpl, &len);
        nvs_close(nvs);
    }
    
    // basic escape for JSON (just quotes and newlines for now is enough for template)
    char escaped[2048] = {0};
    int j = 0;
    for(int i=0; mqtt_tpl[i] && j<2047; i++){
        if(mqtt_tpl[i] == '"') { escaped[j++] = '\\\\'; escaped[j++] = '"'; }
        else if(mqtt_tpl[i] == '\\n') { escaped[j++] = '\\\\'; escaped[j++] = 'n'; }
        else if(mqtt_tpl[i] == '\\r') { }
        else escaped[j++] = mqtt_tpl[i];
    }
    
    snprintf(buf, 4096, "{\\"mqtt_tpl\\":\\"%s\\"}", escaped);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, strlen(buf));
    free(buf);
    return ESP_OK;
}
"""
c = c.replace('static esp_err_t api_debug_handler(httpd_req_t *req) {', api_config + '\nstatic esp_err_t api_debug_handler(httpd_req_t *req) {')

# 2. Add /api/config route
uri_config = """
        httpd_uri_t uri_config = { .uri = "/api/config", .method = HTTP_GET, .handler = api_config_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_config);
"""
c = c.replace('httpd_register_uri_handler(server, &uri_debug);', 'httpd_register_uri_handler(server, &uri_debug);\n' + uri_config)

# 3. Save mqtt_tpl in /save handler
save_tpl = """
                else if (strcmp(p, "mqtt_tpl") == 0) {
                    // It can be long, so be careful
                    nvs_set_str(nvs, "mqtt_tpl", decoded);
                }
"""
c = c.replace('else if (strcmp(p, "mqtt_user") == 0 || strcmp(p, "mqtt_pass") == 0) {', 'else if (strcmp(p, "mqtt_user") == 0 || strcmp(p, "mqtt_pass") == 0) {\n' + save_tpl)

with codecs.open('components/web_server/web_server.c', 'w', 'utf-8') as f:
    f.write(c)
