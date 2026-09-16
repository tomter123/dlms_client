import re

# 1. Update web_server.c
with open('components/web_server/web_server.c', 'r') as f:
    text = f.read()

auth_func = """
#include "mbedtls/base64.h"

// 0: none, 1: user, 2: admin
static int check_auth(httpd_req_t *req) {
    char auth_header[128];
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_header, sizeof(auth_header)) != ESP_OK) {
        return 0;
    }
    if (strncmp(auth_header, "Basic ", 6) != 0) return 0;
    
    char decoded[128];
    size_t olen = 0;
    mbedtls_base64_decode((unsigned char*)decoded, sizeof(decoded), &olen, (const unsigned char*)(auth_header + 6), strlen(auth_header + 6));
    decoded[olen] = '\0';
    
    if (strcmp(decoded, "admin:admin") == 0) return 2;
    if (strcmp(decoded, "user:user") == 0) return 1;
    return 0;
}

static esp_err_t request_auth(httpd_req_t *req) {
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"DLMS Smart Gateway\"");
    httpd_resp_send(req, "Unauthorized", 12);
    return ESP_OK;
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
"""

# Insert auth_func after #include <string.h>
text = text.replace('#include <string.h>', '#include <string.h>\n' + auth_func)

# Update get_handler to enforce at least user
get_handler_old = """
static esp_err_t get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
"""
get_handler_new = """
static esp_err_t get_handler(httpd_req_t *req) {
    if (check_auth(req) == 0) return request_auth(req);
    httpd_resp_set_type(req, "text/html");
"""
text = text.replace(get_handler_old.strip(), get_handler_new.strip())

# Update api_data_handler to enforce at least user
api_data_old = """
static esp_err_t api_data_handler(httpd_req_t *req) {
    char *buf = malloc(4096);
"""
api_data_new = """
static esp_err_t api_data_handler(httpd_req_t *req) {
    if (check_auth(req) == 0) return request_auth(req);
    char *buf = malloc(4096);
"""
text = text.replace(api_data_old.strip(), api_data_new.strip())

# Update api_debug_handler to enforce admin
api_debug_old = """
static esp_err_t api_debug_handler(httpd_req_t *req) {
    char query[32];
"""
api_debug_new = """
static esp_err_t api_debug_handler(httpd_req_t *req) {
    if (check_auth(req) != 2) return request_auth(req);
    char query[32];
"""
text = text.replace(api_debug_old.strip(), api_debug_new.strip())

# Update post_handler (save) to enforce admin
post_handler_old = """
static esp_err_t post_handler(httpd_req_t *req) {
    char buf[1024];
"""
post_handler_new = """
static esp_err_t post_handler(httpd_req_t *req) {
    if (check_auth(req) != 2) return request_auth(req);
    char buf[1024];
"""
text = text.replace(post_handler_old.strip(), post_handler_new.strip())

# Register api/role
uri_role = """
        httpd_uri_t uri_role = { .uri = "/api/role", .method = HTTP_GET, .handler = api_role_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_role);
"""
text = text.replace('httpd_register_uri_handler(server, &uri_debug);', 'httpd_register_uri_handler(server, &uri_debug);\n' + uri_role)

with open('components/web_server/web_server.c', 'w') as f:
    f.write(text)

# 2. Update index.html
with open('components/web_server/html/index.html', 'r', encoding='utf-8') as f:
    html = f.read()

# Add logic to hide Config and Debug if role != 'admin'
js_auth = """
        fetch('/api/role').then(r => r.json()).then(data => {
            if (data.role !== 'admin') {
                // Hide tabs
                document.querySelector('.tab[onclick="switchTab(\\'config\\')"]').style.display = 'none';
                document.querySelector('.tab[onclick="switchTab(\\'debug\\')"]').style.display = 'none';
                // Switch to dashboard
                switchTab('dashboard');
                // Ensure auth interval is correct
                setInterval(fetchDashboard, 5000);
            } else {
                setInterval(fetchDashboard, 5000);
                setInterval(fetchDebug, 2000);
            }
        });
"""
# Replace the direct setIntervals in window.onload
html = html.replace('setInterval(fetchDashboard, 5000);\n        setInterval(fetchDebug, 2000);', js_auth.strip())

with open('components/web_server/html/index.html', 'w', encoding='utf-8') as f:
    f.write(html)
