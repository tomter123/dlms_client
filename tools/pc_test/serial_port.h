/*
 * Windows Serial Port (COM Port) API
 *
 * Provides a simple interface for RS-485 communication via
 * USB-to-serial adapters (FTDI, CH340, CP2102, etc.)
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle */
typedef struct serial_port serial_port_t;

/* Parity options */
typedef enum {
    SERIAL_PARITY_NONE = 0,
    SERIAL_PARITY_ODD,
    SERIAL_PARITY_EVEN,
} serial_parity_t;

/* Configuration */
typedef struct {
    const char     *port_name;   /* e.g. "COM3", "\\\\.\\COM12" */
    uint32_t        baud_rate;   /* e.g. 9600 */
    uint8_t         data_bits;   /* 7 or 8 */
    serial_parity_t parity;
    uint8_t         stop_bits;   /* 1 or 2 */
} serial_config_t;

/**
 * Open a serial port.
 * Returns NULL on failure.
 */
serial_port_t *serial_open(const serial_config_t *config);

/**
 * Close a serial port and free resources.
 */
void serial_close(serial_port_t *port);

/**
 * Write bytes to serial port.
 * Returns number of bytes written, or -1 on error.
 */
int serial_write(serial_port_t *port, const uint8_t *data, size_t len);

/**
 * Read bytes from serial port with timeout.
 * Returns number of bytes read (may be 0 if timeout), or -1 on error.
 */
int serial_read(serial_port_t *port, uint8_t *buf, size_t max_len,
                uint32_t timeout_ms);

/**
 * Flush any pending input data.
 */
void serial_flush(serial_port_t *port);

/**
 * Check if port is open and valid.
 */
bool serial_is_open(serial_port_t *port);

/**
 * List available COM ports (Windows).
 * Prints port names to stdout.
 */
void serial_list_ports(void);

#ifdef __cplusplus
}
#endif
