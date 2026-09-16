import re
with open('components/wifi_manager/wifi_manager.c', 'r') as f:
    text = f.read()

# Hardcode MakerLab wifi
old_logic = """
    wifi_config_t wifi_config_sta = {0};
    bool has_sta_config = false;
    
    nvs_handle_t nvs;
    if (nvs_open("config", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(wifi_config_sta.sta.ssid);
        if (nvs_get_str(nvs, "ssid", (char*)wifi_config_sta.sta.ssid, &len) == ESP_OK && len > 1) {
            len = sizeof(wifi_config_sta.sta.password);
            nvs_get_str(nvs, "pass", (char*)wifi_config_sta.sta.password, &len);
            has_sta_config = true;
        }
        nvs_close(nvs);
    }
"""

new_logic = """
    wifi_config_t wifi_config_sta = {0};
    bool has_sta_config = true;
    strcpy((char*)wifi_config_sta.sta.ssid, "MakerLab");
    strcpy((char*)wifi_config_sta.sta.password, "FrankoMerkatori");
"""
text = text.replace(old_logic.strip(), new_logic.strip())

with open('components/wifi_manager/wifi_manager.c', 'w') as f:
    f.write(text)
