import re
with open('main/main.c', 'r') as f:
    text = f.read()

text = text.replace('#include "dlms_hdlc.h"', '#include "dlms_hdlc.h"\n#include "dlms_axdr.h"')
text = text.replace('mqtt_publish_reading(&rdg);', 'mqtt_publish_readings(&rdg, 1);')

with open('main/main.c', 'w') as f:
    f.write(text)
