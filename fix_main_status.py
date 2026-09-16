import re
with open('main/main.c', 'r') as f:
    text = f.read()

text = text.replace('web_server_set_status_info(', '// web_server_set_status_info(')

with open('main/main.c', 'w') as f:
    f.write(text)
