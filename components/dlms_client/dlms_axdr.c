#include "dlms_axdr.h"
#include <string.h>
#include <math.h>
#include "esp_log.h"

static const char *TAG = "dlms_axdr";

int axdr_parse_length(const uint8_t *buf, size_t buf_len, size_t *value) {
    if (buf_len == 0) return -1;
    
    if (buf[0] < 0x80) {
        *value = buf[0];
        return 1;
    } else if (buf[0] == 0x81) {
        if (buf_len < 2) return -1;
        *value = buf[1];
        return 2;
    } else if (buf[0] == 0x82) {
        if (buf_len < 3) return -1;
        *value = (buf[1] << 8) | buf[2];
        return 3;
    }
    
    ESP_LOGE(TAG, "Unsupported length format: %02X", buf[0]);
    return -1;
}

int axdr_decode_type_tag(const uint8_t *buf, size_t buf_len, axdr_type_t *type) {
    if (buf_len < 1) return -1;
    *type = (axdr_type_t)buf[0];
    return 1;
}

int axdr_decode_integer8(const uint8_t *buf, size_t len, int8_t *val) {
    if (len < 1) return -1;
    *val = (int8_t)buf[0];
    return 1;
}

int axdr_decode_integer16(const uint8_t *buf, size_t len, int16_t *val) {
    if (len < 2) return -1;
    *val = (int16_t)((buf[0] << 8) | buf[1]);
    return 2;
}

int axdr_decode_integer32(const uint8_t *buf, size_t len, int32_t *val) {
    if (len < 4) return -1;
    *val = (int32_t)((((uint32_t)buf[0]) << 24) |
                     (((uint32_t)buf[1]) << 16) |
                     (((uint32_t)buf[2]) << 8) |
                      ((uint32_t)buf[3]));
    return 4;
}

int axdr_decode_integer64(const uint8_t *buf, size_t len, int64_t *val) {
    if (len < 8) return -1;
    uint64_t uval = 0;
    for (int i = 0; i < 8; i++) {
        uval = (uval << 8) | buf[i];
    }
    *val = (int64_t)uval;
    return 8;
}

int axdr_decode_unsigned8(const uint8_t *buf, size_t len, uint8_t *val) {
    if (len < 1) return -1;
    *val = buf[0];
    return 1;
}

int axdr_decode_unsigned16(const uint8_t *buf, size_t len, uint16_t *val) {
    if (len < 2) return -1;
    *val = (uint16_t)((buf[0] << 8) | buf[1]);
    return 2;
}

int axdr_decode_unsigned32(const uint8_t *buf, size_t len, uint32_t *val) {
    if (len < 4) return -1;
    *val = (uint32_t)((((uint32_t)buf[0]) << 24) |
                      (((uint32_t)buf[1]) << 16) |
                      (((uint32_t)buf[2]) << 8) |
                       ((uint32_t)buf[3]));
    return 4;
}

int axdr_decode_unsigned64(const uint8_t *buf, size_t len, uint64_t *val) {
    if (len < 8) return -1;
    uint64_t uval = 0;
    for (int i = 0; i < 8; i++) {
        uval = (uval << 8) | buf[i];
    }
    *val = uval;
    return 8;
}

int axdr_decode_float32(const uint8_t *buf, size_t len, float *val) {
    if (len < 4) return -1;
    uint32_t uval = ((((uint32_t)buf[0]) << 24) |
                     (((uint32_t)buf[1]) << 16) |
                     (((uint32_t)buf[2]) << 8) |
                      ((uint32_t)buf[3]));
    memcpy(val, &uval, 4);
    return 4;
}

int axdr_decode_float64(const uint8_t *buf, size_t len, double *val) {
    if (len < 8) return -1;
    uint64_t uval = 0;
    for (int i = 0; i < 8; i++) {
        uval = (uval << 8) | buf[i];
    }
    memcpy(val, &uval, 8);
    return 8;
}

int axdr_decode_boolean(const uint8_t *buf, size_t len, bool *val) {
    if (len < 1) return -1;
    *val = (buf[0] != 0);
    return 1;
}

int axdr_decode_octet_string(const uint8_t *buf, size_t len, uint8_t *out, size_t out_max, size_t *out_len) {
    size_t str_len = 0;
    int length_bytes = axdr_parse_length(buf, len, &str_len);
    if (length_bytes < 0) return -1;
    if (len < (size_t)length_bytes + str_len) return -1;
    
    *out_len = (str_len < out_max) ? str_len : out_max;
    if (*out_len > 0) {
        memcpy(out, buf + length_bytes, *out_len);
    }
    
    return length_bytes + str_len;
}

int axdr_decode_visible_string(const uint8_t *buf, size_t len, char *out, size_t out_max) {
    size_t str_len = 0;
    int length_bytes = axdr_parse_length(buf, len, &str_len);
    if (length_bytes < 0) return -1;
    if (len < (size_t)length_bytes + str_len) return -1;
    
    size_t copy_len = (str_len < out_max - 1) ? str_len : out_max - 1;
    if (copy_len > 0) {
        memcpy(out, buf + length_bytes, copy_len);
    }
    out[copy_len] = '\0';
    
    return length_bytes + str_len;
}

