/*
 * PC DLMS Client Wrapper Implementation (SN REFERENCING)
 */
#include "dlms_pc_client.h"
#include "dlms_hdlc.h"
#include "dlms_axdr.h"
#include "dlms_helpers.h"
#include "compat/esp_log.h"

#include <string.h>
#include <windows.h>
#include <time.h>

static const char *TAG = "pc_client";

/* AARQ Application Context OIDs */
static const uint8_t APP_CONTEXT_SN[] = {
    0x06, 0x07, 0x60, 0x85, 0x74, 0x05, 0x08, 0x01, 0x02  /* .1.2 = SN No Ciphering */
};
static const uint8_t AUTH_MECHANISM_LOW_SN[] = {
    0x60, 0x85, 0x74, 0x05, 0x08, 0x02, 0x01
};

static uint32_t get_millis(void) {
    return GetTickCount();
}

static bool wait_for_frame(serial_port_t *port, dlms_hdlc_frame_t *frame, uint32_t timeout_ms, uint8_t *rx_buf, size_t *rx_len) {
    uint8_t tmp[256];
    uint32_t start = get_millis();
    *rx_len = 0;

    while ((get_millis() - start) < timeout_ms) {
        int n = serial_read(port, tmp, sizeof(tmp), 50);
        if (n > 0) {
            size_t space = DLMS_RX_BUFFER_SIZE - *rx_len;
            size_t copy = (size_t)n < space ? (size_t)n : space;
            memcpy(rx_buf + *rx_len, tmp, copy);
            *rx_len += copy;

            size_t frame_start, frame_end;
            if (dlms_hdlc_find_frame(rx_buf, *rx_len, &frame_start, &frame_end) == 0) {
                const uint8_t *fdata = rx_buf + frame_start;
                size_t flen = frame_end - frame_start;

                ESP_LOG_BUFFER_HEXDUMP("RX", fdata, flen, ESP_LOG_DEBUG);
                if (dlms_hdlc_parse_frame(fdata, flen, frame)) {
                    return true;
                } else {
                    if (frame_end < *rx_len) {
                        memmove(rx_buf, rx_buf + frame_end, *rx_len - frame_end);
                        *rx_len -= frame_end;
                    } else {
                        *rx_len = 0;
                    }
                }
            }
        }
    }
    return false;
}

