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

#include "wifi_manager.h"
#include "mqtt_publisher.h"
#include "web_server.h"
#include "dlms_client.h"
#include "dlms_types.h"

static const char *TAG = "main";

/* ─── Global DLMS Client Instance ────────────────────────────────── */
static dlms_client_t s_dlms_client;

/* ─── DLMS Readings Callback ─────────────────────────────────────── */
/**
 * Called by the DLMS client task each time a poll cycle completes.
 * Publishes readings to MQTT.
 */
static void on_dlms_readings(const dlms_reading_t *readings, uint8_t count,
                              void *user_ctx)
{
    ESP_LOGI(TAG, "Received %u readings from DLMS client", count);

    /* Publish to MQTT if connected */
    if (mqtt_publisher_is_connected()) {
        esp_err_t err = mqtt_publish_readings(readings, count);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "MQTT publish failed: %s", esp_err_to_name(err));
        }
    }

    /* Update web server status */
    web_server_set_status_info(
        dlms_client_get_state_str(&s_dlms_client),
        wifi_manager_get_rssi(),
        mqtt_publisher_is_connected()
    );
}

/* ─── Default OBIS Entries ───────────────────────────────────────── */
/**
 * Register common electricity meter OBIS codes.
 */
static void register_default_obis(dlms_client_t *client)
{
    /* ── Energy registers ── */
    dlms_client_add_obis(client, "1.0.1.8.0.255",
                          42768, -3, "Energy Import Total", "kWh", "energy");
    dlms_client_add_obis(client, "1.0.1.8.1.255",
                          58112, -3, "Energy Import T1", "kWh", "energy");
    dlms_client_add_obis(client, "1.0.1.8.2.255",
                          58208, -3, "Energy Import T2", "kWh", "energy");

    /* 🔌 Instantaneous power 🔌 */
    dlms_client_add_obis(client, "1.0.1.7.0.255",
                          22160, 0, "Active Power Total", "W", "power");

    /* ⚡ Voltage (per phase) ⚡ */
    dlms_client_add_obis(client, "1.0.32.7.0.255",
                          29216, 0, "Voltage L1", "V", "voltage");
    dlms_client_add_obis(client, "1.0.52.7.0.255",
                          21776, 0, "Voltage L2", "V", "voltage");
    dlms_client_add_obis(client, "1.0.72.7.0.255",
                          21872, 0, "Voltage L3", "V", "voltage");

}

/* ─── Application Entry Point ────────────────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  DLMS/COSEM Smart Meter Reader");
    ESP_LOGI(TAG, "  Firmware version: 1.0.0");
    ESP_LOGI(TAG, "  Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG, "========================================");

    /* ── Initialize NVS ── */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* ── Initialize event loop ── */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* ── Initialize Wi-Fi ── */
    ESP_LOGI(TAG, "Initializing Wi-Fi...");
    ESP_ERROR_CHECK(wifi_manager_init());

    /* Wait for Wi-Fi connection (with timeout) */
    EventGroupHandle_t wifi_events = wifi_manager_get_event_group();
    EventBits_t bits = xEventGroupWaitBits(wifi_events,
                                            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                            pdFALSE, pdFALSE,
                                            pdMS_TO_TICKS(30000));

    if (bits & WIFI_CONNECTED_BIT) {
        char ip_str[16];
        wifi_manager_get_ip_str(ip_str, sizeof(ip_str));
        ESP_LOGI(TAG, "Wi-Fi connected! IP: %s", ip_str);
    } else {
        ESP_LOGW(TAG, "Wi-Fi connection failed - continuing without network");
        /* DLMS communication will still work, just no MQTT/HTTP */
    }

    /* ── Initialize MQTT ── */
    ESP_LOGI(TAG, "Initializing MQTT...");
    ESP_ERROR_CHECK(mqtt_publisher_init());
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_ERROR_CHECK(mqtt_publisher_start());
    }

    /* ── Initialize DLMS Client ── */
    ESP_LOGI(TAG, "Initializing DLMS client...");

    esp_log_level_set("dlms_client", ESP_LOG_DEBUG);
    esp_log_level_set("dlms_rx", ESP_LOG_DEBUG);

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
#ifdef CONFIG_DLMS_PUSH_MODE_ENABLED
        .push_mode_enabled = true,
#else
        .push_mode_enabled = false,
#endif
        .receive_timeout_ms = CONFIG_DLMS_RECEIVE_TIMEOUT_MS,
        .inter_frame_delay_ms = CONFIG_DLMS_INTER_FRAME_DELAY_MS,
    };

    strncpy(dlms_cfg.password, "00000000", sizeof(dlms_cfg.password) - 1);

    ESP_ERROR_CHECK(dlms_client_init(&s_dlms_client, &dlms_cfg));

    /* Register default OBIS codes */
    register_default_obis(&s_dlms_client);

    /* Set callback for readings updates */
    dlms_client_set_callback(&s_dlms_client, on_dlms_readings, NULL);

    /* ── Initialize Web Server ── */
    ESP_LOGI(TAG, "Initializing web server...");
    web_server_set_readings_source(
        s_dlms_client.readings,
        &s_dlms_client.obis_count,
        s_dlms_client.readings_mutex
    );
    ESP_ERROR_CHECK(web_server_init());

    /* ── Publish HA Discovery (after MQTT connects) ── */
    /* Give MQTT a moment to connect, then publish discovery */
    vTaskDelay(pdMS_TO_TICKS(2000));
    if (mqtt_publisher_is_connected()) {
        mqtt_publish_ha_discovery(s_dlms_client.obis_entries,
                                   s_dlms_client.obis_count);
        ESP_LOGI(TAG, "Home Assistant MQTT discovery published");
    }

    /* ── Start DLMS Communication Tasks ── */
    ESP_LOGI(TAG, "Starting DLMS poll task...");
    xTaskCreate(dlms_client_poll_task, "dlms_poll",
                8192, &s_dlms_client, 5, NULL);

#ifdef CONFIG_DLMS_PUSH_MODE_ENABLED
    ESP_LOGI(TAG, "Starting DLMS push listener task...");
    xTaskCreate(dlms_client_push_task, "dlms_push",
                4096, &s_dlms_client, 4, NULL);
#endif

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  System ready!");
    ESP_LOGI(TAG, "  Polling every %d seconds", CONFIG_DLMS_POLL_INTERVAL_SEC);
    ESP_LOGI(TAG, "  MQTT: %s", CONFIG_MQTT_BROKER_URI);
    if (bits & WIFI_CONNECTED_BIT) {
        char ip_str[16];
        wifi_manager_get_ip_str(ip_str, sizeof(ip_str));
        ESP_LOGI(TAG, "  Web dashboard: http://%s/", ip_str);
    }
    ESP_LOGI(TAG, "========================================");

    /* Main task is done - FreeRTOS tasks handle everything */
}

