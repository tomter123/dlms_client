with open('components/web_server/CMakeLists.txt', 'r') as f:
    text = f.read()
text = text.replace('esp_http_server nvs_flash', 'esp_http_server nvs_flash mbedtls')
with open('components/web_server/CMakeLists.txt', 'w') as f:
    f.write(text)
