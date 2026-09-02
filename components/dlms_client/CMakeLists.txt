idf_component_register(
    SRCS "dlms_hdlc.c" "dlms_axdr.c" "dlms_helpers.c" "dlms_client.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer log freertos
)

