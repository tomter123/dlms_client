#include "dlms_hdlc.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "dlms_hdlc";

static const uint16_t fcstab[256] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

uint16_t dlms_crc16(const uint8_t *data, size_t len) {
    uint16_t fcs = 0xffff;
    while (len--) {
        fcs = (fcs >> 8) ^ fcstab[(fcs ^ *data++) & 0xff];
    }
    return ~fcs;
}

bool dlms_crc16_verify(const uint8_t *data, size_t len) {
    uint16_t fcs = 0xffff;
    while (len--) {
        fcs = (fcs >> 8) ^ fcstab[(fcs ^ *data++) & 0xff];
    }
    return fcs == 0xf0b8;
}

size_t dlms_hdlc_encode_address(uint8_t *out, uint16_t logical_addr, uint16_t physical_addr, uint8_t addr_len) {
    if (addr_len == 1) {
        out[0] = (logical_addr << 1) | 0x01;
        return 1;
    } else if (addr_len == 2) {
        out[0] = (logical_addr << 1);
        out[1] = (physical_addr << 1) | 0x01;
        return 2;
    } else if (addr_len == 4) {
        out[0] = ((logical_addr >> 7) & 0x7F) << 1;
        out[1] = (logical_addr & 0x7F) << 1;
        out[2] = ((physical_addr >> 7) & 0x7F) << 1;
        out[3] = ((physical_addr & 0x7F) << 1) | 0x01;
        return 4;
    }
    return 0;
}

static size_t build_frame(uint8_t *out, size_t max_len, uint8_t client_addr, uint16_t server_logical, uint16_t server_physical, uint8_t addr_len, uint8_t control, const uint8_t *info, size_t info_len) {
    if (max_len < 12 + info_len) return 0;
    
    size_t idx = 0;
    out[idx++] = DLMS_HDLC_FLAG;
    
    size_t format_len_idx = idx;
    idx += 2; // format + length placeholder
    
    // Dest address = server
    idx += dlms_hdlc_encode_address(&out[idx], server_logical, server_physical, addr_len);
    // Source address = client (1 byte)
    out[idx++] = (client_addr << 1) | 0x01;
    
    // Control
    out[idx++] = control;
    
    bool has_info = (info_len > 0);
    size_t frame_len = (idx - format_len_idx) + (has_info ? 2 + info_len + 2 : 2);
    
    out[format_len_idx] = HDLC_FORMAT_TYPE | ((frame_len >> 8) & 0x07);
    out[format_len_idx + 1] = frame_len & 0xFF;
    
    if (has_info) {
        uint16_t hcs = dlms_crc16(&out[format_len_idx], idx - format_len_idx);
        out[idx++] = hcs & 0xFF;
        out[idx++] = (hcs >> 8) & 0xFF;
        
        memcpy(&out[idx], info, info_len);
        idx += info_len;
    }
    
    uint16_t fcs = dlms_crc16(&out[format_len_idx], idx - format_len_idx);
    out[idx++] = fcs & 0xFF;
    out[idx++] = (fcs >> 8) & 0xFF;
    
    out[idx++] = DLMS_HDLC_FLAG;
    
    return idx;
}

size_t dlms_hdlc_build_snrm(uint8_t *out, size_t max_len, uint8_t client_addr, uint16_t server_logical, uint16_t server_physical, uint8_t addr_len) {
    uint8_t snrm_info[] = {0x81, 0x80, 0x12, 0x05, 0x01, 0x80, 0x06, 0x01, 0x80, 0x07, 0x04, 0x00, 0x00, 0x00, 0x01, 0x08, 0x04, 0x00, 0x00, 0x00, 0x01};
    return build_frame(out, max_len, client_addr, server_logical, server_physical, addr_len, HDLC_CTRL_SNRM, snrm_info, sizeof(snrm_info));
}

size_t dlms_hdlc_build_disc(uint8_t *out, size_t max_len, uint8_t client_addr, uint16_t server_logical, uint16_t server_physical, uint8_t addr_len) {
    return build_frame(out, max_len, client_addr, server_logical, server_physical, addr_len, HDLC_CTRL_DISC, NULL, 0);
}

