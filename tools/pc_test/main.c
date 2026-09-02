#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "serial_port.h"
#include "dlms_pc_client.h"

int main(int argc, char **argv) {
    printf("========================================\n");
    printf(" DLMS/COSEM PC Test Tool\n");
    printf("========================================\n\n");

    if (argc < 2) {
        printf("Usage: %s <COM_PORT> [BAUD_RATE]\n", argv[0]);
        printf("Example: %s COM3 9600\n\n", argv[0]);
        serial_list_ports();
        return 1;
    }

    const char *port_name = argv[1];
    uint32_t baud = (argc > 2) ? atoi(argv[2]) : 9600;

    serial_config_t s_cfg = {
        .port_name = port_name,
        .baud_rate = baud,
        .data_bits = 8,
        .parity = SERIAL_PARITY_NONE,
        .stop_bits = 1
    };

    serial_port_t *port = serial_open(&s_cfg);
    if (!port) {
        return 1;
    }

    pc_client_config_t cfg = {
        .port = port,
        .client_address = 32,
        .server_logical = 1,
        .server_physical = 4555,
        .server_addr_len = 4,
        .auth_mode = DLMS_AUTH_LOW,
        .timeout_ms = 3000
    };
    strncpy(cfg.password, "00000000", sizeof(cfg.password));

    dlms_obis_entry_t obis_list[] = {
        { {1,0,1,8,0,255}, "1.0.1.8.0.255", 42768, DLMS_CLASS_REGISTER, 2, "Energy Import Total", "kWh", "energy" },
        { {1,0,1,8,1,255}, "1.0.1.8.1.255", 58112, DLMS_CLASS_REGISTER, 2, "Energy Import T1", "kWh", "energy" },
        { {1,0,1,8,2,255}, "1.0.1.8.2.255", 58208, DLMS_CLASS_REGISTER, 2, "Energy Import T2", "kWh", "energy" },
        { {1,0,32,7,0,255}, "1.0.32.7.0.255", 29216, DLMS_CLASS_REGISTER, 2, "Voltage L1", "V", "voltage" },
        { {1,0,72,7,0,255}, "1.0.72.7.0.255", 21872, DLMS_CLASS_REGISTER, 2, "Voltage L3", "V", "voltage" }
    };
    uint8_t obis_count = sizeof(obis_list) / sizeof(obis_list[0]);
    dlms_reading_t readings[5];

    printf("\nStarting DLMS polling cycle on %s...\n", port_name);
    
    if (pc_dlms_poll(&cfg, obis_list, obis_count, readings)) {
        printf("\n--- Results ---\n");
        for (uint8_t i = 0; i < obis_count; i++) {
            if (readings[i].data_type > 0) {
                printf("%s (%s): %lld %s (raw)\n", obis_list[i].name, obis_list[i].obis_str, (long long)readings[i].int_val, obis_list[i].unit_str);
            } else {
                printf("%s (%s): FAILED\n", obis_list[i].name, obis_list[i].obis_str);
            }
        }
        printf("---------------\n");
    } else {
        printf("\nPolling cycle failed.\n");
    }

    serial_close(port);
    return 0;
}

