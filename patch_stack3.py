import codecs
import re

with open('components/web_server/web_server.c', 'r', encoding='utf-8') as f:
    c = f.read()

c = re.sub(r'nvs_set_u8\(nvs, "mqtt_en", 1\);\s*\}\s*\}', r'nvs_set_u8(nvs, "mqtt_en", 1);\n            }\n            free(decoded);\n        }', c)

with open('components/web_server/web_server.c', 'w', encoding='utf-8') as f:
    f.write(c)
