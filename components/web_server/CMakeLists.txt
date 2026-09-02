idf_component_register(
    SRCS "web_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server log esp_timer json dlms_client
    EMBED_FILES "web/index.html"
)
