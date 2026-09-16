import codecs
with open('components/web_server/web_server.c', 'r', encoding='utf-8') as f:
    c = f.read()

c = c.replace('#include "cJSON.h"', '// removed cjson')

with open('components/web_server/web_server.c', 'w', encoding='utf-8') as f:
    f.write(c)
