with open('components/web_server/web_server.c', 'r', encoding='utf-8') as f:
    text = f.read()

import re

# find the auth block
m = re.search(r'(#include "mbedtls/base64\.h".*?static esp_err_t api_role_handler.*?return ESP_OK;\n})', text, re.DOTALL)
if m:
    auth_block = m.group(1)
    # remove it from the top
    text = text.replace(auth_block, '', 1)
    # insert it after #include <ctype.h>
    text = text.replace('#include <ctype.h>', '#include <ctype.h>\n' + auth_block)
    
    with open('components/web_server/web_server.c', 'w', encoding='utf-8') as f:
        f.write(text)