static size_t build_aarq_sn(uint8_t *out, dlms_auth_t auth, const char *password) {
    size_t pos = 0;
    out[pos++] = DLMS_TAG_AARQ; /* 0x60 */
    size_t aarq_len_pos = pos;
    out[pos++] = 0; 
    size_t content_start = pos;

    out[pos++] = 0xA1; /* [1] application-context-name */
    out[pos++] = sizeof(APP_CONTEXT_SN);
    memcpy(out + pos, APP_CONTEXT_SN, sizeof(APP_CONTEXT_SN));
    pos += sizeof(APP_CONTEXT_SN);

    if (auth == DLMS_AUTH_LOW && password) {
        out[pos++] = 0x8A; /* [10] sender-acse-requirements IMPLICIT BIT STRING */
        out[pos++] = sizeof(AUTH_MECHANISM_LOW_SN);
        memcpy(out + pos, AUTH_MECHANISM_LOW_SN, sizeof(AUTH_MECHANISM_LOW_SN));
        pos += sizeof(AUTH_MECHANISM_LOW_SN);

        size_t pw_len = strlen(password);
        out[pos++] = pw_len + 2;
        out[pos++] = 0x80; /* [0] IMPLICIT charstring (GraphicString) */
        out[pos++] = pw_len;
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

    /* Update the total AARQ length at out[1] */
    out[aarq_len_pos] = pos - content_start;
    return pos;
}

static size_t build_read_request_sn(uint8_t *out, uint16_t short_name) {
    size_t pos = 0;
    out[pos++] = 0x05; /* ReadRequest */
    out[pos++] = 0x01; /* item count = 1 */
    size_t rx_len, tx_len;
    uint8_t apdu[256];
    size_t apdu_len;

    serial_flush(cfg->port);

    // 1. SNRM -> UA
    ESP_LOGI(TAG, "Sending SNRM...");
    tx_len = dlms_hdlc_build_snrm(tx_buf, sizeof(tx_buf), cfg->client_address, cfg->server_logical, cfg->server_physical, cfg->server_addr_len);
    ESP_LOG_BUFFER_HEXDUMP("TX", tx_buf, tx_len, ESP_LOG_DEBUG);
    serial_write(cfg->port, tx_buf, tx_len);

    if (!wait_for_frame(cfg->port, &frame, cfg->timeout_ms, rx_buf, &rx_len) || (frame.control != HDLC_CTRL_UA && (frame.control & 0xEF) != 0x63)) {
        ESP_LOGE(TAG, "Failed to connect (no UA)");
        return false;
    }
    ESP_LOGI(TAG, "HDLC Connected.");

    // 2. AARQ -> AARE (SN)
    ESP_LOGI(TAG, "Sending SN AARQ...");
    apdu_len = build_aarq_sn(apdu, cfg->auth_mode, cfg->password);
    tx_len = dlms_hdlc_build_iframe(tx_buf, sizeof(tx_buf), cfg->client_address, cfg->server_logical, cfg->server_physical, cfg->server_addr_len, send_seq, recv_seq, apdu, apdu_len);
    send_seq = (send_seq + 1) & 0x07;
    
    ESP_LOG_BUFFER_HEXDUMP("TX", tx_buf, tx_len, ESP_LOG_DEBUG);
    serial_write(cfg->port, tx_buf, tx_len);

    if (!wait_for_frame(cfg->port, &frame, cfg->timeout_ms, rx_buf, &rx_len) || !frame.is_iframe) {
        ESP_LOGE(TAG, "Failed to get AARE");
        return false;
    }
    recv_seq = (frame.send_seq + 1) & 0x07;
    ESP_LOGI(TAG, "DLMS SN Association Established.");

    // 3. Read Requests (SN)
    for (uint8_t i = 0; i < obis_count; i++) {
        ESP_LOGI(TAG, "Reading SN 0x%04X (%s)...", obis_list[i].short_name, obis_list[i].name);
        apdu_len = build_read_request_sn(apdu, obis_list[i].short_name);
        tx_len = dlms_hdlc_build_iframe(tx_buf, sizeof(tx_buf), cfg->client_address, cfg->server_logical, cfg->server_physical, cfg->server_addr_len, send_seq, recv_seq, apdu, apdu_len);
        send_seq = (send_seq + 1) & 0x07;
        
        ESP_LOG_BUFFER_HEXDUMP("TX", tx_buf, tx_len, ESP_LOG_DEBUG);
        serial_write(cfg->port, tx_buf, tx_len);

        out_readings[i].valid = false;
        strncpy(out_readings[i].obis_str, obis_list[i].obis_str, sizeof(out_readings[i].obis_str)-1);

        if (wait_for_frame(cfg->port, &frame, cfg->timeout_ms, rx_buf, &rx_len) && frame.is_iframe) {
            recv_seq = (frame.send_seq + 1) & 0x07;
            if (frame.info_len >= 3 && frame.info[0] == 0x0C && frame.info[1] == 0x01 && frame.info[2] == 0x00) {
                /* 0x0C = ReadResponse, 0x01 = item count, 0x00 = Success (DataAccessResult) */
                if (axdr_parse_value(frame.info + 3, frame.info_len - 3, &out_readings[i]) > 0) {
                    out_readings[i].valid = true;
                    if (out_readings[i].scaler != 0) {
                        out_readings[i].scaled_value = axdr_apply_scaler(out_readings[i].int_val, out_readings[i].scaler);
                    } else {
                        out_readings[i].scaled_value = out_readings[i].float_val;
                    }
                    ESP_LOGI(TAG, " -> Value: %.3f", out_readings[i].scaled_value);
                }
            } else {
                ESP_LOGE(TAG, "Invalid ReadResponse or DataAccessError");
            }
        }
        Sleep(50);
    }

    // 4. DISC
    ESP_LOGI(TAG, "Sending DISC...");
    tx_len = dlms_hdlc_build_disc(tx_buf, sizeof(tx_buf), cfg->client_address, cfg->server_logical, cfg->server_physical, cfg->server_addr_len);
    ESP_LOG_BUFFER_HEXDUMP("TX", tx_buf, tx_len, ESP_LOG_DEBUG);
    serial_write(cfg->port, tx_buf, tx_len);
    wait_for_frame(cfg->port, &frame, 500, rx_buf, &rx_len);

    return true;
}