size_t dlms_hdlc_build_iframe(uint8_t *out, size_t max_len, uint8_t client_addr, uint16_t server_logical, uint16_t server_physical, uint8_t addr_len, uint8_t send_seq, uint8_t recv_seq, const uint8_t *apdu, size_t apdu_len) {
    uint8_t control = (send_seq << 1) | (recv_seq << 5) | 0x10;
    
    uint8_t info[DLMS_TX_BUFFER_SIZE];
    info[0] = DLMS_LLC_REQUEST_DSAP;
    info[1] = DLMS_LLC_REQUEST_SSAP;
    info[2] = DLMS_LLC_CONTROL;
    
    if (apdu_len > sizeof(info) - 3) {
        ESP_LOGE(TAG, "APDU too large");
        return 0;
    }
    memcpy(&info[3], apdu, apdu_len);
    
    return build_frame(out, max_len, client_addr, server_logical, server_physical, addr_len, control, info, 3 + apdu_len);
}

bool dlms_hdlc_parse_frame(const uint8_t *data, size_t len, dlms_hdlc_frame_t *frame) {
    if (len < 5) return false;
    if (data[0] != DLMS_HDLC_FLAG || data[len-1] != DLMS_HDLC_FLAG) return false;
    
    if ((data[1] & 0xF0) != HDLC_FORMAT_TYPE) return false;
    
    uint16_t frame_len = ((data[1] & 0x07) << 8) | data[2];
    if (frame_len != len - 2) return false;
    
    if (!dlms_crc16_verify(&data[1], len - 2)) {
        ESP_LOGE(TAG, "FCS check failed");
        return false;
    }
    
    size_t idx = 3;
    // skip dest addr
    while (idx < len - 3 && !(data[idx++] & 1));
    // skip src addr
    while (idx < len - 3 && !(data[idx++] & 1));
    
    frame->control = data[idx++];
    size_t header_len = idx - 1; // From format byte to control byte inclusive
    
    frame->info_len = 0;
    frame->is_iframe = false;
    frame->is_rr = false;
    frame->is_ui = false;
    
    if ((frame->control & 0x01) == 0) {
        frame->is_iframe = true;
        frame->send_seq = (frame->control >> 1) & 0x07;
        frame->recv_seq = (frame->control >> 5) & 0x07;
    } else if ((frame->control & 0x0F) == 0x01) {
        frame->is_rr = true;
        frame->recv_seq = (frame->control >> 5) & 0x07;
    } else if (frame->control == 0x13) {
        frame->is_ui = true;
    }
    
    if (frame_len > header_len + 2) {
        if (!dlms_crc16_verify(&data[1], header_len + 2)) {
            ESP_LOGE(TAG, "HCS check failed");
            return false;
        }
        idx += 2; // skip HCS
        
        size_t info_field_len = len - 1 - 2 - idx;
        
        if (info_field_len > 0) {
                          // LLC for response (E6 E7 00) or UI push (E6 E6 00)
              if (info_field_len >= 3 && ((data[idx] == DLMS_LLC_RESPONSE_DSAP && (data[idx+1] == DLMS_LLC_RESPONSE_SSAP || data[idx+1] == DLMS_LLC_REQUEST_SSAP)) || data[idx] == 0xE0) && data[idx+2] == DLMS_LLC_CONTROL) {
                  idx += 3;
                  info_field_len -= 3;
              }
              // Double LLC quirk for E450 push
              if (info_field_len >= 3 && data[idx] == 0xE0 && data[idx+2] == 0x00) {
                  idx += 3;
                  info_field_len -= 3;
              }

            
            if (info_field_len > sizeof(frame->info)) {
                ESP_LOGE(TAG, "Info field too large");
                return false;
            }
            
            memcpy(frame->info, &data[idx], info_field_len);
            frame->info_len = info_field_len;
        }
    }
    
    return true;
}

int dlms_hdlc_find_frame(const uint8_t *buf, size_t len, size_t *frame_start, size_t *frame_end) {
    *frame_start = 0;
    *frame_end = 0;
    
    bool found_start = false;
    for (size_t i = 0; i < len; i++) {
        if (buf[i] == DLMS_HDLC_FLAG) {
            if (!found_start) {
                *frame_start = i;
                found_start = true;
            } else {
                if (i - *frame_start > 1) {
                    *frame_end = i + 1; /* include closing flag */
                    return 0;
                } else {
                    *frame_start = i; // consecutive flags
                }
            }
        }
    }
    
    return -1;
}
