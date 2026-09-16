idf_component_register(
    SRCS "mqtt_publisher.c"
    INCLUDE_DIRS "include"
    REQUIRES mqtt nvs_flash log esp_timer json dlms_client
)
