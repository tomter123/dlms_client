import codecs

with open('components/web_server/web_server.c', 'r', encoding='utf-8') as f:
    c = f.read()

# I will replace post_handler body entirely!
post_handler_start = c.find('static esp_err_t post_handler(httpd_req_t *req) {')
post_handler_end = c.find('static esp_err_t api_data_handler', post_handler_start)

new_post_handler = """static esp_err_t post_handler(httpd_req_t *req) {
    if (check_auth(req) != 2) return request_auth(req);
    char buf[1024];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    if ((ret = httpd_req_recv(req, buf, remaining)) <= 0) {
        return ESP_FAIL;
    }
    buf[ret] = '\\0';
    ESP_LOGI(TAG, "Form data: %s", buf);

    nvs_handle_t nvs;
    if (nvs_open("config", NVS_READWRITE, &nvs) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    nvs_set_u8(nvs, "mqtt_en", 0);

    char *p = strtok(buf, "&");
    while (p) {
        char *eq = strchr(p, '=');
        if (eq) {
            *eq = '\\0';
            char *val = eq + 1;
            char decoded[1024];
            urldecode(decoded, val);
            
            if (strlen(decoded) > 0) {
                if (strcmp(p, "ssid") == 0 || strcmp(p, "pass") == 0 || strcmp(p, "meter_type") == 0 ||
                    strcmp(p, "mqtt_uri") == 0 || strcmp(p, "mqtt_topic") == 0 ||
                    strcmp(p, "mqtt_user") == 0 || strcmp(p, "mqtt_pass") == 0 ||
                    strcmp(p, "auth") == 0 || strcmp(p, "pass") == 0 ||
                    strcmp(p, "parity") == 0 || strcmp(p, "e450_parity") == 0 || strcmp(p, "mbus_parity") == 0 ||
                    strcmp(p, "mqtt_tpl") == 0) {
                    nvs_set_str(nvs, p, decoded);
                } 
                else if (strcmp(p, "baud") == 0 || strcmp(p, "client_addr") == 0 || 
                         strcmp(p, "server_logical") == 0 || strcmp(p, "server_physical") == 0 ||
                         strcmp(p, "databits") == 0 || strcmp(p, "stopbits") == 0 ||
                         strcmp(p, "e450_baud") == 0 || strcmp(p, "e450_databits") == 0 || 
                         strcmp(p, "e450_stopbits") == 0 || strcmp(p, "mbus_baud") == 0 || 
                         strcmp(p, "mbus_addr") == 0 || strcmp(p, "mbus_databits") == 0 || 
                         strcmp(p, "mbus_stopbits") == 0) {
                    nvs_set_u32(nvs, p, atoi(decoded));
                }
            }

            if (strcmp(p, "mqtt_enable") == 0 && strcmp(decoded, "on") == 0) {
                nvs_set_u8(nvs, "mqtt_en", 1);
            }
        }
        p = strtok(NULL, "&");
    }

    nvs_commit(nvs);
    nvs_close(nvs);

    const char *resp = "<!DOCTYPE html><html><body><h2>Settings saved!</h2><p>Rebooting device...</p></body></html>";
    httpd_resp_send(req, resp, strlen(resp));
    
    ESP_LOGI(TAG, "Rebooting in 1s...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

"""
c = c[:post_handler_start] + new_post_handler + c[post_handler_end:]

with open('components/web_server/web_server.c', 'w', encoding='utf-8') as f:
    f.write(c)

