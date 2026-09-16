#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "dlms_types.h"

extern void mqtt_publish_readings(const dlms_reading_t *readings, uint8_t count);
extern void cache_readings(const dlms_reading_t *readings, uint8_t count);

extern SemaphoreHandle_t g_debug_mutex;
extern uint8_t g_debug_buf[];
extern volatile int g_debug_head;
extern volatile int g_debug_tail;
#define DEBUG_BUF_SIZE 4096

static void parse_text_line(char *line) {
    char *start = strchr(line, '(');
    if (!start) return;
    
    *start = '\0';
    char *obis_str = line;
    
    char *val_str = start + 1;
    char *end = strchr(val_str, '*');
    if (!end) end = strchr(val_str, ')');
    if (end) *end = '\0';
    
    float val = atof(val_str);
    
    uint8_t obis[6] = {0,0,0,0,0,255};
    int a=0, b=0, c=0, d=0, e=0, f=255;
    int scanned = sscanf(obis_str, "%d-%d:%d.%d.%d.%d", &a, &b, &c, &d, &e, &f);
    if (scanned < 5) return;
    
    obis[0]=a; obis[1]=b; obis[2]=c; obis[3]=d; obis[4]=e; obis[5]=f;
    
    dlms_reading_t rdg;
    memset(&rdg, 0, sizeof(rdg));
    memcpy(rdg.obis, obis, 6);
    sprintf(rdg.obis_str, "%u.%u.%u.%u.%u.%u", obis[0], obis[1], obis[2], obis[3], obis[4], obis[5]);
    rdg.scaled_value = val;
    rdg.float_val = val;
    rdg.data_type = 0x17; // AXDR_FLOAT32
    rdg.valid = true;
    
    ESP_LOGI("ASCII_PARSER", "Parsed %s: %.3f", rdg.obis_str, rdg.scaled_value);
    mqtt_publish_readings(&rdg, 1);
    cache_readings(&rdg, 1);
}

