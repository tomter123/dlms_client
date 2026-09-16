import re
with open('main/main.c', 'r') as f:
    text = f.read()

replacement = """__attribute__((unused)) static void on_dlms_readings(const dlms_reading_t *readings, uint8_t count,
                               bool dlms_connected, uint32_t uptime_s) {
    if (dlms_connected) {
        mqtt_publish_readings(readings, count);
    }
}
"""

text = re.sub(r'__attribute__\(\(unused\)\) static void on_dlms_readings.*?\{.*?\n\}', replacement, text, flags=re.DOTALL)

with open('main/main.c', 'w') as f:
    f.write(text)
