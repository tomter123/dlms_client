/*
 * DLMS/COSEM Client Implementation
 *
 * Core protocol engine implementing the full DLMS communication lifecycle:
 *   SNRM → UA → AARQ → AARE → GET-Request(s) → DISC
 *
 * Ported from esphome-dlms-cosem to standalone ESP-IDF.
 *
 * SPDX-License-Identifier: MIT
 */

#include "dlms_client.h"
#include "dlms_hdlc.h"
#include "dlms_axdr.h"
#include "dlms_helpers.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include <string.h>
#include <math.h>

static const char *TAG = "dlms_client";

#define MAX_RETRIES          3
#define UART_RX_BUF_SIZE     2048
#define UART_TX_BUF_SIZE     512
#define UART_QUEUE_SIZE      20

/* ─── AARQ Application Context OIDs ─────────────────────────────── */
/* SN referencing without ciphering: 2.16.756.5.8.1.2 */
static const uint8_t APP_CONTEXT_SN[] = {
    0x06, 0x07, 0x60, 0x85, 0x74, 0x05, 0x08, 0x01, 0x02
};

/* Authentication Mechanism Name: Low level security OID: 2.16.756.5.8.2.1 */
static const uint8_t AUTH_MECHANISM_LOW_SN[] = {
    0x60, 0x85, 0x74, 0x05, 0x08, 0x02, 0x01
};

/* ─── Internal Helpers ───────────────────────────────────────────── */

static int64_t get_millis(void)
{
    return esp_timer_get_time() / 1000LL;
}

/**
 * Send raw bytes over UART with RS-485 direction control.
 */
static esp_err_t uart_send(dlms_client_t *client, const uint8_t *data, size_t len)
{
    ESP_LOG_BUFFER_HEXDUMP("dlms_tx", data, len, ESP_LOG_DEBUG);

    int written = uart_write_bytes(client->uart_port, (const char *)data, len);
    if (written < 0) {
        ESP_LOGE(TAG, "UART write failed");
        return ESP_FAIL;
    }
    /* Wait for all bytes to be physically transmitted (important for RS-485) */
    esp_err_t err = uart_wait_tx_done(client->uart_port, pdMS_TO_TICKS(200));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "uart_wait_tx_done timeout");
    }
    return ESP_OK;
}

/**
 * Receive bytes with timeout. Returns number of bytes read.
 */
static int uart_receive(dlms_client_t *client, uint8_t *buf, size_t max_len,
                         uint32_t timeout_ms)
{
    int len = uart_read_bytes(client->uart_port, buf, max_len,
                              pdMS_TO_TICKS(timeout_ms));
    if (len > 0) {
        ESP_LOG_BUFFER_HEXDUMP("dlms_rx", buf, len, ESP_LOG_DEBUG);
    }
    return len;
}

/**
 * Flush any pending bytes in UART RX buffer.
 */
static void uart_flush_rx(dlms_client_t *client)
{
    uart_flush_input(client->uart_port);
}

/**
 * Wait for a complete HDLC frame response from the meter.
 * Accumulates bytes until a valid frame is found between 0x7E markers.
 * Returns true if a valid frame was received and parsed.
 */
