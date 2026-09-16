import codecs

with open('main/main.c', 'r', encoding='utf-8') as f:
    c = f.read()

# I will literally replace backslash followed by n
c = c.replace('\\\\n', '\\n')

with open('main/main.c', 'w', encoding='utf-8') as f:
    f.write(c)
