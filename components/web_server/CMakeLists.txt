idf_component_register(
    SRCS "web_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server log nvs_flash mbedtls
    EMBED_FILES "html/index.html"
)
