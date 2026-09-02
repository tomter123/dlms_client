#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "dlms_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Decode ASN.1 variable-length
 * 
 * @param buf Buffer
 * @param buf_len Buffer length
 * @param value Pointer to store the decoded length
 * @return int Bytes consumed, or -1 on error
 */
int axdr_parse_length(const uint8_t *buf, size_t buf_len, size_t *value);

/**
 * @brief Read 1-byte type tag
 * 
 * @param buf Buffer
 * @param buf_len Buffer length
 * @param type Pointer to store the type tag
 * @return int Bytes consumed, or -1 on error
 */
int axdr_decode_type_tag(const uint8_t *buf, size_t buf_len, axdr_type_t *type);

int axdr_decode_integer8(const uint8_t *buf, size_t len, int8_t *val);
int axdr_decode_integer16(const uint8_t *buf, size_t len, int16_t *val);
int axdr_decode_integer32(const uint8_t *buf, size_t len, int32_t *val);
int axdr_decode_integer64(const uint8_t *buf, size_t len, int64_t *val);

int axdr_decode_unsigned8(const uint8_t *buf, size_t len, uint8_t *val);
int axdr_decode_unsigned16(const uint8_t *buf, size_t len, uint16_t *val);
int axdr_decode_unsigned32(const uint8_t *buf, size_t len, uint32_t *val);
int axdr_decode_unsigned64(const uint8_t *buf, size_t len, uint64_t *val);

int axdr_decode_float32(const uint8_t *buf, size_t len, float *val);
int axdr_decode_float64(const uint8_t *buf, size_t len, double *val);

int axdr_decode_boolean(const uint8_t *buf, size_t len, bool *val);

int axdr_decode_octet_string(const uint8_t *buf, size_t len, uint8_t *out, size_t out_max, size_t *out_len);
int axdr_decode_visible_string(const uint8_t *buf, size_t len, char *out, size_t out_max);

/**
 * @brief Parse one complete A-XDR value (type tag + data) into a reading struct.
 * 
 * @param buf Buffer
 * @param len Buffer length
 * @param reading Pointer to store the parsed reading
 * @return int Bytes consumed, or -1 on error
 */
int axdr_parse_value(const uint8_t *buf, size_t len, dlms_reading_t *reading);

/**
 * @brief Parse the 2-element structure: 0x02 0x02 0x0F scaler 0x16 unit.
 * 
 * @param buf Buffer
 * @param len Buffer length
 * @param scaler Pointer to store the scaler
 * @param unit Pointer to store the unit
 * @return int Bytes consumed, or -1 on error
 */
int axdr_parse_scaler_unit(const uint8_t *buf, size_t len, int8_t *scaler, uint8_t *unit);

/**
 * @brief Calculate raw_value * 10^scaler
 * 
 * @param raw_value Raw integer value
 * @param scaler Scaler power of 10
 * @return float Scaled value
 */
float axdr_apply_scaler(int64_t raw_value, int8_t scaler);

#ifdef __cplusplus
}
#endif
