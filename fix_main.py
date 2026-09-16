import re
with open('main/main.c', 'r') as f:
    text = f.read()

# Replace the MQTT init block
mqtt_old = """
    // Start MQTT in background if possible, or handle it inside the wifi event handler
    ESP_LOGI(TAG, "Initializing MQTT...");
    ESP_ERROR_CHECK(mqtt_publisher_init());
    ESP_ERROR_CHECK(mqtt_publisher_start());
"""

mqtt_new = """
    nvs_handle_t nvs_mqtt;
    if (nvs_open("config", NVS_READONLY, &nvs_mqtt) == ESP_OK) {
        uint8_t mqtt_en = 0;
        nvs_get_u8(nvs_mqtt, "mqtt_en", &mqtt_en);
        if (mqtt_en) {
            char mqtt_host[64] = {0};
            char mqtt_user[64] = {0};
            char mqtt_pass[64] = {0};
            size_t len = 64;
            
            nvs_get_str(nvs_mqtt, "mqtt_host", mqtt_host, &len); len = 64;
            nvs_get_str(nvs_mqtt, "mqtt_user", mqtt_user, &len); len = 64;
            nvs_get_str(nvs_mqtt, "mqtt_pass", mqtt_pass, &len); len = 64;
            
            uint32_t mqtt_port = 1883;
            nvs_get_u32(nvs_mqtt, "mqtt_port", &mqtt_port);
            
            char broker_uri[128];
            snprintf(broker_uri, sizeof(broker_uri), "mqtt://%s:%lu", mqtt_host, mqtt_port);
            
            ESP_LOGI(TAG, "Initializing MQTT to %s...", broker_uri);
            mqtt_publisher_init(broker_uri, 
                                strlen(mqtt_user) > 0 ? mqtt_user : NULL, 
                                strlen(mqtt_pass) > 0 ? mqtt_pass : NULL, 
                                "dlms/meter");
            mqtt_publisher_start();
        }
        nvs_close(nvs_mqtt);
    }
"""
text = text.replace(mqtt_old, mqtt_new)

# E570 DLMS config update inside main()
dlms_poll_old = """
    if (strcmp(meter_type, "e570") == 0) {
        // ESP_LOGI(TAG, "Starting E570 DLMS poll task...");
        // xTaskCreate(dlms_client_poll_task, "dlms_poll", 8192, &s_dlms_client, 5, NULL);
    }
"""

dlms_poll_new = """
    if (strcmp(meter_type, "e570") == 0) {
        ESP_LOGI(TAG, "Starting E570 DLMS poll task...");
        nvs_handle_t nvs_dlms;
        if (nvs_open("config", NVS_READONLY, &nvs_dlms) == ESP_OK) {
            uint32_t baud = 9600;
            uint16_t client_addr = 32;
            uint16_t server_logical = 1;
            uint16_t server_physical = 17;
            char auth[16] = "low";
            char pass[64] = {0};
            size_t len = 16;
            
            nvs_get_u32(nvs_dlms, "dlms_baud", &baud);
            nvs_get_u16(nvs_dlms, "dlms_client", &client_addr);
            nvs_get_u16(nvs_dlms, "dlms_server_logical", &server_logical);
            nvs_get_u16(nvs_dlms, "dlms_server_physical", &server_physical);
            nvs_get_str(nvs_dlms, "dlms_auth", auth, &len); len = 64;
            nvs_get_str(nvs_dlms, "dlms_pass", pass, &len);
            nvs_close(nvs_dlms);
            
            ESP_LOGI(TAG, "E570 Config: Baud=%lu, Client=%u, ServerL=%u, ServerP=%u, Auth=%s", baud, client_addr, server_logical, server_physical, auth);
            
            // Set up UART correctly (use the dynamic baud rate)
            uart_set_baudrate(UART_NUM_2, baud);
            
            dlms_client_config_t cfg = {
                .uart_num = UART_NUM_2,
                .client_addr = client_addr,
                .server_logical_addr = server_logical,
                .server_physical_addr = server_physical,
                .auth_level = strcmp(auth, "low") == 0 ? 1 : (strcmp(auth, "high") == 0 ? 2 : 0)
            };
            strncpy(cfg.password, pass, sizeof(cfg.password)-1);
            
            dlms_client_init(&s_dlms_client, &cfg);
            // xTaskCreate(dlms_client_poll_task, "dlms_poll", 8192, &s_dlms_client, 5, NULL);
        }
    }
"""
text = text.replace(dlms_poll_old.strip(), dlms_poll_new.strip())

with open('main/main.c', 'w') as f:
    f.write(text)
