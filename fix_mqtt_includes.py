import codecs

with codecs.open('components/mqtt_publisher/mqtt_publisher.c', 'r', 'utf-8') as f:
    c = f.read()

c = c.replace('#include "cJSON.h"', '#include "cJSON.h"\\n#include "nvs.h"\\n#include "nvs_flash.h"')

with codecs.open('components/mqtt_publisher/mqtt_publisher.c', 'w', 'utf-8') as f:
    f.write(c)

with codecs.open('components/mqtt_publisher/CMakeLists.txt', 'r', 'utf-8') as f:
    c2 = f.read()

if 'nvs_flash' not in c2:
    c2 = c2.replace('REQUIRES mqtt', 'REQUIRES mqtt nvs_flash')
    with codecs.open('components/mqtt_publisher/CMakeLists.txt', 'w', 'utf-8') as f:
        f.write(c2)
