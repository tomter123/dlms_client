#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dlms_types.h"

#ifdef __cplusplus
extern "C" {
#endif

bool dlms_obis_str_to_bytes(const char *str, uint8_t obis[6]);
void dlms_obis_bytes_to_str(const uint8_t obis[6], char *str, size_t str_len);
bool dlms_obis_match(const uint8_t a[6], const uint8_t b[6]);
const char *dlms_unit_to_str(uint8_t unit);
const char *dlms_state_to_str(dlms_state_t state);

#ifdef __cplusplus
}
#endif
