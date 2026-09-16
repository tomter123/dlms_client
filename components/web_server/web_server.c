#include "web_server.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "mbedtls/base64.h"
#include "esp_system.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "web_server";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

extern void get_dashboard_json(char *buf, size_t max_len);
extern void get_debug_json(char *buf, size_t max_len);
extern void clear_debug_buf(void);

static esp_err_t request_auth(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"DLMS Gateway\"");
    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    return ESP_OK;
}

static int check_auth(httpd_req_t *req) {
    size_t len = httpd_req_get_hdr_value_len(req, "Authorization");
    if (len == 0) return 0;
    char auth_header[128];
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_header, sizeof(auth_header)) != ESP_OK) return 0;
    if (strncmp(auth_header, "Basic ", 6) != 0) return 0;
    char decoded[128];
    size_t olen = 0;
    mbedtls_base64_decode((unsigned char*)decoded, sizeof(decoded), &olen, (const unsigned char*)(auth_header + 6), strlen(auth_header + 6));
    decoded[olen] = '\0';
    if (strcmp(decoded, "admin:admin") == 0) return 2;
    if (strcmp(decoded, "user:user") == 0) return 1;
    return 0;
}

static esp_err_t api_role_handler(httpd_req_t *req) {
    int role = check_auth(req);
    if (role == 0) return request_auth(req);
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"role\":\"%s\"}", role == 2 ? "admin" : "user");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, strlen(buf));
    return ESP_OK;
}

static esp_err_t get_handler(httpd_req_t *req) {
    if (check_auth(req) == 0) return request_auth(req);
    httpd_resp_set_type(req, "text/html");
    const size_t index_html_size = (index_html_end - index_html_start);
    httpd_resp_send(req, (const char *)index_html_start, index_html_size);
    return ESP_OK;
}

static void urldecode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit((unsigned char)a) && isxdigit((unsigned char)b))) {
            if (a >= 'a') a -= 'a'-'A';
            if (a >= 'A') a -= ('A' - 10); else a -= '0';
            if (b >= 'a') b -= 'a'-'A';
            if (b >= 'A') b -= ('A' - 10); else b -= '0';
            *dst++ = 16*a+b;
            src+=3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst++ = '\0';
}

