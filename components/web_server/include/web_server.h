#pragma once
#include <esp_err.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t web_server_init(void);

// Implemented in main.c, called by web_server.c
void get_dashboard_json(char *buf, size_t max_len);
void get_debug_json(char *buf, size_t max_len);

#ifdef __cplusplus
}
#endif
