import codecs

with open('main/main.c', 'r', encoding='utf-8') as f:
    lines = f.readlines()

for i, line in enumerate(lines):
    if 'printf("%s' in line:
        # merge the next line
        if i + 1 < len(lines) and '", rx_buf);' in lines[i+1]:
            lines[i] = '                printf("%s\\\\n", rx_buf);\\n'
            lines[i+1] = ''

with open('main/main.c', 'w', encoding='utf-8') as f:
    f.writelines(lines)