static bool wait_for_frame(dlms_client_t *client, dlms_hdlc_frame_t *frame,
                            uint32_t timeout_ms)
{
    uint8_t tmp[256];
    int64_t start = get_millis();
    client->rx_len = 0;

    while ((get_millis() - start) < timeout_ms) {
        int n = uart_receive(client, tmp, sizeof(tmp),
                              timeout_ms > 100 ? 100 : timeout_ms);
        if (n > 0) {
            /* Append to rx_buffer */
            size_t space = DLMS_RX_BUFFER_SIZE - client->rx_len;
            size_t copy = (size_t)n < space ? (size_t)n : space;
            memcpy(client->rx_buffer + client->rx_len, tmp, copy);
            client->rx_len += copy;

            /* Try to find a complete frame */
            size_t frame_start, frame_end;
            if (dlms_hdlc_find_frame(client->rx_buffer, client->rx_len,
                                      &frame_start, &frame_end) == 0) {
                /* Parse the frame */
                const uint8_t *fdata = client->rx_buffer + frame_start;
                size_t flen = frame_end - frame_start;

                if (dlms_hdlc_parse_frame(fdata, flen, frame)) {
                    ESP_LOGD(TAG, "Frame received: ctrl=0x%02X info_len=%u",
                             frame->control, (unsigned)frame->info_len);
                    return true;
                } else {
                    ESP_LOGW(TAG, "Frame parse failed (CRC error?)");
                    /* Discard consumed bytes and keep looking */
                    if (frame_end < client->rx_len) {
                        memmove(client->rx_buffer,
                                client->rx_buffer + frame_end,
                                client->rx_len - frame_end);
                        client->rx_len -= frame_end;
                    } else {
                        client->rx_len = 0;
                    }
                }
            }
        }
    }

    ESP_LOGW(TAG, "Frame receive timeout (%lu ms)", (unsigned long)timeout_ms);
    return false;
}

/* ─── AARQ (Application Association Request) Builder ─────────────── */

/**
 * Build an AARQ APDU for establishing a DLMS application association.
 * Supports NONE and LOW authentication.
 * Returns the length of the AARQ APDU written to `out`.
 */
static size_t build_aarq_sn(uint8_t *out, size_t max_len,
                             dlms_auth_t auth, const char *password)
{
    size_t pos = 0;
    size_t aarq_len_pos;

    /* AARQ tag */
    out[pos++] = DLMS_TAG_AARQ;

    /* Reserve space for AARQ length (will fill later) */
    aarq_len_pos = pos;
    out[pos++] = 0; /* placeholder */

    size_t content_start = pos;

    /* ── Application Context Name (Tag 0xA1) ── */
    out[pos++] = 0xA1;
    out[pos++] = (uint8_t)sizeof(APP_CONTEXT_SN);
    memcpy(out + pos, APP_CONTEXT_SN, sizeof(APP_CONTEXT_SN));
    pos += sizeof(APP_CONTEXT_SN);

    /* ── Authentication (only for LOW auth) ── */
    if (auth == DLMS_AUTH_LOW && password) {
        out[pos++] = 0x8A; /* [10] sender-acse-requirements IMPLICIT BIT STRING */
        out[pos++] = 0x02;
        out[pos++] = 0x07;
        out[pos++] = 0x80;

        out[pos++] = 0x8B; /* [11] mechanism-name IMPLICIT */
        out[pos++] = sizeof(AUTH_MECHANISM_LOW_SN);
        memcpy(out + pos, AUTH_MECHANISM_LOW_SN, sizeof(AUTH_MECHANISM_LOW_SN));
        pos += sizeof(AUTH_MECHANISM_LOW_SN);

        size_t pw_len = strlen(password);
        out[pos++] = 0xAC; /* [12] calling-authentication-value */
        out[pos++] = pw_len + 2;
        out[pos++] = 0x80; /* [0] IMPLICIT charstring (GraphicString) */
        out[pos++] = pw_len;
        memcpy(out + pos, password, pw_len);
        pos += pw_len;
    }

    out[pos++] = 0xBE; /* [30] user-information EXPLICIT */
    out[pos++] = 0x10; /* length */
    
    /* Exact payload from Gurux Trace: 04 0E 01 00 00 00 06 5F 1F 04 00 1C 03 20 FF FF */
    uint8_t user_info_bytes[] = {
        0x04, 0x0E, 0x01, 0x00, 0x00, 0x00, 0x06, 0x5F, 
        0x1F, 0x04, 0x00, 0x1C, 0x03, 0x20, 0xFF, 0xFF
    };
    memcpy(out + pos, user_info_bytes, sizeof(user_info_bytes));
    pos += sizeof(user_info_bytes);

    /* Fill in AARQ length */
    out[aarq_len_pos] = (uint8_t)(pos - content_start);

    return pos;
}

