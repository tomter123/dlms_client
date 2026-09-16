import re

with open('main/main.c', 'r') as f:
    text = f.read()

# 1. Add global variables at the top of main.c
globals_code = """
/* Globals for Dashboard & Debugging */
#include "cJSON.h"

#define DEBUG_BUF_SIZE 2048
static uint8_t g_debug_buf[DEBUG_BUF_SIZE];
static size_t g_debug_head = 0;
static size_t g_debug_tail = 0;
static SemaphoreHandle_t g_debug_mutex = NULL;

static dlms_reading_t g_dashboard_readings[DLMS_MAX_OBIS_ENTRIES];
static SemaphoreHandle_t g_readings_mutex = NULL;

void get_dashboard_json(char *buf, size_t max_len) {
    if (!g_readings_mutex) { snprintf(buf, max_len, "{}"); return; }
    
    cJSON *root = cJSON_CreateObject();
    xSemaphoreTake(g_readings_mutex, portMAX_DELAY);
    for (int i = 0; i < DLMS_MAX_OBIS_ENTRIES; i++) {
        if (!g_dashboard_readings[i].valid && g_dashboard_readings[i].data_type == 0) continue;
        
        char key[32];
        strncpy(key, g_dashboard_readings[i].obis_str, sizeof(key));
        for(char *p=key; *p; p++) if(*p=='.') *p='_';
        
        // Add a nice name alias for common ones if we want, but for now just use obis_key
        // For chart, we specifically look for 'power' (1_0_1_7_0_255 or 1_0_2_7_0_255 etc)
        if (strcmp(key, "1_0_1_7_0_255") == 0) {
            cJSON_AddNumberToObject(root, "power", g_dashboard_readings[i].scaled_value);
        }
        
        if (g_dashboard_readings[i].data_type == AXDR_OCTET_STRING || g_dashboard_readings[i].data_type == AXDR_VISIBLE_STRING) {
            cJSON_AddStringToObject(root, key, g_dashboard_readings[i].str_val);
        } else {
            cJSON_AddNumberToObject(root, key, g_dashboard_readings[i].scaled_value);
        }
    }
    xSemaphoreGive(g_readings_mutex);
    
    cJSON_AddNumberToObject(root, "uptime_s", esp_timer_get_time() / 1000000);
    char *json_str = cJSON_PrintUnformatted(root);
    strncpy(buf, json_str, max_len-1);
    free(json_str);
    cJSON_Delete(root);
}

void get_debug_json(char *buf, size_t max_len) {
    if (!g_debug_mutex) { snprintf(buf, max_len, "{\"hex\":\"\"}"); return; }
    
    // Check if clear was requested (dummy clear for now)
    // We'll just read out the buffer
    char *hex_str = malloc(DEBUG_BUF_SIZE * 3 + 1);
    if (!hex_str) { snprintf(buf, max_len, "{}"); return; }
    
    hex_str[0] = '\0';
    int pos = 0;
    
    xSemaphoreTake(g_debug_mutex, portMAX_DELAY);
    size_t curr = g_debug_tail;
    while (curr != g_debug_head && pos < (DEBUG_BUF_SIZE * 3 - 4)) {
        pos += sprintf(hex_str + pos, "%02X ", g_debug_buf[curr]);
        curr = (curr + 1) % DEBUG_BUF_SIZE;
    }
    xSemaphoreGive(g_debug_mutex);
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "hex", hex_str);
    char *json_str = cJSON_PrintUnformatted(root);
    strncpy(buf, json_str, max_len-1);
    free(json_str);
    cJSON_Delete(root);
    free(hex_str);
}
"""
text = text.replace('void e450_push_task(void *pvParameters);', globals_code + '\nvoid e450_push_task(void *pvParameters);')

# 2. Update e450_push_task UART config and buffering
e450_init_old = """
    ESP_LOGI(TAG_PUSH, "Initializing E450 push listener on UART2 (RX=6, 2400 8E1)...");
    uart_config_t uart_config = {
        .baud_rate = 2400,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
"""

