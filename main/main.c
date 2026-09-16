/*
 * DLMS/COSEM Smart Meter Reader - Main Application
 *
 * Standalone ESP-IDF firmware for ESP32-C3 that communicates with
 * DLMS/COSEM electricity meters via RS-485, publishes readings
 * via MQTT and serves a web dashboard.
 *
 * Ported from: https://github.com/latonita/esphome-dlms-cosem
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "wifi_manager.h"
#include "mqtt_publisher.h"
#include "web_server.h"
#include "dlms_client.h"
#include "dlms_types.h"
#include "dlms_hdlc.h"
#include "dlms_axdr.h"

static const char *TAG = "main";


/* Globals for Dashboard & Debugging */
#include "cJSON.h"
#include "esp_timer.h"

#define TRACE_BUF_SIZE 4096
static char g_trace_buf[TRACE_BUF_SIZE];
static int g_trace_head = 0;
static int g_trace_tail = 0;
static portMUX_TYPE s_trace_mux = portMUX_INITIALIZER_UNLOCKED;
static vprintf_like_t s_orig_vprintf = NULL;

static void append_trace_str(const char *str) {
    portENTER_CRITICAL(&s_trace_mux);
    while (*str) {
        g_trace_buf[g_trace_head] = *str++;
        g_trace_head = (g_trace_head + 1) % TRACE_BUF_SIZE;
        if (g_trace_head == g_trace_tail) {
            g_trace_tail = (g_trace_tail + 1) % TRACE_BUF_SIZE;
        }
    }
    portEXIT_CRITICAL(&s_trace_mux);
}

static int trace_vprintf(const char *fmt, va_list args) {
    char buf[256];
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(buf, sizeof(buf), fmt, args_copy);
    va_end(args_copy);
    if (len > 0) {
        if (strstr(buf, "e450") || strstr(buf, "dlms") || strstr(buf, "main") || strstr(buf, "RX:") || strstr(buf, "TX:") || strstr(buf, "Parsed")) {
            append_trace_str(buf);
        }
    }
    if (s_orig_vprintf) return s_orig_vprintf(fmt, args);
    return len;
}

void clear_trace_buf(void) {
    portENTER_CRITICAL(&s_trace_mux);
    g_trace_head = 0;
    g_trace_tail = 0;
    portEXIT_CRITICAL(&s_trace_mux);
}


#define DEBUG_BUF_SIZE 4096
uint8_t g_debug_buf[DEBUG_BUF_SIZE];
volatile int g_debug_head = 0;
volatile int g_debug_tail = 0;
SemaphoreHandle_t g_debug_mutex = NULL;

static dlms_reading_t g_dashboard_readings[DLMS_MAX_OBIS_ENTRIES];
static SemaphoreHandle_t g_readings_mutex = NULL;


void cache_readings(const dlms_reading_t *readings, uint8_t count) {
    if (g_readings_mutex) {
        xSemaphoreTake(g_readings_mutex, portMAX_DELAY);
        for (int i = 0; i < count; i++) {
            bool found = false;
            for (int j = 0; j < DLMS_MAX_OBIS_ENTRIES; j++) {
                if ((g_dashboard_readings[j].valid || g_dashboard_readings[j].data_type != 0) &&
                    strcmp(g_dashboard_readings[j].obis_str, readings[i].obis_str) == 0) {
                    g_dashboard_readings[j] = readings[i];
                    found = true;
                    break;
                }
            }
            if (!found) {
                for (int j = 0; j < DLMS_MAX_OBIS_ENTRIES; j++) {
                    if (!g_dashboard_readings[j].valid && g_dashboard_readings[j].data_type == 0) {
                        g_dashboard_readings[j] = readings[i];
                        break;
                    }
                }
            }
        }
        xSemaphoreGive(g_readings_mutex);
    }
}

dlms_reading_t get_cached_reading(const char *obis) {
    dlms_reading_t rdg = {0};
    if (g_readings_mutex) {
        xSemaphoreTake(g_readings_mutex, portMAX_DELAY);
        for (int i = 0; i < DLMS_MAX_OBIS_ENTRIES; i++) {
            if ((g_dashboard_readings[i].valid || g_dashboard_readings[i].data_type != 0) &&
                strcmp(g_dashboard_readings[i].obis_str, obis) == 0) {
                rdg = g_dashboard_readings[i];
                break;
            }
        }
        xSemaphoreGive(g_readings_mutex);
    }
    return rdg;
}

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


