import re
with open('main/main.c', 'r') as f:
    text = f.read()

text = text.replace('xTaskCreate(e450_push_task, "e450_push", 4096, NULL, 5, NULL);', 'xTaskCreate(e450_push_task, "e450_push", 8192, NULL, 5, NULL);')

with open('main/main.c', 'w') as f:
    f.write(text)