/* ─── Read-Request Builder ───────────────────────────────────────── */

/**
 * Build a ReadRequest for a Short Name reference.
 */
static size_t build_read_request_sn(uint8_t *out, size_t max_len, uint16_t short_name)
{
    size_t pos = 0;
    out[pos++] = 0x05; /* ReadRequest */
    out[pos++] = 0x01; /* 1 block / item */
    out[pos++] = 0x02; /* variable_name */
    
    out[pos++] = (uint8_t)(short_name >> 8);
    out[pos++] = (uint8_t)(short_name & 0xFF);
    
    return pos;
}

/* ─── Response Handlers ──────────────────────────────────────────── */

/**
 * Check if AARE response indicates accepted association.
 */
static bool parse_aare(const uint8_t *apdu, size_t len)
{
    if (len < 2 || apdu[0] != DLMS_TAG_AARE) {
        ESP_LOGE(TAG, "Not an AARE response (tag=0x%02X)", len > 0 ? apdu[0] : 0);
        return false;
    }

    /* Scan for association-result (tag 0xA2) */
    size_t pos = 2; /* skip tag + length */
    while (pos + 2 < len) {
        uint8_t tag = apdu[pos];
        uint8_t tlen = apdu[pos + 1];

        if (tag == 0xA2) {
            /* association-result: should contain INTEGER = 0 (accepted) */
            if (pos + 2 + tlen <= len && tlen >= 3) {
                uint8_t result = apdu[pos + 4]; /* value after tag(02) + len(01) */
                if (result == 0) {
                    ESP_LOGI(TAG, "AARE: Association accepted");
                    return true;
                } else {
                    ESP_LOGE(TAG, "AARE: Association rejected (result=%u)", result);
                    return false;
                }
            }
        }
        pos += 2 + tlen;
    }

    ESP_LOGE(TAG, "AARE: Could not find association-result");
    return false;
}

/**
 * Handle ReadResponse for Short Name.
 * Returns 0 on success, -1 on error.
 */
static int handle_read_response_sn(dlms_client_t *client, const uint8_t *apdu,
                                   size_t len, dlms_reading_t *reading)
{
    /* Expecting: 0x0C 0x01 <DataAccessResult> OR 0x0C 0x01 0x00 <AXDR data> */
    if (len < 3 || apdu[0] != 0x0C || apdu[1] != 0x01) {
        ESP_LOGE(TAG, "Not a valid ReadResponse");
        return -1;
    }

    uint8_t result = apdu[2];
    if (result != 0x00) {
        ESP_LOGW(TAG, "ReadResponse error: result=0x%02X", result);
        return -1;
    }

    /* Parse A-XDR data starting at apdu[3] */
    const uint8_t *data = apdu + 3;
    size_t data_len = len - 3;

    int consumed = axdr_parse_value(data, data_len, reading);
    if (consumed < 0) {
        ESP_LOGW(TAG, "A-XDR parse failed");
        return -1;
    }

    reading->valid = true;
    reading->timestamp_ms = get_millis();
    return 0;
}

/* ─── Public API Implementation ──────────────────────────────────── */

