import codecs

with open('main/main.c', 'r', encoding='utf-8') as f:
    c = f.read()

c = c.replace('\\n\\n', '\\n')
c = c.replace('\\n', '\\n')

c = c.replace('\\ndlms_reading_t get_cached_reading(const char *obis) {', '\\ndlms_reading_t get_cached_reading(const char *obis) {')
c = c.replace('mqtt_publish_readings(readings, count);\\n        cache_readings(readings, count);', 'mqtt_publish_readings(readings, count);\\n        cache_readings(readings, count);')

# Wait, let's just do a clean regex fix for the stray \\n
import re
c = re.sub(r'\\\\n\s*dlms_reading_t', r'\\ndlms_reading_t', c)
c = re.sub(r';\\\\n\s*cache_readings', r';\\ncache_readings', c)

# Let's just find \n in the file and replace with \n
c = c.replace(r'\n', '\\n')

with open('main/main.c', 'w', encoding='utf-8') as f:
    f.write(c)