void clear_debug_buf(void) {
    if (g_debug_mutex) {
        xSemaphoreTake(g_debug_mutex, portMAX_DELAY);
        g_debug_head = 0;
        g_debug_tail = 0;
        xSemaphoreGive(g_debug_mutex);
    }
}

void get_debug_json(char *buf, size_t max_len) {
    if (!g_debug_mutex) { snprintf(buf, max_len, "{\"hex\":\"\"}"); return; }
    
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
    
    char *trace_str = malloc(TRACE_BUF_SIZE + 1);
    if (trace_str) {
        portENTER_CRITICAL(&s_trace_mux);
        int tpos = 0;
        int tcurr = g_trace_tail;
        while (tcurr != g_trace_head && tpos < TRACE_BUF_SIZE) {
            trace_str[tpos++] = g_trace_buf[tcurr];
            tcurr = (tcurr + 1) % TRACE_BUF_SIZE;
        }
        trace_str[tpos] = '\0';
        portEXIT_CRITICAL(&s_trace_mux);
        
        // Remove ANSI codes
        for (int i = 0; i < tpos; i++) {
            if (trace_str[i] == '\033') {
                int j = i;
                while (j < tpos && trace_str[j] != 'm') j++;
                if (j < tpos) {
                    memmove(&trace_str[i], &trace_str[j+1], tpos - j);
                    tpos -= (j + 1 - i);
                    i--;
                }
            }
        }
        
        cJSON_AddStringToObject(root, "trace", trace_str);
        free(trace_str);
    }
    
    char *json_str = cJSON_PrintUnformatted(root);
    strncpy(buf, json_str, max_len-1);
    free(json_str);
    cJSON_Delete(root);
    free(hex_str);
}

void e450_push_task(void *pvParameters);
extern void am550_ascii_task(void *pvParameters);
extern void iec62056_21_task(void *pvParameters);

/* â”€â”€â”€ Global DLMS Client Instance â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
__attribute__((unused)) static dlms_client_t s_dlms_client;

/* â”€â”€â”€ DLMS Readings Callback â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
/**
 * Called by the DLMS client task each time a poll cycle completes.
 * Publishes readings to MQTT.
 */
__attribute__((unused)) static void on_dlms_readings(const dlms_reading_t *readings, uint8_t count,
                               bool dlms_connected, uint32_t uptime_s) {
    if (dlms_connected) {
        mqtt_publish_readings(readings, count);
        cache_readings(readings, count);
    }
}


/* â”€â”€â”€ Default OBIS Entries â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
/**
 * Register common electricity meter OBIS codes.
 */
__attribute__((unused)) static void register_default_obis(dlms_client_t *client)
{
    /* â”€â”€ Energy registers â”€â”€ */
    dlms_client_add_obis(client, "1.0.1.8.0.255",
                          42768, -3, "Energy Import Total", "kWh", "energy");
    dlms_client_add_obis(client, "1.0.1.8.1.255",
                          58112, -3, "Energy Import T1", "kWh", "energy");
    dlms_client_add_obis(client, "1.0.1.8.2.255",
                          58208, -3, "Energy Import T2", "kWh", "energy");

    /* đź”Ś Instantaneous power đź”Ś */
    dlms_client_add_obis(client, "1.0.1.7.0.255",
                          22160, 0, "Active Power Total", "W", "power");

    /* âšˇ Voltage (per phase) âšˇ */
    dlms_client_add_obis(client, "1.0.32.7.0.255",
                          29216, 0, "Voltage L1", "V", "voltage");
    dlms_client_add_obis(client, "1.0.52.7.0.255",
                          21776, 0, "Voltage L2", "V", "voltage");
    dlms_client_add_obis(client, "1.0.72.7.0.255",
                          21872, 0, "Voltage L3", "V", "voltage");

}