e450_init_new = """
    if (!g_debug_mutex) g_debug_mutex = xSemaphoreCreateMutex();
    if (!g_readings_mutex) g_readings_mutex = xSemaphoreCreateMutex();

    uint32_t baud = 2400;
    int databits = 8;
    int stopbits = 1;
    char parity_str[16] = "even";
    
    nvs_handle_t nvs;
    if (nvs_open("config", NVS_READONLY, &nvs) == ESP_OK) {
        nvs_get_u32(nvs, "e450_baud", &baud);
        nvs_get_u32(nvs, "e450_databits", (uint32_t*)&databits);
        nvs_get_u32(nvs, "e450_stopbits", (uint32_t*)&stopbits);
        size_t len = sizeof(parity_str);
        nvs_get_str(nvs, "e450_parity", parity_str, &len);
        nvs_close(nvs);
    }
    
    uart_parity_t parity = UART_PARITY_EVEN;
    if (strcmp(parity_str, "none") == 0) parity = UART_PARITY_DISABLE;
    else if (strcmp(parity_str, "odd") == 0) parity = UART_PARITY_ODD;

    ESP_LOGI(TAG_PUSH, "Initializing E450 push listener on UART1 (RX=6, %lu %d%c%d)...", 
             baud, databits, parity_str[0], stopbits);
             
    uart_config_t uart_config = {
        .baud_rate = baud,
        .data_bits = databits == 7 ? UART_DATA_7_BITS : UART_DATA_8_BITS,
        .parity = parity,
        .stop_bits = stopbits == 2 ? UART_STOP_BITS_2 : UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
"""
text = text.replace(e450_init_old.strip(), e450_init_new.strip())

# 3. Buffer the read bytes
read_old = """
        int len = uart_read_bytes(UART_NUM_1, rx_buf + rx_len, DLMS_RX_BUFFER_SIZE - rx_len, pdMS_TO_TICKS(100));
        if (len > 0) {
"""

read_new = """
        int len = uart_read_bytes(UART_NUM_1, rx_buf + rx_len, DLMS_RX_BUFFER_SIZE - rx_len, pdMS_TO_TICKS(100));
        if (len > 0) {
            xSemaphoreTake(g_debug_mutex, portMAX_DELAY);
            for (int i = 0; i < len; i++) {
                g_debug_buf[g_debug_head] = (rx_buf + rx_len)[i];
                g_debug_head = (g_debug_head + 1) % DEBUG_BUF_SIZE;
                if (g_debug_head == g_debug_tail) {
                    g_debug_tail = (g_debug_tail + 1) % DEBUG_BUF_SIZE;
                }
            }
            xSemaphoreGive(g_debug_mutex);
"""
text = text.replace(read_old.strip(), read_new.strip())

# 4. Save to g_dashboard_readings
save_rdg_old = """
                                // Also update the global struct for MQTT
                                rdg.valid = true;
                                rdg.timestamp_ms = esp_timer_get_time() / 1000;
                                
                                // Publish to MQTT
                                mqtt_publish_readings(&rdg, 1);
"""

save_rdg_new = """
                                // Also update the global struct for MQTT
                                rdg.valid = true;
                                rdg.timestamp_ms = esp_timer_get_time() / 1000;
                                
                                xSemaphoreTake(g_readings_mutex, portMAX_DELAY);
                                bool found = false;
                                for (int k = 0; k < DLMS_MAX_OBIS_ENTRIES; k++) {
                                    if (strcmp(g_dashboard_readings[k].obis_str, rdg.obis_str) == 0) {
                                        g_dashboard_readings[k] = rdg;
                                        found = true; break;
                                    }
                                }
                                if (!found) {
                                    for (int k = 0; k < DLMS_MAX_OBIS_ENTRIES; k++) {
                                        if (g_dashboard_readings[k].data_type == 0) {
                                            g_dashboard_readings[k] = rdg;
                                            break;
                                        }
                                    }
                                }
                                xSemaphoreGive(g_readings_mutex);
                                
                                // Publish to MQTT
                                mqtt_publish_readings(&rdg, 1);
"""
text = text.replace(save_rdg_old.strip(), save_rdg_new.strip())


with open('main/main.c', 'w') as f:
    f.write(text)
