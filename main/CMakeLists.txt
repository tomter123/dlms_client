idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES dlms_client wifi_manager mqtt_publisher web_server
             nvs_flash esp_wifi esp_event esp_netif driver esp_timer log
)

