#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dlms_types.h"

#ifdef __cplusplus
extern "C" {
#endif

uint16_t dlms_crc16(const uint8_t *data, size_t len);
bool dlms_crc16_verify(const uint8_t *data, size_t len);
size_t dlms_hdlc_encode_address(uint8_t *out, uint16_t logical_addr, uint16_t physical_addr, uint8_t addr_len);
size_t dlms_hdlc_build_snrm(uint8_t *out, size_t max_len, uint8_t client_addr, uint16_t server_logical, uint16_t server_physical, uint8_t addr_len);
size_t dlms_hdlc_build_disc(uint8_t *out, size_t max_len, uint8_t client_addr, uint16_t server_logical, uint16_t server_physical, uint8_t addr_len);
size_t dlms_hdlc_build_iframe(uint8_t *out, size_t max_len, uint8_t client_addr, uint16_t server_logical, uint16_t server_physical, uint8_t addr_len, uint8_t send_seq, uint8_t recv_seq, const uint8_t *apdu, size_t apdu_len);
bool dlms_hdlc_parse_frame(const uint8_t *data, size_t len, dlms_hdlc_frame_t *frame);
int dlms_hdlc_find_frame(const uint8_t *buf, size_t len, size_t *frame_start, size_t *frame_end);

#ifdef __cplusplus
}
#endif
