idf_component_register(
    SRCS "mqtt_publisher.c"
    INCLUDE_DIRS "include"
    REQUIRES mqtt log esp_timer json dlms_client
)