void am550_ascii_task(void *pvParameters) {
    if (!g_debug_mutex) g_debug_mutex = xSemaphoreCreateMutex();
    uint32_t baud = 115200;
    nvs_handle_t nvs;
    if (nvs_open("config", NVS_READONLY, &nvs) == ESP_OK) {
        nvs_get_u32(nvs, "am550_baud", &baud);
        nvs_close(nvs);
    }
    
    uart_config_t uart_config = {
        .baud_rate = baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 2048, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, 2, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    uint8_t *rx_buf = malloc(1024);
    size_t rx_len = 0;
    
    while(1) {
        int len = uart_read_bytes(UART_NUM_1, rx_buf + rx_len, 1024 - rx_len - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            xSemaphoreTake(g_debug_mutex, portMAX_DELAY);
            for (int i = 0; i < len; i++) {
                g_debug_buf[g_debug_head] = (rx_buf + rx_len)[i];
                g_debug_head = (g_debug_head + 1) % DEBUG_BUF_SIZE;
                if (g_debug_head == g_debug_tail) g_debug_tail = (g_debug_tail + 1) % DEBUG_BUF_SIZE;
            }
            xSemaphoreGive(g_debug_mutex);
            
            rx_len += len;
            rx_buf[rx_len] = '\0';
            
            char *line_start = (char*)rx_buf;
            char *line_end;
            while ((line_end = strchr(line_start, '\n')) != NULL) {
                *line_end = '\0';
                if (line_end > line_start && *(line_end - 1) == '\r') *(line_end - 1) = '\0';
                parse_text_line(line_start);
                line_start = line_end + 1;
            }
            
            int consumed = line_start - (char*)rx_buf;
            if (consumed > 0) {
                memmove(rx_buf, line_start, rx_len - consumed);
                rx_len -= consumed;
            } else if (rx_len >= 1000) {
                rx_len = 0;
            }
        }
    }
}

void iec62056_21_task(void *pvParameters) {
    if (!g_debug_mutex) g_debug_mutex = xSemaphoreCreateMutex();
    uint32_t init_baud = 300;
    uint32_t target_baud = 9600;
    nvs_handle_t nvs;
    if (nvs_open("config", NVS_READONLY, &nvs) == ESP_OK) {
        nvs_get_u32(nvs, "iec_baud", &init_baud);
        nvs_get_u32(nvs, "iec_target_baud", &target_baud);
        nvs_close(nvs);
    }
    
    uart_config_t uart_config = {
        .baud_rate = init_baud,
        .data_bits = UART_DATA_7_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 2048, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, 5, 4, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    uint8_t *rx_buf = malloc(1024);
    size_t rx_len = 0;
    
    while(1) {
        uart_flush(UART_NUM_1);
        uart_set_baudrate(UART_NUM_1, init_baud);
        const char *wakeup = "/?!\r\n";
        uart_write_bytes(UART_NUM_1, wakeup, 5);
        ESP_LOGI("IEC62056", "Sent wakeup /?!");
        
        vTaskDelay(pdMS_TO_TICKS(1000));
        int len = uart_read_bytes(UART_NUM_1, rx_buf, 256, pdMS_TO_TICKS(2000));
        if (len > 0) {
            rx_buf[len] = '\0';
            ESP_LOGI("IEC62056", "Got ID: %s", rx_buf);
            
            xSemaphoreTake(g_debug_mutex, portMAX_DELAY);
            for(int i=0; i<len; i++) {
                g_debug_buf[g_debug_head] = rx_buf[i];
                g_debug_head = (g_debug_head + 1) % DEBUG_BUF_SIZE;
                if(g_debug_head == g_debug_tail) g_debug_tail = (g_debug_tail + 1) % DEBUG_BUF_SIZE;
            }
            xSemaphoreGive(g_debug_mutex);
            
            char baud_char = '5';
            char *slash = strchr((char*)rx_buf, '/');
            if (slash && strlen(slash) >= 5) baud_char = slash[4];
            
            char ack[6] = {0x06, '0', baud_char, '0', '\r', '\n'};
            uart_write_bytes(UART_NUM_1, ack, 6);
            uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(100));
            
            uint32_t real_target = target_baud;
            if (baud_char == '0') real_target = 300;
            else if (baud_char == '1') real_target = 600;
            else if (baud_char == '2') real_target = 1200;
            else if (baud_char == '3') real_target = 2400;
            else if (baud_char == '4') real_target = 4800;
            else if (baud_char == '5') real_target = 9600;
            else if (baud_char == '6') real_target = 19200;
            
            ESP_LOGI("IEC62056", "Switching to %lu baud", real_target);
            uart_set_baudrate(UART_NUM_1, real_target);
            
            rx_len = 0;
            TickType_t start_ticks = xTaskGetTickCount();
            while((xTaskGetTickCount() - start_ticks) < pdMS_TO_TICKS(5000)) {
                len = uart_read_bytes(UART_NUM_1, rx_buf + rx_len, 1024 - rx_len - 1, pdMS_TO_TICKS(100));
                if (len > 0) {
                    xSemaphoreTake(g_debug_mutex, portMAX_DELAY);
                    for(int i=0; i<len; i++) {
                        g_debug_buf[g_debug_head] = (rx_buf+rx_len)[i];
                        g_debug_head = (g_debug_head + 1) % DEBUG_BUF_SIZE;
                        if(g_debug_head == g_debug_tail) g_debug_tail = (g_debug_tail + 1) % DEBUG_BUF_SIZE;
                    }
                    xSemaphoreGive(g_debug_mutex);
                    
                    rx_len += len;
                    rx_buf[rx_len] = '\0';
                    
                    char *line_start = (char*)rx_buf;
                    char *line_end;
                    while ((line_end = strchr(line_start, '\n')) != NULL) {
                        *line_end = '\0';
                        if (line_end > line_start && *(line_end - 1) == '\r') *(line_end - 1) = '\0';
                        parse_text_line(line_start);
                        line_start = line_end + 1;
                    }
                    int consumed = line_start - (char*)rx_buf;
                    if (consumed > 0) {
                        memmove(rx_buf, line_start, rx_len - consumed);
                        rx_len -= consumed;
                    }
                    if (strchr((char*)rx_buf, '!')) break;
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

