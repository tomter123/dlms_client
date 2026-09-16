import re
with open('main/main.c', 'r') as f:
    text = f.read()

replacement_tasks = """
    ESP_LOGI(TAG, "Initializing web server...");
    extern esp_err_t web_server_init(void);
    web_server_init();

    char meter_type[32] = "e450"; // default
    nvs_handle_t nvs;
    if (nvs_open("config", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(meter_type);
        nvs_get_str(nvs, "meter", meter_type, &len);
        nvs_close(nvs);
    }
    
    ESP_LOGI(TAG, "Selected meter type: %s", meter_type);
    if (strcmp(meter_type, "e570") == 0) {
        // ESP_LOGI(TAG, "Starting E570 DLMS poll task...");
        // xTaskCreate(dlms_client_poll_task, "dlms_poll", 8192, &s_dlms_client, 5, NULL);
    } else {
        ESP_LOGI(TAG, "Starting E450 DLMS push listener task...");
        xTaskCreate(e450_push_task, "e450_push", 8192, NULL, 5, NULL);
    }
"""

text = re.sub(r'ESP_LOGI\(TAG, "Starting E450 DLMS push listener task\.\.\."\);\s*xTaskCreate\(e450_push_task, "e450_push", 8192, NULL, 5, NULL\);', replacement_tasks, text, flags=re.DOTALL)

with open('main/main.c', 'w') as f:
    f.write(text)
