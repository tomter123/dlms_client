import codecs

with open('main/main.c', 'r', encoding='utf-8') as f:
    c = f.read()

# Replace MQTT init block
mqtt_old = """
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
"""
mqtt_new = """
        uint8_t mqtt_en = 0;
        nvs_get_u8(nvs_mqtt, "mqtt_en", &mqtt_en);
        if (mqtt_en) {
            char mqtt_uri[128] = "mqtt://192.168.0.100:1883";
            char mqtt_topic[64] = "dlms/meter";
            char mqtt_user[64] = {0};
            char mqtt_pass[64] = {0};
            size_t len = 128;
            nvs_get_str(nvs_mqtt, "mqtt_uri", mqtt_uri, &len); 
            len = 64; nvs_get_str(nvs_mqtt, "mqtt_topic", mqtt_topic, &len);
            len = 64; nvs_get_str(nvs_mqtt, "mqtt_user", mqtt_user, &len);
            len = 64; nvs_get_str(nvs_mqtt, "mqtt_pass", mqtt_pass, &len);
            
            ESP_LOGI(TAG, "Initializing MQTT to %s...", mqtt_uri);
            mqtt_publisher_init(mqtt_uri, 
                                strlen(mqtt_user) > 0 ? mqtt_user : NULL, 
                                strlen(mqtt_pass) > 0 ? mqtt_pass : NULL, 
                                mqtt_topic);
            mqtt_publisher_start();
        }
"""
if mqtt_old.strip() in c:
    c = c.replace(mqtt_old.strip(), mqtt_new.strip())
else:
    print("MQTT block not found!")

# Replace meter type and DLMS config block
meter_old = """
    char meter_type[32] = "e450"; // default
    nvs_handle_t nvs;
    if (nvs_open("config", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(meter_type);
        nvs_get_str(nvs, "meter", meter_type, &len);
        nvs_close(nvs);
    }
    
    ESP_LOGI(TAG, "Selected meter type: %s", meter_type);
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
"""
meter_new = """
    char meter_type[32] = "e450"; // default
    nvs_handle_t nvs;
    if (nvs_open("config", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(meter_type);
        nvs_get_str(nvs, "meter_type", meter_type, &len);
        nvs_close(nvs);
    }
    
    ESP_LOGI(TAG, "Selected meter type: %s", meter_type);
    if (strcmp(meter_type, "e570") == 0) {
        ESP_LOGI(TAG, "Starting E570 DLMS poll task...");
        nvs_handle_t nvs_dlms;
        if (nvs_open("config", NVS_READONLY, &nvs_dlms) == ESP_OK) {
            uint32_t baud = 9600;
            uint32_t client_addr_u32 = 16;
            uint32_t server_logical_u32 = 1;
            uint32_t server_physical_u32 = 17;
            char auth[16] = "none";
            char pass[64] = {0};
            size_t len = 16;
            
            nvs_get_u32(nvs_dlms, "baud", &baud);
            nvs_get_u32(nvs_dlms, "client_addr", &client_addr_u32);
            nvs_get_u32(nvs_dlms, "server_logical", &server_logical_u32);
            nvs_get_u32(nvs_dlms, "server_physical", &server_physical_u32);
            nvs_get_str(nvs_dlms, "auth", auth, &len); len = 64;
            nvs_get_str(nvs_dlms, "pass", pass, &len);
            nvs_close(nvs_dlms);
            
            uint16_t client_addr = client_addr_u32;
            uint16_t server_logical = server_logical_u32;
            uint16_t server_physical = server_physical_u32;
"""
if meter_old.strip() in c:
    c = c.replace(meter_old.strip(), meter_new.strip())
else:
    print("Meter block not found!")

with open('main/main.c', 'w', encoding='utf-8') as f:
    f.write(c)
