with open('components/web_server/CMakeLists.txt', 'r') as f:
    text = f.read()
text = text.replace('REQUIRES esp_http_server log nvs_flash', 'REQUIRES esp_http_server log nvs_flash mbedtls')
with open('components/web_server/CMakeLists.txt', 'w') as f:
    f.write(text)
