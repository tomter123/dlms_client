/*
 * ESP-IDF Logging Shim for PC builds
 *
 * Maps ESP_LOGx macros to printf so the same DLMS protocol code
 * compiles on both ESP32 and PC without changes.
 */
#pragma once

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* Log levels matching ESP-IDF */
typedef enum {
    ESP_LOG_NONE = 0,
    ESP_LOG_ERROR,
    ESP_LOG_WARN,
    ESP_LOG_INFO,
    ESP_LOG_DEBUG,
    ESP_LOG_VERBOSE
} esp_log_level_t;

/* Global log level - can be changed at runtime */
#ifndef PC_LOG_LEVEL
#define PC_LOG_LEVEL ESP_LOG_DEBUG
#endif

#define ESP_LOGE(tag, fmt, ...) do { \
    if (PC_LOG_LEVEL >= ESP_LOG_ERROR) \
        fprintf(stderr, "\033[31m[E][%s] " fmt "\033[0m\n", tag, ##__VA_ARGS__); \
} while(0)

#define ESP_LOGW(tag, fmt, ...) do { \
    if (PC_LOG_LEVEL >= ESP_LOG_WARN) \
        fprintf(stderr, "\033[33m[W][%s] " fmt "\033[0m\n", tag, ##__VA_ARGS__); \
} while(0)

#define ESP_LOGI(tag, fmt, ...) do { \
    if (PC_LOG_LEVEL >= ESP_LOG_INFO) \
        printf("\033[32m[I][%s] " fmt "\033[0m\n", tag, ##__VA_ARGS__); \
} while(0)

#define ESP_LOGD(tag, fmt, ...) do { \
    if (PC_LOG_LEVEL >= ESP_LOG_DEBUG) \
        printf("\033[36m[D][%s] " fmt "\033[0m\n", tag, ##__VA_ARGS__); \
} while(0)

#define ESP_LOGV(tag, fmt, ...) do { \
    if (PC_LOG_LEVEL >= ESP_LOG_VERBOSE) \
        printf("[V][%s] " fmt "\n", tag, ##__VA_ARGS__); \
} while(0)

/* Hex dump helper matching ESP-IDF's ESP_LOG_BUFFER_HEXDUMP */
static inline void ESP_LOG_BUFFER_HEXDUMP(const char *tag, const void *data,
                                           size_t len, esp_log_level_t level)
{
    if ((int)level > PC_LOG_LEVEL) return;

    const uint8_t *bytes = (const uint8_t *)data;
    printf("[%s] hex dump (%zu bytes):\n", tag, len);

    for (size_t i = 0; i < len; i += 16) {
        printf("  %04zx: ", i);
        /* Hex bytes */
        for (size_t j = 0; j < 16; j++) {
            if (i + j < len)
                printf("%02x ", bytes[i + j]);
            else
                printf("   ");
            if (j == 7) printf(" ");
        }
        /* ASCII */
        printf(" |");
        for (size_t j = 0; j < 16 && (i + j) < len; j++) {
            uint8_t c = bytes[i + j];
            printf("%c", (c >= 0x20 && c < 0x7f) ? c : '.');
        }
        printf("|\n");
    }
}
