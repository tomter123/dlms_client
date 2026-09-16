import re
with open('main/main.c', 'r') as f:
    text = f.read()

text = text.replace('push_value_idx = 0;', 'push_value_idx = 1;')

with open('main/main.c', 'w') as f:
    f.write(text)
