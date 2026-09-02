#include "dlms_helpers.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

bool dlms_obis_str_to_bytes(const char *str, uint8_t obis[6]) {
    if (!str || !obis) return false;
    
    int parsed = sscanf(str, "%hhu.%hhu.%hhu.%hhu.%hhu.%hhu", 
                        &obis[0], &obis[1], &obis[2], 
                        &obis[3], &obis[4], &obis[5]);
    
    return parsed == 6;
}

void dlms_obis_bytes_to_str(const uint8_t obis[6], char *str, size_t str_len) {
    if (!str || str_len == 0) return;
    
    snprintf(str, str_len, "%u.%u.%u.%u.%u.%u", 
             obis[0], obis[1], obis[2], 
             obis[3], obis[4], obis[5]);
}

bool dlms_obis_match(const uint8_t a[6], const uint8_t b[6]) {
    return memcmp(a, b, 6) == 0;
}

const char *dlms_unit_to_str(uint8_t unit) {
    switch (unit) {
        case DLMS_UNIT_YEAR: return "y";
        case DLMS_UNIT_MONTH: return "mo";
        case DLMS_UNIT_WEEK: return "wk";
        case DLMS_UNIT_DAY: return "d";
        case DLMS_UNIT_HOUR: return "h";
        case DLMS_UNIT_MINUTE: return "min";
        case DLMS_UNIT_SECOND: return "s";
        case DLMS_UNIT_ACTIVE_POWER_W: return "W";
        case DLMS_UNIT_APPARENT_POWER: return "VA";
        case DLMS_UNIT_REACTIVE_POWER: return "var";
        case DLMS_UNIT_ACTIVE_ENERGY: return "Wh";
        case DLMS_UNIT_APPARENT_ENERGY: return "VAh";
        case DLMS_UNIT_REACTIVE_ENERGY: return "varh";
        case DLMS_UNIT_VOLTAGE: return "V";
        case DLMS_UNIT_CURRENT: return "A";
        case DLMS_UNIT_FREQUENCY: return "Hz";
        case 255: return ""; // No unit
        default: return "";
    }
}

const char *dlms_state_to_str(dlms_state_t state) {
    switch (state) {
        case DLMS_STATE_IDLE: return "IDLE";
        case DLMS_STATE_CONNECTING_HDLC: return "CONNECTING_HDLC";
        case DLMS_STATE_CONNECTING_DLMS: return "CONNECTING_DLMS";
        case DLMS_STATE_CONNECTED: return "CONNECTED";
        case DLMS_STATE_REQUEST_SENT: return "REQUEST_SENT";
        case DLMS_STATE_READING_DATABLOCK: return "READING_DATABLOCK";
        case DLMS_STATE_DISCONNECTING: return "DISCONNECTING";
        case DLMS_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}
