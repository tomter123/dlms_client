import re
with open('main/main.c', 'r') as f:
    text = f.read()

text = text.replace('static void on_dlms_readings(', '__attribute__((unused)) static void on_dlms_readings(')
text = text.replace('static void register_default_obis(', '__attribute__((unused)) static void register_default_obis(')

with open('main/main.c', 'w') as f:
    f.write(text)
