import codecs
with open('components/web_server/web_server.c', 'r', encoding='utf-8') as f:
    c = f.read()

c = c.replace('void web_server_init(void) {', 'esp_err_t web_server_init(void) {')
c = c.replace('httpd_register_uri_handler(server, &uri_role);\\n    }\\n}', 'httpd_register_uri_handler(server, &uri_role);\\n    }\\n    return ESP_OK;\\n}')

with open('components/web_server/web_server.c', 'w', encoding='utf-8') as f:
    f.write(c)