/* â”€â”€â”€ Application Entry Point â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  DLMS/COSEM Smart Meter Reader");
    ESP_LOGI(TAG, "  Firmware version: 1.0.0");
    ESP_LOGI(TAG, "  Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG, "========================================");

    /* â”€â”€ Initialize NVS â”€â”€ */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* â”€â”€ Initialize event loop â”€â”€ */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* â”€â”€ Initialize Wi-Fi â”€â”€ */
    
    s_orig_vprintf = esp_log_set_vprintf(trace_vprintf);

    ESP_LOGI(TAG, "Initializing Wi-Fi...");
    ESP_ERROR_CHECK(wifi_manager_init());

    /* Wait for Wi-Fi connection (with timeout) */
    EventGroupHandle_t wifi_events = wifi_manager_get_event_group();
        // Don't wait for WiFi, just start the tasks
    // EventBits_t bits = xEventGroupWaitBits...
    nvs_handle_t nvs_mqtt;
    if (nvs_open("config", NVS_READONLY, &nvs_mqtt) == ESP_OK) {
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
        nvs_close(nvs_mqtt);
    }


    /* â”€â”€ Initialize DLMS Client â”€â”€ */
    ESP_LOGI(TAG, "Initializing DLMS client...");

    esp_log_level_set("dlms_client", ESP_LOG_DEBUG);
    esp_log_level_set("dlms_rx", ESP_LOG_DEBUG);

    (void)wifi_events;
    dlms_client_config_t dlms_cfg = {
        .uart_port      = CONFIG_DLMS_UART_PORT_NUM,
        .tx_pin         = CONFIG_DLMS_UART_TX_PIN,
        .rx_pin         = CONFIG_DLMS_UART_RX_PIN,
        .rts_pin        = CONFIG_DLMS_UART_RTS_PIN,
        .baud_rate      = CONFIG_DLMS_UART_BAUD_RATE,
        .client_address = 32,         // Client address 32 (0x20)
        .server_logical = 1,          // Logical address 1
        .server_physical = 4555,      // Physical address 4555
        .server_addr_len = 4,         // 4 bytes required to fit logical 1 + physical 4555
        .auth_mode      = DLMS_AUTH_LOW,
        .poll_interval_sec = CONFIG_DLMS_POLL_INTERVAL_SEC,
        .push_mode_enabled = false,
    };
    (void)dlms_cfg;

    // ESP_ERROR_CHECK(dlms_client_init(&s_dlms_client, &dlms_cfg));
    // register_default_obis(&s_dlms_client);
    // dlms_client_set_callback(&s_dlms_client, on_dlms_readings, NULL);

    ESP_LOGI(TAG, "Initializing web server...");
    // web_server_set_readings_source(s_dlms_client.readings, &s_dlms_client.obis_count, s_dlms_client.readings_mutex);
    // ESP_ERROR_CHECK(web_server_init());

    
    ESP_LOGI(TAG, "Initializing web server...");
    extern esp_err_t web_server_init(void);
    web_server_init();

    char meter_type[32] = "e450"; // default
    nvs_handle_t nvs_type;
    if (nvs_open("config", NVS_READONLY, &nvs_type) == ESP_OK) {
        size_t len = sizeof(meter_type);
        nvs_get_str(nvs_type, "meter_type", meter_type, &len);
        nvs_close(nvs_type);
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
            nvs_get_str(nvs_dlms, "auth", auth, &len);
            len = 64; nvs_get_str(nvs_dlms, "pass", pass, &len);
            nvs_close(nvs_dlms);
            
            dlms_client_config_t cfg = {
                .uart_port = UART_NUM_1,
                .client_address = (uint8_t)client_addr_u32,
                .server_logical = (uint16_t)server_logical_u32,
                .server_physical = (uint16_t)server_physical_u32,
                .auth_mode = strcmp(auth, "low") == 0 ? DLMS_AUTH_LOW : DLMS_AUTH_NONE
            };
            if (cfg.auth_mode == DLMS_AUTH_LOW) {
                strncpy(cfg.password, pass, sizeof(cfg.password) - 1);
            }
            dlms_client_init(&s_dlms_client, &cfg);
        } else {
            // Default config
            dlms_client_config_t cfg = {
                .uart_port = UART_NUM_1,
                .client_address = 0x10,
                .server_logical = 0x0001,
                .server_physical = 0x0011,
                .auth_mode = DLMS_AUTH_NONE
            };
            dlms_client_init(&s_dlms_client, &cfg);
        }
        xTaskCreate(dlms_client_poll_task, "dlms_poll", 8192, &s_dlms_client, 5, NULL);
    } else if (strcmp(meter_type, "e450") == 0) {
        ESP_LOGI(TAG, "Starting E450 DLMS push listener task...");
        xTaskCreate(e450_push_task, "e450_push", 8192, NULL, 5, NULL);
    } else if (strcmp(meter_type, "am550_ascii") == 0) {
        ESP_LOGI(TAG, "Starting AM550 ASCII Push listener task...");
        xTaskCreate(am550_ascii_task, "am550_ascii", 8192, NULL, 5, NULL);
    } else if (strcmp(meter_type, "iec62056_21") == 0) {
        ESP_LOGI(TAG, "Starting IEC62056-21 poll task...");
        xTaskCreate(iec62056_21_task, "iec62056_21", 8192, NULL, 5, NULL);
    }


    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  System ready!");
    ESP_LOGI(TAG, "  MQTT: %s", CONFIG_MQTT_BROKER_URI);
    ESP_LOGI(TAG, "========================================");
}

