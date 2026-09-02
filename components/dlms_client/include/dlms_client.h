/*
 * DLMS/COSEM Client - High-Level API
 *
 * Manages the full DLMS communication lifecycle:
 * SNRM → UA → AARQ → AARE → GET-Requests → DISC
 *
 * Runs as a FreeRTOS task, publishes readings via callback.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "dlms_types.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Client Configuration ───────────────────────────────────────── */
typedef struct {
    uart_port_t   uart_port;
    int           tx_pin;
    int           rx_pin;
    int           rts_pin;           /* RS-485 direction control */
    uint32_t      baud_rate;

    uint8_t       client_address;    /* e.g. 16 (Public), 1 (Management) */
    uint16_t      server_logical;    /* Meter logical device address */
    uint16_t      server_physical;   /* Meter physical device address */
    uint8_t       server_addr_len;   /* 1, 2, or 4 */

    dlms_auth_t   auth_mode;
    char          password[DLMS_MAX_PASSWORD_LEN];

    uint32_t      poll_interval_sec;
    bool          push_mode_enabled;

    uint32_t      receive_timeout_ms;
    uint32_t      inter_frame_delay_ms;
} dlms_client_config_t;

/* ─── Callback type for new readings ─────────────────────────────── */
typedef void (*dlms_reading_callback_t)(const dlms_reading_t *readings,
                                        uint8_t count,
                                        void *user_ctx);

/* ─── Client Handle ──────────────────────────────────────────────── */
typedef struct {
    /* Configuration (set during init) */
    dlms_client_config_t config;

    /* UART state */
    uart_port_t   uart_port;

    /* Protocol state */
    dlms_state_t  state;
    uint8_t       send_seq;         /* N(S) for I-frames */
    uint8_t       recv_seq;         /* N(R) for I-frames */
    uint8_t       retry_count;

    /* Buffers */
    uint8_t       rx_buffer[DLMS_RX_BUFFER_SIZE];
    size_t        rx_len;
    uint8_t       tx_buffer[DLMS_TX_BUFFER_SIZE];

    /* Datablock reassembly */
    uint8_t       block_buffer[DLMS_RX_BUFFER_SIZE];
    size_t        block_len;
    uint32_t      block_number;

    /* Registered OBIS entries to poll */
    dlms_obis_entry_t obis_entries[DLMS_MAX_OBIS_ENTRIES];
    uint8_t       obis_count;
    uint8_t       current_obis_index;

    /* Latest readings (thread-safe) */
    dlms_reading_t readings[DLMS_MAX_OBIS_ENTRIES];
    SemaphoreHandle_t readings_mutex;

    /* Callback for new readings */
    dlms_reading_callback_t on_readings_updated;
    void         *callback_ctx;

    /* Statistics */
    uint32_t      polls_completed;
    uint32_t      errors_total;
    int64_t       last_poll_time_ms;
} dlms_client_t;

/* ─── Public API ─────────────────────────────────────────────────── */

/**
 * Initialize the DLMS client: configure UART, allocate resources.
 * Must be called before any other function.
 */
esp_err_t dlms_client_init(dlms_client_t *client, const dlms_client_config_t *config);

/**
 * Add an OBIS code to poll. Can be called multiple times before starting tasks.
 * Add an OBIS code to the polling list.
 */
esp_err_t dlms_client_add_obis(dlms_client_t *client,
                                const char *obis_str,
                                uint16_t short_name,
                                int8_t scaler,
                                const char *name,
                                const char *unit_str,
                                const char *device_class);

/**
 * Set callback for when readings are updated after a poll cycle.
 */
void dlms_client_set_callback(dlms_client_t *client,
                               dlms_reading_callback_t cb,
                               void *user_ctx);

/**
 * Get a copy of a specific reading by OBIS string.
 * Thread-safe (acquires mutex).
 * Returns ESP_OK if found, ESP_ERR_NOT_FOUND otherwise.
 */
esp_err_t dlms_client_get_reading(dlms_client_t *client,
                                   const char *obis_str,
                                   dlms_reading_t *out);

/**
 * Get copies of all current readings.
 * Thread-safe (acquires mutex).
 * Returns number of readings copied.
 */
uint8_t dlms_client_get_all_readings(dlms_client_t *client,
                                      dlms_reading_t *out,
                                      uint8_t max_count);

/**
 * FreeRTOS task function for polling mode.
 * Runs the SNRM→UA→AARQ→AARE→GET→DISC cycle on each poll interval.
 * Pass dlms_client_t* as pvParameters.
 */
void dlms_client_poll_task(void *pvParameters);

/**
 * FreeRTOS task function for push mode.
 * Continuously listens for incoming HDLC frames from the meter.
 * Pass dlms_client_t* as pvParameters.
 */
void dlms_client_push_task(void *pvParameters);

/**
 * Get current client state as a string.
 */
const char *dlms_client_get_state_str(const dlms_client_t *client);

#ifdef __cplusplus
}
#endif

