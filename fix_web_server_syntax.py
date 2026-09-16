import codecs

with open('components/web_server/web_server.c', 'r', encoding='utf-8') as f:
    c = f.read()

c = c.replace('char *buf = malloc(4096);\\n    if(!buf) return ESP_FAIL;', 'char *buf = malloc(4096);\\n    if(!buf) return ESP_FAIL;')
c = c.replace('\\n', '\n') # Wait, this will replace all \n!