static const char *TAG_PUSH = "e450_push";

void e450_push_task(void *pvParameters) {
    static uint8_t push_obis_list[30][6];
    static int push_obis_count = 0;
    static int push_value_idx = 1; // Offset by 1 because definition 1 (Push setup) has no value
    
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
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, DLMS_RX_BUFFER_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, 6, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    gpio_pullup_en((gpio_num_t)6);

    uint8_t *rx_buf = malloc(DLMS_RX_BUFFER_SIZE);
    size_t rx_len = 0;

    while (1) {
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
            rx_len += len;
            ESP_LOGI(TAG_PUSH, "Received %d bytes. Buffer len: %d", len, rx_len);
            ESP_LOG_BUFFER_HEX(TAG_PUSH, rx_buf, rx_len);

            // Try to find HDLC or MBUS frame
            size_t start = 0, end = 0;
            bool frame_found = false;
            bool is_mbus = false;

            // 1. Try HDLC
            if (dlms_hdlc_find_frame(rx_buf, rx_len, &start, &end) == 0) {
                frame_found = true;
            } else {
                // 2. Try M-Bus (68 LL LL 68 ... CS 16)
                for (size_t i = 0; i < rx_len; i++) {
                    if (rx_buf[i] == 0x68 && (rx_len - i) >= 6) {
                        uint8_t L1 = rx_buf[i+1];
                        uint8_t L2 = rx_buf[i+2];
                        if (L1 == L2 && rx_buf[i+3] == 0x68) {
                            size_t frame_len = 4 + L1 + 2; // header + payload + CS + 16
                            if (rx_len - i >= frame_len) {
                                if (rx_buf[i + frame_len - 1] == 0x16) {
                                    start = i;
                                    end = i + frame_len;
                                    frame_found = true;
                                    is_mbus = true;
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            if (frame_found) {
                if (is_mbus) {
                    ESP_LOGI(TAG_PUSH, "Found M-Bus frame from %u to %u", start, end);
                    size_t mbus_payload_len = rx_buf[start+1]; // L field
                    if (mbus_payload_len >= 3) {
                        uint8_t c = rx_buf[start+4];
                        uint8_t a = rx_buf[start+5];
                        uint8_t ci = rx_buf[start+6];
                        ESP_LOGI(TAG_PUSH, "M-Bus Control: %02X, Addr: %02X, CI: %02X", c, a, ci);
                        ESP_LOGI(TAG_PUSH, "M-Bus Payload:");
                        ESP_LOG_BUFFER_HEX(TAG_PUSH, rx_buf + start + 7, mbus_payload_len - 3);
                    }
                } else {
                    ESP_LOGI(TAG_PUSH, "Found HDLC frame from %u to %u", start, end);
                    dlms_hdlc_frame_t frame;
                    
                      if (dlms_hdlc_parse_frame(rx_buf + start, end - start, &frame)) {
                          ESP_LOGI(TAG_PUSH, "Parsed Push Frame! Control: %02X, InfoLen: %u", frame.control, frame.info_len);
                          if (frame.info_len > 4) {
                                                            // Skip the proprietary 4-byte header (e.g. 02 00 00 6C or 03 00 00 5F)
                              uint8_t *ptr = frame.info + 4;
                              size_t remain = frame.info_len - 4;
                              
                                                            // If it starts with a DataNotification (0x0F), skip the invoke-ID (4 bytes) and datetime (13 bytes including length)
                              if (remain > 18 && ptr[0] == 0x0F) {
                                  ptr += 18;
                                  remain -= 18;
                              }
                              
                              // If it starts with Struct and Array of definitions (e.g. 02 0D 01 0D), skip those 4 bytes
                              if (remain > 4 && ptr[0] == 0x02 && ptr[2] == 0x01) {
                                  ptr += 4;
                                  remain -= 4;
                              }


                              
                              while (remain > 0) {
                                  // Is it a definition? (0x02 0x04 0x12 ...)
                                  if (remain >= 18 && ptr[0] == 0x02 && ptr[1] == 0x04 && ptr[2] == 0x12) {
                                        if (push_value_idx > 1) { // We have parsed values, so this must be a new cycle
                                            // New push cycle started
                                            push_obis_count = 0;
                                            push_value_idx = 1;
                                        }
                                      
                                      // Extract OBIS
                                      if (ptr[5] == 0x09 && ptr[6] == 0x06) {
                                          uint8_t obis[6];
                                          memcpy(obis, ptr + 7, 6);
                                          char obis_str[32];
                                          sprintf(obis_str, "%u.%u.%u.%u.%u.%u", obis[0], obis[1], obis[2], obis[3], obis[4], obis[5]);
                                          ESP_LOGI(TAG_PUSH, "Parsed Definition: %s", obis_str);
                                          
                                          // Add to list
                                          if (push_obis_count < 30) {
                                              memcpy(push_obis_list[push_obis_count], obis, 6);
                                              push_obis_count++;
                                          }
                                      }
                                      ptr += 18;
                                      remain -= 18;
                                  } else {
                                      // It's a value!
                                      dlms_reading_t rdg;
                                      memset(&rdg, 0, sizeof(rdg));
                                      if (push_value_idx < push_obis_count) {
                                          memcpy(rdg.obis, push_obis_list[push_value_idx], 6);
                                      }
                                      
                                      int consumed = axdr_parse_value(ptr, remain, &rdg);
                                      if (consumed <= 0) {
                                          ESP_LOGE(TAG_PUSH, "Failed to parse A-XDR value, breaking");
                                          break;
                                      }
                                      
                                      if (push_value_idx < push_obis_count) {
                                          sprintf(rdg.obis_str, "%u.%u.%u.%u.%u.%u", rdg.obis[0], rdg.obis[1], rdg.obis[2], rdg.obis[3], rdg.obis[4], rdg.obis[5]);
                                          
                                          if (rdg.data_type == AXDR_OCTET_STRING || rdg.data_type == AXDR_VISIBLE_STRING || rdg.data_type == AXDR_UTF8_STRING) {
                                              ESP_LOGI(TAG_PUSH, "Value %s: %s", rdg.obis_str, rdg.str_val ? rdg.str_val : "null");
                                          } else {
                                              if (rdg.data_type == AXDR_FLOAT32 || rdg.data_type == AXDR_FLOAT64) {
                                                  rdg.scaled_value = rdg.float_val;
                                              } else {
                                                  rdg.scaled_value = (float)rdg.int_val;
                                              }
                                              ESP_LOGI(TAG_PUSH, "Value %s: %.2f", rdg.obis_str, rdg.scaled_value);
                                          }
                                          // Publish to MQTT
                                          mqtt_publish_readings(&rdg, 1);
                                          cache_readings(&rdg, 1);
                                      }
                                      
                                      push_value_idx++;
                                      ptr += consumed;
                                      remain -= consumed;
                                  }
                              }
                          }
                      } else {
                          ESP_LOGW(TAG_PUSH, "Failed to parse HDLC frame");
                      }

                }
                                  // Remove parsed frame from buffer, keeping the trailing flag for back-to-back frames
                  memmove(rx_buf, rx_buf + end - 1, rx_len - (end - 1));
                  rx_len -= (end - 1);
            } else if (rx_len == DLMS_RX_BUFFER_SIZE) {
                ESP_LOGW(TAG_PUSH, "Buffer full without valid frame, clearing");
                rx_len = 0;
            }
        } else if (rx_len > 0) {
            // Timeout, maybe it's ASCII? Check if it starts with '/'
            if (rx_buf[0] == '/') {
                ESP_LOGI(TAG_PUSH, "ASCII Message detected:");
                rx_buf[rx_len] = 0;
                printf("%s\\n", rx_buf);
                rx_len = 0;
            }
        }
    }
}