static esp_err_t post_handler(httpd_req_t *req) {
    if (check_auth(req) != 2) return request_auth(req);
    char *buf = malloc(4096);
    if (!buf) return ESP_FAIL;
    int ret, remaining = req->content_len;
    if (remaining >= 4096) {
        free(buf);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    if ((ret = httpd_req_recv(req, buf, remaining)) <= 0) {
        free(buf);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    ESP_LOGI(TAG, "Form data: %s", buf);

    nvs_handle_t nvs;
    if (nvs_open("config", NVS_READWRITE, &nvs) != ESP_OK) {
        free(buf);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    nvs_set_u8(nvs, "mqtt_en", 0);
    char *p = strtok(buf, "&");
    while (p) {
        char *eq = strchr(p, '=');
        if (eq) {
            *eq = '\0';
            char *val = eq + 1;
            char *decoded = calloc(1, 4096);
            urldecode(decoded, val);
            if (strlen(decoded) > 0) {
                if (strcmp(p, "ssid") == 0 || strcmp(p, "pass") == 0 || strcmp(p, "meter_type") == 0 ||
                    strcmp(p, "mqtt_uri") == 0 || strcmp(p, "mqtt_topic") == 0 ||
                    strcmp(p, "mqtt_user") == 0 || strcmp(p, "mqtt_pass") == 0 ||
                    strcmp(p, "auth") == 0 || strcmp(p, "pass") == 0 ||
                    strcmp(p, "parity") == 0 || strcmp(p, "e450_parity") == 0 || strcmp(p, "mbus_parity") == 0 ||
                    strcmp(p, "mqtt_tpl") == 0 || strcmp(p, "hidden_obis") == 0) {
                    nvs_set_str(nvs, p, decoded);
                } 
                else if (strcmp(p, "baud") == 0 || strcmp(p, "client_addr") == 0 || 
                         strcmp(p, "server_logical") == 0 || strcmp(p, "server_physical") == 0 ||
                         strcmp(p, "databits") == 0 || strcmp(p, "stopbits") == 0 ||
                         strcmp(p, "e450_baud") == 0 || strcmp(p, "e450_databits") == 0 || 
                         strcmp(p, "e450_stopbits") == 0 || strcmp(p, "mbus_baud") == 0 || 
                         strcmp(p, "mbus_addr") == 0 || strcmp(p, "mbus_databits") == 0 || 
                         strcmp(p, "mbus_stopbits") == 0 || strcmp(p, "am550_baud") == 0 ||
                         strcmp(p, "iec_baud") == 0 || strcmp(p, "iec_target_baud") == 0) {
                    nvs_set_u32(nvs, p, atoi(decoded));
                }
            }
            if (strcmp(p, "mqtt_enable") == 0 && strcmp(decoded, "on") == 0) {
                nvs_set_u8(nvs, "mqtt_en", 1);
            }
            free(decoded);
        }
        p = strtok(NULL, "&");
    }
    nvs_commit(nvs);
    nvs_close(nvs);

    const char *resp = "<!DOCTYPE html><html><body><h2>Settings saved!</h2><p>Rebooting device...</p></body></html>";
    httpd_resp_send(req, resp, strlen(resp));
    free(buf);
    ESP_LOGI(TAG, "Rebooting in 1s...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

static esp_err_t api_data_handler(httpd_req_t *req) {
    if (check_auth(req) == 0) return request_auth(req);
    char *buf = malloc(4096);
    if (!buf) return ESP_FAIL;
    get_dashboard_json(buf, 4096);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, strlen(buf));
    free(buf);
    return ESP_OK;
}

static esp_err_t api_debug_handler(httpd_req_t *req) {
    if (check_auth(req) != 2) return request_auth(req);
    char buf[256];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char val[16];
        if (httpd_query_key_value(buf, "clear", val, sizeof(val)) == ESP_OK) {
            clear_debug_buf();
        }
    }
    char *dbuf = calloc(1, 32768);
    if (!dbuf) return ESP_FAIL;
    get_debug_json(dbuf, 32768);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, dbuf, strlen(dbuf));
    free(dbuf);
    return ESP_OK;
}

static esp_err_t api_config_handler(httpd_req_t *req) {
    if (check_auth(req) != 2) return request_auth(req);
    char *buf = calloc(1, 16384);
    if (!buf) return ESP_FAIL;
    
    nvs_handle_t nvs;
    char *mqtt_tpl = calloc(1, 1024); if(!mqtt_tpl) { free(buf); return ESP_FAIL; }
    char *hidden_obis = calloc(1, 1024); if(!hidden_obis) { free(mqtt_tpl); free(buf); return ESP_FAIL; }
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
    uint32_t am550_baud = 115200;
    uint32_t iec_baud = 300;
    uint32_t iec_target_baud = 9600;

    if (nvs_open("config", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = 1024; nvs_get_str(nvs, "mqtt_tpl", mqtt_tpl, &len);
        len = 1024; nvs_get_str(nvs, "hidden_obis", hidden_obis, &len);
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
        nvs_get_u32(nvs, "am550_baud", &am550_baud);
        nvs_get_u32(nvs, "iec_baud", &iec_baud);
        nvs_get_u32(nvs, "iec_target_baud", &iec_target_baud);
        
        nvs_close(nvs);
    }
    
    char *escaped = calloc(1, 2048); if(!escaped) { free(hidden_obis); free(mqtt_tpl); free(buf); return ESP_FAIL; }
    int j = 0;
    for(int i=0; mqtt_tpl[i] && j<2047; i++){
        if(mqtt_tpl[i] == '"') { escaped[j++] = '\\'; escaped[j++] = '"'; }
        else if(mqtt_tpl[i] == '\n') { escaped[j++] = '\\'; escaped[j++] = 'n'; }
        else if(mqtt_tpl[i] == '\r') { }
        else escaped[j++] = mqtt_tpl[i];
    }
    
    snprintf(buf, 16384, 
        "{\"mqtt_tpl\":\"%s\", \"meter_type\":\"%s\", \"mqtt_en\":%d, "
        "\"mqtt_uri\":\"%s\", \"mqtt_topic\":\"%s\", \"mqtt_user\":\"%s\", "
        "\"mqtt_pass\":\"%s\", \"auth\":\"%s\", \"pass\":\"%s\", "
        "\"parity\":\"%s\", \"baud\":%lu, \"client_addr\":%lu, "
        "\"server_logical\":%lu, \"server_physical\":%lu, \"hidden_obis\":\"%s\", "
        "\"am550_baud\":%lu, \"iec_baud\":%lu, \"iec_target_baud\":%lu}",
        escaped, meter_type, mqtt_en, mqtt_uri, mqtt_topic, mqtt_user, mqtt_pass, 
        auth, pass, parity, baud, client_addr, server_logical, server_physical, hidden_obis,
        am550_baud, iec_baud, iec_target_baud);
        
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, strlen(buf));
    free(escaped);
    free(hidden_obis);
    free(mqtt_tpl);
    free(buf);
    return ESP_OK;
}

esp_err_t web_server_init(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_get = { .uri = "/", .method = HTTP_GET, .handler = get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_get);
        httpd_uri_t uri_post = { .uri = "/save", .method = HTTP_POST, .handler = post_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_post);
        httpd_uri_t uri_data = { .uri = "/api/data", .method = HTTP_GET, .handler = api_data_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_data);
        httpd_uri_t uri_debug = { .uri = "/api/debug", .method = HTTP_GET, .handler = api_debug_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_debug);
        httpd_uri_t uri_config = { .uri = "/api/config", .method = HTTP_GET, .handler = api_config_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_config);
        httpd_uri_t uri_role = { .uri = "/api/role", .method = HTTP_GET, .handler = api_role_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_role);
        return ESP_OK;
    }
    return ESP_FAIL;
}