esp_err_t dlms_client_init(dlms_client_t *client, const dlms_client_config_t *config)
{
    memset(client, 0, sizeof(dlms_client_t));
    memcpy(&client->config, config, sizeof(dlms_client_config_t));

    client->uart_port = config->uart_port;
    client->state = DLMS_STATE_IDLE;

    /* Create readings mutex */
    client->readings_mutex = xSemaphoreCreateMutex();
    if (client->readings_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create readings mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Configure UART */
    uart_config_t uart_config = {
        .baud_rate  = (int)config->baud_rate,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err;

    err = uart_driver_install(config->uart_port,
                               UART_RX_BUF_SIZE, UART_TX_BUF_SIZE,
                               UART_QUEUE_SIZE, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_param_config(config->uart_port, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART config failed: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_set_pin(config->uart_port,
                        5, 4, // Hardcoded TX=5, RX=4 to match user request
                        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART set pin failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Since we are using an auto-direction RS485 chip, just use standard UART mode */
    err = uart_set_mode(config->uart_port, UART_MODE_UART);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART mode failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "DLMS client initialized: UART%d TX=5 RX=4 RTS=NONE @ %lu baud",
             config->uart_port, (unsigned long)config->baud_rate);
    ESP_LOGI(TAG, "Client addr=0x%02X, Server logical=%u physical=%u (len=%u)",
             config->client_address, config->server_logical,
             config->server_physical, config->server_addr_len);
    ESP_LOGI(TAG, "Auth: %s, Poll interval: %lu s",
             config->auth_mode == DLMS_AUTH_LOW ? "LOW" : "NONE",
             (unsigned long)config->poll_interval_sec);

    return ESP_OK;
}

esp_err_t dlms_client_add_obis(dlms_client_t *client,
                                const char *obis_str,
                                uint16_t short_name,
                                int8_t scaler,
                                const char *name,
                                const char *unit_str,
                                const char *device_class)
{
    if (client->obis_count >= DLMS_MAX_OBIS_ENTRIES) {
        ESP_LOGE(TAG, "Max OBIS entries reached (%d)", DLMS_MAX_OBIS_ENTRIES);
        return ESP_ERR_NO_MEM;
    }

    dlms_obis_entry_t *entry = &client->obis_entries[client->obis_count];
    memset(entry, 0, sizeof(dlms_obis_entry_t));

    dlms_obis_str_to_bytes(obis_str, entry->obis);
    strncpy(entry->obis_str, obis_str, sizeof(entry->obis_str) - 1);
    entry->short_name = short_name;
    entry->scaler = scaler;
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    strncpy(entry->unit_str, unit_str ? unit_str : "", sizeof(entry->unit_str) - 1);
    strncpy(entry->device_class, device_class ? device_class : "",
            sizeof(entry->device_class) - 1);

    /* Initialize the corresponding reading slot */
    dlms_reading_t *reading = &client->readings[client->obis_count];
    memset(reading, 0, sizeof(dlms_reading_t));
    memcpy(reading->obis, entry->obis, 6);
    strncpy(reading->obis_str, obis_str, sizeof(reading->obis_str) - 1);
    strncpy(reading->name, name, sizeof(reading->name) - 1);

    client->obis_count++;

    ESP_LOGI(TAG, "Added OBIS %s [%s] sn=%u",
             obis_str, name, short_name);

    return ESP_OK;
}

void dlms_client_set_callback(dlms_client_t *client,
                               dlms_reading_callback_t cb,
                               void *user_ctx)
{
    client->on_readings_updated = cb;
    client->callback_ctx = user_ctx;
}

esp_err_t dlms_client_get_reading(dlms_client_t *client,
                                   const char *obis_str,
                                   dlms_reading_t *out)
{
    uint8_t obis[6];
    if (!dlms_obis_str_to_bytes(obis_str, obis)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(client->readings_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    for (uint8_t i = 0; i < client->obis_count; i++) {
        if (dlms_obis_match(client->readings[i].obis, obis)) {
            memcpy(out, &client->readings[i], sizeof(dlms_reading_t));
            xSemaphoreGive(client->readings_mutex);
            return ESP_OK;
        }
    }

    xSemaphoreGive(client->readings_mutex);
    return ESP_ERR_NOT_FOUND;
}

uint8_t dlms_client_get_all_readings(dlms_client_t *client,
                                      dlms_reading_t *out,
                                      uint8_t max_count)
{
    if (xSemaphoreTake(client->readings_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return 0;
    }

    uint8_t count = client->obis_count < max_count ? client->obis_count : max_count;
    memcpy(out, client->readings, count * sizeof(dlms_reading_t));

    xSemaphoreGive(client->readings_mutex);
    return count;
}

const char *dlms_client_get_state_str(const dlms_client_t *client)
{
    return dlms_state_to_str(client->state);
}

/* ─── Single Poll Cycle ──────────────────────────────────────────── */

/**
 * Execute one complete poll cycle:
 *   1. SNRM → UA (HDLC link setup)
 *   2. AARQ → AARE (DLMS association)
 *   3. For each OBIS: GET-Request → GET-Response
 *   4. DISC → UA/DM (disconnect)
 */
static void do_poll_cycle(dlms_client_t *client)
{
    dlms_hdlc_frame_t frame;
    uint8_t apdu[256];
    size_t apdu_len;
    size_t tx_len;

    const dlms_client_config_t *cfg = &client->config;

    ESP_LOGI(TAG, "=== Poll cycle #%lu starting ===",
             (unsigned long)(client->polls_completed + 1));

    /* Reset protocol state */
    client->send_seq = 0;
    client->recv_seq = 0;
    client->block_len = 0;
    uart_flush_rx(client);

    /* ── Step 1: SNRM → UA ── */
    client->state = DLMS_STATE_CONNECTING_HDLC;
    ESP_LOGI(TAG, "Sending SNRM...");

    tx_len = dlms_hdlc_build_snrm(client->tx_buffer, DLMS_TX_BUFFER_SIZE,
                                    cfg->client_address,
                                    cfg->server_logical,
                                    cfg->server_physical,
                                    cfg->server_addr_len);
    if (tx_len == 0) {
        ESP_LOGE(TAG, "Failed to build SNRM frame");
        goto error;
    }

    for (int retry = 0; retry < MAX_RETRIES; retry++) {
        uart_send(client, client->tx_buffer, tx_len);

        if (wait_for_frame(client, &frame, cfg->receive_timeout_ms)) {
            if (frame.control == HDLC_CTRL_UA ||
                (frame.control & 0xEF) == 0x63) { /* UA with various P/F bits */
                ESP_LOGI(TAG, "UA received - HDLC link established");
                goto hdlc_connected;
            } else if (frame.control == HDLC_CTRL_DM) {
                ESP_LOGW(TAG, "DM received (meter disconnected mode)");
                goto error;
            } else {
                ESP_LOGW(TAG, "Unexpected frame ctrl=0x%02X, retrying", frame.control);
            }
        } else {
            ESP_LOGW(TAG, "SNRM retry %d/%d", retry + 1, MAX_RETRIES);
        }
    }
    ESP_LOGE(TAG, "SNRM failed after %d retries", MAX_RETRIES);
    goto error;

hdlc_connected:

    /* ── Step 2: AARQ → AARE ── */
    client->state = DLMS_STATE_CONNECTING_DLMS;
    ESP_LOGI(TAG, "Sending AARQ (auth=%s)...",
             cfg->auth_mode == DLMS_AUTH_LOW ? "LOW" : "NONE");

    apdu_len = build_aarq_sn(apdu, sizeof(apdu), cfg->auth_mode, cfg->password);

    tx_len = dlms_hdlc_build_iframe(client->tx_buffer, DLMS_TX_BUFFER_SIZE,
                                     cfg->client_address,
                                     cfg->server_logical,
                                     cfg->server_physical,
                                     cfg->server_addr_len,
                                     client->send_seq,
                                     client->recv_seq,
                                     apdu, apdu_len);
    client->send_seq = (client->send_seq + 1) & 0x07;

    uart_send(client, client->tx_buffer, tx_len);

    if (!wait_for_frame(client, &frame, cfg->receive_timeout_ms)) {
        ESP_LOGE(TAG, "No response to AARQ");
        goto disconnect;
    }

    /* Update receive sequence from response */
    if (frame.is_iframe) {
        client->recv_seq = (frame.send_seq + 1) & 0x07;
    }

    /* Parse AARE */
    if (!parse_aare(frame.info, frame.info_len)) {
        ESP_LOGE(TAG, "Association failed");
        goto disconnect;
    }

    client->state = DLMS_STATE_CONNECTED;
    ESP_LOGI(TAG, "DLMS association established");

    /* ── Step 3: GET-Request for each OBIS entry ── */
    for (uint8_t i = 0; i < client->obis_count; i++) {
        dlms_obis_entry_t *entry = &client->obis_entries[i];
        dlms_reading_t reading;
        memset(&reading, 0, sizeof(reading));
        memcpy(reading.obis, entry->obis, 6);
        strncpy(reading.obis_str, entry->obis_str, sizeof(reading.obis_str) - 1);
        strncpy(reading.name, entry->name, sizeof(reading.name) - 1);

        ESP_LOGI(TAG, "Reading OBIS %s [%s]...", entry->obis_str, entry->name);
        client->state = DLMS_STATE_REQUEST_SENT;
        client->current_obis_index = i;

        /* Build ReadRequest for SN */
        apdu_len = build_read_request_sn(apdu, sizeof(apdu), entry->short_name);

        tx_len = dlms_hdlc_build_iframe(client->tx_buffer, DLMS_TX_BUFFER_SIZE,
                                         cfg->client_address,
                                         cfg->server_logical,
                                         cfg->server_physical,
                                         cfg->server_addr_len,
                                         client->send_seq,
                                         client->recv_seq,
                                         apdu, apdu_len);
        client->send_seq = (client->send_seq + 1) & 0x07;

        uart_send(client, client->tx_buffer, tx_len);

        /* Wait for ReadResponse */
        if (!wait_for_frame(client, &frame, cfg->receive_timeout_ms)) {
            ESP_LOGW(TAG, "No response for OBIS %s", entry->obis_str);
            reading.valid = false;
            goto update_reading;
        }

        if (frame.is_iframe) {
            client->recv_seq = (frame.send_seq + 1) & 0x07;
        }

        int result = handle_read_response_sn(client, frame.info, frame.info_len, &reading);

        if (result == -1) {
            reading.valid = false;
        }

        /* Apply scaler if we got a numeric value */
        if (reading.valid) {
            float raw_val = 0.0f;
            if (reading.data_type == AXDR_FLOAT32 || reading.data_type == AXDR_FLOAT64) {
                raw_val = reading.float_val;
            } else {
                raw_val = (float)reading.int_val;
            }

            if (reading.scaler != 0) {
                reading.scaled_value = axdr_apply_scaler(reading.int_val, reading.scaler);
            } else if (entry->scaler != 0) {
                reading.scaled_value = axdr_apply_scaler(reading.int_val, entry->scaler);
            } else {
                reading.scaled_value = raw_val;
            }
        }

update_reading:
        /* Store the reading (thread-safe) */
        if (xSemaphoreTake(client->readings_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            memcpy(&client->readings[i], &reading, sizeof(dlms_reading_t));
            xSemaphoreGive(client->readings_mutex);
        }

        if (reading.valid) {
            ESP_LOGI(TAG, "  %s = %.3f %s",
                     entry->name, reading.scaled_value, entry->unit_str);
        } else {
            ESP_LOGW(TAG, "  %s = FAILED", entry->name);
        }

        client->state = DLMS_STATE_CONNECTED;

        /* Inter-frame delay */
        vTaskDelay(pdMS_TO_TICKS(cfg->inter_frame_delay_ms));
    }

    /* Notify callback */
    if (client->on_readings_updated) {
        client->on_readings_updated(client->readings, client->obis_count,
                                     client->callback_ctx);
    }

disconnect:
    /* ── Step 4: DISC → UA/DM ── */
    client->state = DLMS_STATE_DISCONNECTING;
    ESP_LOGI(TAG, "Sending DISC...");

    tx_len = dlms_hdlc_build_disc(client->tx_buffer, DLMS_TX_BUFFER_SIZE,
                                   cfg->client_address,
                                   cfg->server_logical,
                                   cfg->server_physical,
                                   cfg->server_addr_len);
    uart_send(client, client->tx_buffer, tx_len);

    /* Wait for UA/DM (don't care too much if it fails) */
    if (wait_for_frame(client, &frame, 1000)) {
        ESP_LOGD(TAG, "Disconnect ack received (ctrl=0x%02X)", frame.control);
    }

    client->state = DLMS_STATE_IDLE;
    client->polls_completed++;
    client->last_poll_time_ms = get_millis();

    ESP_LOGI(TAG, "=== Poll cycle #%lu complete ===",
             (unsigned long)client->polls_completed);
    return;

error:
    client->state = DLMS_STATE_ERROR;
    client->errors_total++;
    ESP_LOGE(TAG, "Poll cycle failed (errors total: %lu)",
             (unsigned long)client->errors_total);

    /* Brief backoff before next attempt */
    vTaskDelay(pdMS_TO_TICKS(2000));
    client->state = DLMS_STATE_IDLE;
}

/* ─── FreeRTOS Task Functions ────────────────────────────────────── */

void dlms_client_poll_task(void *pvParameters)
{
    dlms_client_t *client = (dlms_client_t *)pvParameters;

    ESP_LOGI(TAG, "Poll task started (interval=%lu s, entries=%u)",
             (unsigned long)client->config.poll_interval_sec,
             client->obis_count);

    /* Initial delay to let Wi-Fi/MQTT connect */
    vTaskDelay(pdMS_TO_TICKS(3000));

    for (;;) {
        do_poll_cycle(client);

        /* Wait for next poll interval */
        uint32_t delay_ms = client->config.poll_interval_sec * 1000;
        ESP_LOGD(TAG, "Next poll in %lu seconds",
                 (unsigned long)client->config.poll_interval_sec);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

void dlms_client_push_task(void *pvParameters)
{
    dlms_client_t *client = (dlms_client_t *)pvParameters;

    ESP_LOGI(TAG, "Push listener task started");

    dlms_hdlc_frame_t frame;

    for (;;) {
        /* Continuously try to receive frames */
        if (wait_for_frame(client, &frame, 5000)) {
            ESP_LOGI(TAG, "PUSH frame received: ctrl=0x%02X info_len=%u",
                     frame.control, (unsigned)frame.info_len);

            if (frame.is_iframe && frame.info_len > 0) {
                /* Check if it's a DataNotification (tag 0x0F) or
                   unsolicited ReadResponse */
                uint8_t tag = frame.info[0];

                if (tag == 0x0C || tag == 0x0F) {
                    /*
                     * Parse PUSH data. The meter typically sends a structure
                     * containing multiple OBIS-value pairs.
                     * For now, try to parse as ReadResponse and match OBIS.
                     */
                    for (uint8_t i = 0; i < client->obis_count; i++) {
                        dlms_reading_t reading;
                        memset(&reading, 0, sizeof(reading));

                        int result = handle_read_response_sn(client, frame.info,
                                                          frame.info_len,
                                                          &reading);
                        if (result == 0 && reading.valid) {
                            /* Try to match to registered OBIS entry */
                            if (xSemaphoreTake(client->readings_mutex,
                                               pdMS_TO_TICKS(100)) == pdTRUE) {
                                memcpy(&client->readings[i], &reading,
                                       sizeof(dlms_reading_t));
                                xSemaphoreGive(client->readings_mutex);
                            }
                        }
                    }

                    /* Notify callback */
                    if (client->on_readings_updated) {
                        client->on_readings_updated(client->readings,
                                                     client->obis_count,
                                                     client->callback_ctx);
                    }
                }

                /* Send RR acknowledgment if meter expects it */
                /* Build minimal RR frame - just ack the received frame */
                /* For push mode, the meter usually doesn't need ack, but
                   some implementations do */
            }
        }

        /* Small delay to prevent busy-looping if no data */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

