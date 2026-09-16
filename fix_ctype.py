import re
with open('components/web_server/web_server.c', 'r') as f:
    text = f.read()
text = text.replace('#include <sys/param.h>', '#include <sys/param.h>\n#include <ctype.h>')
with open('components/web_server/web_server.c', 'w') as f:
    f.write(text)

with open('main/main.c', 'r') as f:
    text = f.read()
text = text.replace('static dlms_client_t s_dlms_client;', '__attribute__((unused)) static dlms_client_t s_dlms_client;')
with open('main/main.c', 'w') as f:
    f.write(text)
