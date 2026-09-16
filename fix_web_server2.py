import codecs

with open('components/web_server/web_server.c', 'r', encoding='utf-8') as f:
    c = f.read()

# 1. Fix post_handler buffer size
c = c.replace('char buf[1024];', 'char *buf = malloc(4096);\\n    if(!buf) return ESP_FAIL;')
c = c.replace('if (remaining >= sizeof(buf)) {', 'if (remaining >= 4096) {\\n        free(buf);')
c = c.replace('if ((ret = httpd_req_recv(req, buf, remaining)) <= 0) {', 'if ((ret = httpd_req_recv(req, buf, remaining)) <= 0) {\\n        free(buf);')
c = c.replace('ESP_LOGI(TAG, "Rebooting in 1s...");', 'free(buf);\\n    ESP_LOGI(TAG, "Rebooting in 1s...");')

# 2. Fix api_config_handler to return everything
api_config_start = c.find('static esp_err_t api_config_handler')
api_config_end = c.find('static esp_err_t api_role_handler', api_config_start)

new_api_config = """static esp_err_t api_config_handler(httpd_req_t *req) {
    if (check_auth(req) != 2) return request_auth(req);
    
    char *buf = malloc(8192);
    if (!buf) return ESP_FAIL;
    
    nvs_handle_t nvs;
    char mqtt_tpl[1024] = {0};
    char meter_type[32] = "e570";
    char mqtt_uri[128] = "mqtt://192.168.0.100:1883";
    char mqtt_topic[64] = "dlms/meter";
    char mqtt_user[64] = {0};
    char mqtt_pass[64] = {0};
    char auth[16] = "none";
    char pass[64] = {0};
    char parity[16] = "none";
    uint8_t mqtt_en = 0;
    uint32_t baud = 9600;
    uint32_t client_addr = 16;
    uint32_t server_logical = 1;
    uint32_t server_physical = 17;

    if (nvs_open("config", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(mqtt_tpl); nvs_get_str(nvs, "mqtt_tpl", mqtt_tpl, &len);
        len = sizeof(meter_type); nvs_get_str(nvs, "meter_type", meter_type, &len);
        len = sizeof(mqtt_uri); nvs_get_str(nvs, "mqtt_uri", mqtt_uri, &len);
        len = sizeof(mqtt_topic); nvs_get_str(nvs, "mqtt_topic", mqtt_topic, &len);
        len = sizeof(mqtt_user); nvs_get_str(nvs, "mqtt_user", mqtt_user, &len);
        len = sizeof(mqtt_pass); nvs_get_str(nvs, "mqtt_pass", mqtt_pass, &len);
        len = sizeof(auth); nvs_get_str(nvs, "auth", auth, &len);
        len = sizeof(pass); nvs_get_str(nvs, "pass", pass, &len);
        len = sizeof(parity); nvs_get_str(nvs, "parity", parity, &len);
        
        nvs_get_u8(nvs, "mqtt_en", &mqtt_en);
        nvs_get_u32(nvs, "baud", &baud);
        nvs_get_u32(nvs, "client_addr", &client_addr);
        nvs_get_u32(nvs, "server_logical", &server_logical);
        nvs_get_u32(nvs, "server_physical", &server_physical);
        
        nvs_close(nvs);
    }
    
    char escaped[2048] = {0};
    int j = 0;
    for(int i=0; mqtt_tpl[i] && j<2047; i++){
        if(mqtt_tpl[i] == '"') { escaped[j++] = '\\\\'; escaped[j++] = '"'; }
        else if(mqtt_tpl[i] == '\\n') { escaped[j++] = '\\\\'; escaped[j++] = 'n'; }
        else if(mqtt_tpl[i] == '\\r') { }
        else escaped[j++] = mqtt_tpl[i];
    }
    
    snprintf(buf, 8192, 
        "{\\"mqtt_tpl\\":\\"%s\\", \\"meter_type\\":\\"%s\\", \\"mqtt_en\\":%d, "
        "\\"mqtt_uri\\":\\"%s\\", \\"mqtt_topic\\":\\"%s\\", \\"mqtt_user\\":\\"%s\\", "
        "\\"mqtt_pass\\":\\"%s\\", \\"auth\\":\\"%s\\", \\"pass\\":\\"%s\\", "
        "\\"parity\\":\\"%s\\", \\"baud\\":%lu, \\"client_addr\\":%lu, "
        "\\"server_logical\\":%lu, \\"server_physical\\":%lu}",
        escaped, meter_type, mqtt_en, mqtt_uri, mqtt_topic, mqtt_user, mqtt_pass, 
        auth, pass, parity, baud, client_addr, server_logical, server_physical);
        
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, strlen(buf));
    free(buf);
    return ESP_OK;
}

"""
c = c[:api_config_start] + new_api_config + c[api_config_end:]

with open('components/web_server/web_server.c', 'w', encoding='utf-8') as f:
    f.write(c)

