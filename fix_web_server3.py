import codecs

with open('components/web_server/web_server.c', 'r', encoding='utf-8') as f:
    c = f.read()

# Let's clean the literal '\n' that are stray.
c = c.replace('char *buf = malloc(4096);\\\\n    if(!buf) return ESP_FAIL;', 'char *buf = malloc(4096);\\n    if(!buf) return ESP_FAIL;')
c = c.replace('if (remaining >= 4096) {\\\\n        free(buf);', 'if (remaining >= 4096) {\\n        free(buf);')
c = c.replace('if ((ret = httpd_req_recv(req, buf, remaining)) <= 0) {\\\\n        free(buf);', 'if ((ret = httpd_req_recv(req, buf, remaining)) <= 0) {\\n        free(buf);')
c = c.replace('free(buf);\\\\n    ESP_LOGI(TAG, "Rebooting in 1s...");', 'free(buf);\\n    ESP_LOGI(TAG, "Rebooting in 1s...");')

with open('components/web_server/web_server.c', 'w', encoding='utf-8') as f:
    f.write(c)

