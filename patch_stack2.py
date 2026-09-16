import codecs
import re

with open('components/web_server/web_server.c', 'r', encoding='utf-8') as f:
    c = f.read()

c = re.sub(r'char decoded\[1024\];\s*urldecode\(decoded, val\);', r'char *decoded = calloc(1, 4096);\n            urldecode(decoded, val);', c)

c = c.replace('nvs_set_u8(nvs, "mqtt_en", 1);\\n            }\\n        }', 'nvs_set_u8(nvs, "mqtt_en", 1);\\n            }\\n            free(decoded);\\n        }')

with open('components/web_server/web_server.c', 'w', encoding='utf-8') as f:
    f.write(c)
