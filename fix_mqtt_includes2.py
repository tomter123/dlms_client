import codecs

with codecs.open('components/mqtt_publisher/mqtt_publisher.c', 'r', 'utf-8') as f:
    c = f.read()

c = c.replace('#include "cJSON.h"\\n#include "nvs.h"\\n#include "nvs_flash.h"', '#include "cJSON.h"\n#include "nvs.h"\n#include "nvs_flash.h"\n')

with codecs.open('components/mqtt_publisher/mqtt_publisher.c', 'w', 'utf-8') as f:
    f.write(c)
