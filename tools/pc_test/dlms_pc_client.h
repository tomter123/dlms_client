/*
 * PC DLMS Client Wrapper
 *
 * Wraps the portable DLMS protocol code to use the Windows serial_port API.
 */
#pragma once

#include "dlms_types.h"
#include "serial_port.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    serial_port_t *port;
    uint8_t       client_address;
    uint16_t      server_logical;
    uint16_t      server_physical;
    uint8_t       server_addr_len;
    dlms_auth_t   auth_mode;
    char          password[32];
    uint32_t      timeout_ms;
} pc_client_config_t;

/**
 * Execute a single synchronous DLMS poll cycle over the serial port.
 * Returns true if successful, false on error.
 */
bool pc_dlms_poll(const pc_client_config_t *config, const dlms_obis_entry_t *obis_list, uint8_t obis_count, dlms_reading_t *out_readings);

#ifdef __cplusplus
}
#endif

