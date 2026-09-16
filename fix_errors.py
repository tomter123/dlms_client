import re
with open('main/main.c', 'r') as f:
    text = f.read()
text = text.replace('#include "cJSON.h"', '#include "cJSON.h"\n#include "esp_timer.h"')
text = text.replace('"{\\"hex\\":\\"\\"}"', '"{\\"hex\\":\\"\\"}"')
text = text.replace('"{"hex":""}"', '"{\\"hex\\":\\"\\"}"')

with open('main/main.c', 'w') as f:
    f.write(text)
