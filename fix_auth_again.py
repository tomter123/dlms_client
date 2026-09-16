import codecs

auth_func = """
#include "mbedtls/base64.h"
#include <string.h>

// 0: none, 1: user, 2: admin
static esp_err_t request_auth(httpd_req_t *req) {
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\\"DLMS Smart Gateway\\"");
    httpd_resp_send(req, "Unauthorized", 12);
    return ESP_OK;
}

static int check_auth(httpd_req_t *req) {
    char auth_header[128];
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_header, sizeof(auth_header)) != ESP_OK) {
        return 0;
    }
    if (strncmp(auth_header, "Basic ", 6) != 0) return 0;
    
    char decoded[128];
    size_t olen = 0;
    mbedtls_base64_decode((unsigned char*)decoded, sizeof(decoded), &olen, (const unsigned char*)(auth_header + 6), strlen(auth_header + 6));
    decoded[olen] = '\\0';
    
    if (strcmp(decoded, "admin:admin") == 0) return 2;
    if (strcmp(decoded, "user:user") == 0) return 1;
    return 0;
}

static esp_err_t api_role_handler(httpd_req_t *req) {
    int role = check_auth(req);
    if (role == 0) return request_auth(req);
    
    char buf[64];
    snprintf(buf, sizeof(buf), "{\\"role\\":\\"%s\\"}", role == 2 ? "admin" : "user");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, strlen(buf));
    return ESP_OK;
}
"""

with open('components/web_server/web_server.c', 'r', encoding='utf-8') as f:
    text = f.read()

text = auth_func + '\n' + text

with open('components/web_server/web_server.c', 'w', encoding='utf-8') as f:
    f.write(text)
