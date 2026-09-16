with open('main/main.c', 'rb') as f:
    data = f.read()
data = data.replace(b"hex_str[0] = '\x00';", b"hex_str[0] = '\\0';")
with open('main/main.c', 'wb') as f:
    f.write(data)
