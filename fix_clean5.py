import codecs

with open('main/main.c', 'r', encoding='utf-8') as f:
    c = f.read()

c = c.replace('printf("%s\\\\n", rx_buf);\\\\n                rx_len = 0;', 'printf("%s\\\\n", rx_buf);\\n                rx_len = 0;')

with open('main/main.c', 'w', encoding='utf-8') as f:
    f.write(c)
