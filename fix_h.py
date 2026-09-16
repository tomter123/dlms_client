import codecs

with open('components/dlms_client/include/dlms_types.h', 'r', encoding='utf-8') as f:
    c = f.read()

c = c.replace('#define DLMS_OBIS_STR_LEN          20', '#define DLMS_OBIS_STR_LEN          32')
c = c.replace('char       obis_str[24];', 'char       obis_str[32];')

with open('components/dlms_client/include/dlms_types.h', 'w', encoding='utf-8') as f:
    f.write(c)