int axdr_parse_value(const uint8_t *buf, size_t len, dlms_reading_t *reading) {
    if (len < 1) return -1;
    
    axdr_type_t type;
    int type_bytes = axdr_decode_type_tag(buf, len, &type);
    if (type_bytes < 0) return -1;
    
    int consumed = type_bytes;
    reading->data_type = type;
    
    switch (type) {
        case AXDR_NULL_DATA:
            break;
            
        case AXDR_BOOLEAN: {
            bool val;
            int bytes = axdr_decode_boolean(buf + consumed, len - consumed, &val);
            if (bytes < 0) return -1;
            reading->bool_val = val;
            consumed += bytes;
            break;
        }
            
        case AXDR_INTEGER: {
            int8_t val;
            int bytes = axdr_decode_integer8(buf + consumed, len - consumed, &val);
            if (bytes < 0) return -1;
            reading->int_val = val;
            consumed += bytes;
            break;
        }
            
        case AXDR_LONG: {
            int16_t val;
            int bytes = axdr_decode_integer16(buf + consumed, len - consumed, &val);
            if (bytes < 0) return -1;
            reading->int_val = val;
            consumed += bytes;
            break;
        }
            
        case AXDR_DOUBLE_LONG: {
            int32_t val;
            int bytes = axdr_decode_integer32(buf + consumed, len - consumed, &val);
            if (bytes < 0) return -1;
            reading->int_val = val;
            consumed += bytes;
            break;
        }
            
        case AXDR_LONG64: {
            int64_t val;
            int bytes = axdr_decode_integer64(buf + consumed, len - consumed, &val);
            if (bytes < 0) return -1;
            reading->int_val = val;
            consumed += bytes;
            break;
        }
            
        case AXDR_UNSIGNED:
        case AXDR_ENUM: {
            uint8_t val;
            int bytes = axdr_decode_unsigned8(buf + consumed, len - consumed, &val);
            if (bytes < 0) return -1;
            reading->uint_val = val;
            consumed += bytes;
            break;
        }
            
        case AXDR_LONG_UNSIGNED: {
            uint16_t val;
            int bytes = axdr_decode_unsigned16(buf + consumed, len - consumed, &val);
            if (bytes < 0) return -1;
            reading->uint_val = val;
            reading->int_val = val;
            consumed += bytes;
            break;
        }
            
        case AXDR_DOUBLE_LONG_UNSIGNED: {
            uint32_t val;
            int bytes = axdr_decode_unsigned32(buf + consumed, len - consumed, &val);
            if (bytes < 0) return -1;
            reading->uint_val = val;
            reading->int_val = val;
            consumed += bytes;
            break;
        }
            
        case AXDR_LONG64_UNSIGNED: {
            uint64_t val;
            int bytes = axdr_decode_unsigned64(buf + consumed, len - consumed, &val);
            if (bytes < 0) return -1;
            reading->uint_val = val;
            reading->int_val = val;
            consumed += bytes;
            break;
        }
            
        case AXDR_FLOAT32: {
            float val;
            int bytes = axdr_decode_float32(buf + consumed, len - consumed, &val);
            if (bytes < 0) return -1;
            reading->float_val = val;
            consumed += bytes;
            break;
        }
            
        case AXDR_FLOAT64: {
            double val;
            int bytes = axdr_decode_float64(buf + consumed, len - consumed, &val);
            if (bytes < 0) return -1;
            reading->double_val = val;
            consumed += bytes;
            break;
        }
            
        case AXDR_OCTET_STRING: {
            size_t out_len;
            int bytes = axdr_decode_octet_string(buf + consumed, len - consumed, (uint8_t *)reading->str_val, sizeof(reading->str_val) - 1, &out_len);
            if (bytes < 0) return -1;
            // Octet string is generally hex, but we might just store raw bytes. Null terminate safely.
            reading->str_val[out_len] = '\0';
            consumed += bytes;
            break;
        }
            
        case AXDR_VISIBLE_STRING:
        case AXDR_UTF8_STRING: {
            int bytes = axdr_decode_visible_string(buf + consumed, len - consumed, reading->str_val, sizeof(reading->str_val));
            if (bytes < 0) return -1;
            consumed += bytes;
            break;
        }
            
        case AXDR_ARRAY:
        case AXDR_STRUCTURE: {
            size_t count = 0;
            int len_bytes = axdr_parse_length(buf + consumed, len - consumed, &count);
            if (len_bytes < 0) return -1;
            consumed += len_bytes;
            
            for (size_t i = 0; i < count; i++) {
                dlms_reading_t dummy_reading;
                int bytes = axdr_parse_value(buf + consumed, len - consumed, &dummy_reading);
                if (bytes < 0) return -1;
                consumed += bytes;
            }
            break;
        }
            
        default:
            ESP_LOGW(TAG, "Unexpected A-XDR type tag: %02X", type);
            // We can't skip unknown types because we don't know their length
            return -1;
    }
    
    return consumed;
}

int axdr_parse_scaler_unit(const uint8_t *buf, size_t len, int8_t *scaler, uint8_t *unit) {
    if (len < 6) return -1;
    
    int consumed = 0;
    
    // Expecting 0x02 (Structure)
    if (buf[consumed++] != AXDR_STRUCTURE) return -1;
    
    // Expecting length 2
    if (buf[consumed++] != 0x02) return -1;
    
    // Expecting 0x0F (Integer) for scaler
    if (buf[consumed++] != AXDR_INTEGER) return -1;
    
    // Read scaler
    *scaler = (int8_t)buf[consumed++];
    
    // Expecting 0x16 (Enum) for unit
    if (buf[consumed++] != AXDR_ENUM) return -1;
    
    // Read unit
    *unit = buf[consumed++];
    
    return consumed;
}

float axdr_apply_scaler(int64_t raw_value, int8_t scaler) {
    switch (scaler) {
        case 0: return (float)raw_value;
        case 1: return (float)raw_value * 10.0f;
        case 2: return (float)raw_value * 100.0f;
        case 3: return (float)raw_value * 1000.0f;
        case -1: return (float)raw_value * 0.1f;
        case -2: return (float)raw_value * 0.01f;
        case -3: return (float)raw_value * 0.001f;
        default: return (float)raw_value * powf(10.0f, (float)scaler);
    }
}
