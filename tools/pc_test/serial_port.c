/*
 * Windows Serial Port Implementation
 *
 * Uses Win32 API for COM port access.
 * Works with USB-to-RS485 adapters (FTDI, CH340, CP2102, etc.)
 */

#include "serial_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

struct serial_port {
    HANDLE hComm;
    char   port_name[32];
};

serial_port_t *serial_open(const serial_config_t *config)
{
    serial_port_t *port = (serial_port_t *)calloc(1, sizeof(serial_port_t));
    if (!port) return NULL;

    /* Build port path — for COM10+ need \\.\COMxx format */
    char path[64];
    if (strncmp(config->port_name, "\\\\.\\", 4) == 0) {
        strncpy(path, config->port_name, sizeof(path) - 1);
    } else {
        snprintf(path, sizeof(path), "\\\\.\\%s", config->port_name);
    }
    strncpy(port->port_name, config->port_name, sizeof(port->port_name) - 1);

    /* Open COM port */
    port->hComm = CreateFileA(
        path,
        GENERIC_READ | GENERIC_WRITE,
        0,              /* No sharing */
        NULL,           /* Default security */
        OPEN_EXISTING,
        0,              /* Synchronous I/O */
        NULL
    );

    if (port->hComm == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        fprintf(stderr, "Error: Cannot open %s (error %lu)\n", path, (unsigned long)err);
        if (err == ERROR_FILE_NOT_FOUND) {
            fprintf(stderr, "  Port does not exist. Use --list to see available ports.\n");
        } else if (err == ERROR_ACCESS_DENIED) {
            fprintf(stderr, "  Port is in use by another application.\n");
        }
        free(port);
        return NULL;
    }

    /* Configure port parameters */
    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(port->hComm, &dcb)) {
        fprintf(stderr, "Error: GetCommState failed\n");
        CloseHandle(port->hComm);
        free(port);
        return NULL;
    }

    dcb.BaudRate = config->baud_rate;
    dcb.ByteSize = config->data_bits;

    switch (config->parity) {
        case SERIAL_PARITY_NONE: dcb.Parity = NOPARITY; break;
        case SERIAL_PARITY_ODD:  dcb.Parity = ODDPARITY; break;
        case SERIAL_PARITY_EVEN: dcb.Parity = EVENPARITY; break;
    }

    dcb.StopBits = (config->stop_bits == 2) ? TWOSTOPBITS : ONESTOPBIT;

    /* Disable flow control */
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fBinary = TRUE;
    dcb.fParity = (config->parity != SERIAL_PARITY_NONE) ? TRUE : FALSE;

    if (!SetCommState(port->hComm, &dcb)) {
        fprintf(stderr, "Error: SetCommState failed (error %lu)\n",
                (unsigned long)GetLastError());
        CloseHandle(port->hComm);
        free(port);
        return NULL;
    }

    /* Set timeouts — we use read timeout per-call via serial_read() */
    COMMTIMEOUTS timeouts;
    timeouts.ReadIntervalTimeout         = 50;     /* ms between chars */
    timeouts.ReadTotalTimeoutMultiplier  = 0;
    timeouts.ReadTotalTimeoutConstant    = 100;    /* default, overridden per read */
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant   = 1000;
    SetCommTimeouts(port->hComm, &timeouts);

    /* Flush any stale data */
    PurgeComm(port->hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);

    printf("Opened %s @ %lu baud, %d%c%d\n",
           config->port_name,
           (unsigned long)config->baud_rate,
           config->data_bits,
           config->parity == SERIAL_PARITY_NONE ? 'N' :
           config->parity == SERIAL_PARITY_EVEN ? 'E' : 'O',
           config->stop_bits);

    return port;
}

void serial_close(serial_port_t *port)
{
    if (port) {
        if (port->hComm != INVALID_HANDLE_VALUE) {
            CloseHandle(port->hComm);
        }
        free(port);
    }
}

int serial_write(serial_port_t *port, const uint8_t *data, size_t len)
{
    if (!port || port->hComm == INVALID_HANDLE_VALUE) return -1;

    DWORD written = 0;
    if (!WriteFile(port->hComm, data, (DWORD)len, &written, NULL)) {
        fprintf(stderr, "Error: WriteFile failed (error %lu)\n",
                (unsigned long)GetLastError());
        return -1;
    }

    /* Wait for transmission to complete */
    FlushFileBuffers(port->hComm);

    return (int)written;
}

int serial_read(serial_port_t *port, uint8_t *buf, size_t max_len,
                uint32_t timeout_ms)
{
    if (!port || port->hComm == INVALID_HANDLE_VALUE) return -1;

    /* Update timeout for this read */
    COMMTIMEOUTS timeouts;
    timeouts.ReadIntervalTimeout         = 50;
    timeouts.ReadTotalTimeoutMultiplier  = 0;
    timeouts.ReadTotalTimeoutConstant    = timeout_ms;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant   = 1000;
    SetCommTimeouts(port->hComm, &timeouts);

    DWORD bytes_read = 0;
    if (!ReadFile(port->hComm, buf, (DWORD)max_len, &bytes_read, NULL)) {
        fprintf(stderr, "Error: ReadFile failed (error %lu)\n",
                (unsigned long)GetLastError());
        return -1;
    }

    return (int)bytes_read;
}

void serial_flush(serial_port_t *port)
{
    if (port && port->hComm != INVALID_HANDLE_VALUE) {
        PurgeComm(port->hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);
    }
}

bool serial_is_open(serial_port_t *port)
{
    return port && port->hComm != INVALID_HANDLE_VALUE;
}

void serial_list_ports(void)
{
    printf("Available COM ports:\n");
    int found = 0;

    for (int i = 1; i <= 256; i++) {
        char path[32];
        snprintf(path, sizeof(path), "\\\\.\\COM%d", i);

        HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                                0, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            printf("  COM%d  (available)\n", i);
            CloseHandle(h);
            found++;
        } else {
            DWORD err = GetLastError();
            if (err == ERROR_ACCESS_DENIED) {
                printf("  COM%d  (in use)\n", i);
                found++;
            }
            /* ERROR_FILE_NOT_FOUND = port doesn't exist, skip */
        }
    }

    if (found == 0) {
        printf("  No COM ports found. Connect a USB-to-RS485 adapter.\n");
    }
}

#else
/* ─── Stub for non-Windows platforms ── */
#error "Serial port implementation is Windows-only. For Linux, use termios."
#endif
