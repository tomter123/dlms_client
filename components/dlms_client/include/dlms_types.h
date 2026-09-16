/*
 * DLMS/COSEM Protocol Type Definitions
 *
 * Core data types used across the DLMS client library.
 * No ESP-IDF or platform dependencies - pure C types.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Maximum limits ─────────────────────────────────────────────── */
#define DLMS_MAX_OBIS_ENTRIES      32
#define DLMS_MAX_PASSWORD_LEN      32
#define DLMS_MAX_NAME_LEN          48
#define DLMS_MAX_STR_VALUE_LEN     128
#define DLMS_OBIS_CODE_LEN         6
#define DLMS_OBIS_STR_LEN          32   /* "255.255.255.255.255.255\0" */
#define DLMS_RX_BUFFER_SIZE        2048
#define DLMS_TX_BUFFER_SIZE        512
#define DLMS_HDLC_FLAG             0x7E

/* ─── HDLC Control Bytes ─────────────────────────────────────────── */
#define HDLC_CTRL_SNRM             0x93  /* Set Normal Response Mode (P=1) */
#define HDLC_CTRL_UA               0x73  /* Unnumbered Acknowledge (F=1) */
#define HDLC_CTRL_DISC             0x53  /* Disconnect (P=1) */
#define HDLC_CTRL_DM               0x1F  /* Disconnected Mode */
#define HDLC_CTRL_FRMR             0x97  /* Frame Reject */

/* HDLC Frame Format Type 3 marker (upper nibble) */
#define HDLC_FORMAT_TYPE           0xA0

/* LLC sublayer headers */
#define DLMS_LLC_REQUEST_DSAP      0xE6
#define DLMS_LLC_REQUEST_SSAP      0xE6
#define DLMS_LLC_RESPONSE_DSAP     0xE6
#define DLMS_LLC_RESPONSE_SSAP     0xE7
#define DLMS_LLC_CONTROL           0x00

/* ─── DLMS APDU Tags ─────────────────────────────────────────────── */
#define DLMS_TAG_AARQ              0x60
#define DLMS_TAG_AARE              0x61
#define DLMS_TAG_GET_REQUEST       0xC0
#define DLMS_TAG_GET_RESPONSE      0xC4
#define DLMS_TAG_RELEASE_REQUEST   0x62
#define DLMS_TAG_RELEASE_RESPONSE  0x63

/* GET-Request / GET-Response subtypes */
#define DLMS_GET_REQUEST_NORMAL    0x01
#define DLMS_GET_REQUEST_NEXT      0x02
#define DLMS_GET_RESPONSE_NORMAL   0x01
#define DLMS_GET_RESPONSE_DATABLOCK 0x02
#define DLMS_GET_RESPONSE_LIST     0x03

/* ─── A-XDR Data Type Tags ───────────────────────────────────────── */
typedef enum {
    AXDR_NULL_DATA           = 0x00,
    AXDR_ARRAY               = 0x01,
    AXDR_STRUCTURE            = 0x02,
    AXDR_BOOLEAN             = 0x03,
    AXDR_BIT_STRING          = 0x04,
    AXDR_DOUBLE_LONG         = 0x05,   /* int32_t  */
    AXDR_DOUBLE_LONG_UNSIGNED = 0x06,  /* uint32_t */
    AXDR_OCTET_STRING        = 0x09,
    AXDR_VISIBLE_STRING      = 0x0A,
    AXDR_UTF8_STRING         = 0x0C,
    AXDR_INTEGER             = 0x0F,   /* int8_t   */
    AXDR_LONG                = 0x10,   /* int16_t  */
    AXDR_UNSIGNED            = 0x11,   /* uint8_t  */
    AXDR_LONG_UNSIGNED       = 0x12,   /* uint16_t */
    AXDR_LONG64              = 0x14,   /* int64_t  */
    AXDR_LONG64_UNSIGNED     = 0x15,   /* uint64_t */
    AXDR_ENUM                = 0x16,   /* uint8_t  */
    AXDR_FLOAT32             = 0x17,   /* float    */
    AXDR_FLOAT64             = 0x18,   /* double   */
    AXDR_DATE_TIME           = 0x19,   /* 12 bytes */
    AXDR_DATE                = 0x1A,   /* 5 bytes  */
    AXDR_TIME                = 0x1B,   /* 4 bytes  */
} axdr_type_t;

/* ─── DLMS Class IDs ─────────────────────────────────────────────── */
typedef enum {
    DLMS_CLASS_DATA              = 1,
    DLMS_CLASS_REGISTER          = 3,
    DLMS_CLASS_EXTENDED_REGISTER = 4,
    DLMS_CLASS_DEMAND_REGISTER   = 5,
    DLMS_CLASS_PROFILE_GENERIC   = 7,
    DLMS_CLASS_CLOCK             = 8,
} dlms_class_id_t;

/* ─── DLMS Protocol State Machine ────────────────────────────────── */
typedef enum {
    DLMS_STATE_IDLE = 0,
    DLMS_STATE_CONNECTING_HDLC,    /* SNRM sent, waiting for UA */
    DLMS_STATE_CONNECTING_DLMS,    /* AARQ sent, waiting for AARE */
    DLMS_STATE_CONNECTED,          /* Association established, ready for GET */
    DLMS_STATE_REQUEST_SENT,       /* GET-Request sent, waiting response */
    DLMS_STATE_READING_DATABLOCK,  /* Multi-block transfer in progress */
    DLMS_STATE_DISCONNECTING,      /* DISC sent, waiting UA/DM */
    DLMS_STATE_ERROR,
} dlms_state_t;

/* ─── Authentication Modes ───────────────────────────────────────── */
typedef enum {
    DLMS_AUTH_NONE = 0,
    DLMS_AUTH_LOW  = 1,   /* Cleartext password */
} dlms_auth_t;

/* ─── Parsed OBIS Reading (output of DLMS client) ────────────────── */
typedef struct {
    uint8_t    obis[DLMS_OBIS_CODE_LEN];
    char       obis_str[DLMS_OBIS_STR_LEN];
    char       name[DLMS_MAX_NAME_LEN];
    axdr_type_t data_type;
    union {
        float      float_val;
        double     double_val;
        int64_t    int_val;
        uint64_t   uint_val;
        bool       bool_val;
        char       str_val[DLMS_MAX_STR_VALUE_LEN];
    };
    int8_t     scaler;       /* 10^scaler multiplier */
    uint8_t    unit;         /* DLMS unit code */
    float      scaled_value; /* Final value after scaler applied */
    bool       valid;        /* true if successfully read */
    int64_t    timestamp_ms; /* Timestamp of last update (ms since boot) */
} dlms_reading_t;

/* ─── OBIS Register Definition (input configuration) ─────────────── */
typedef struct {
    uint8_t    obis[6];
    char       obis_str[32];
    uint16_t   short_name;     /* 16-bit Base Name for SN referencing */
    uint16_t   class_id;       /* 1=Data, 3=Register, 4=ExtendedRegister */
    uint8_t    attribute_id;   /* 2=value, 3=scaler_unit */
    int8_t     scaler;         /* Hardcoded fallback scaler (e.g. -3 for kWh) */
    char       name[32];
    char       unit_str[8];    /* "kWh", "V", "A", "W", etc. */
    char       device_class[16]; /* HA device class: "energy", "voltage", etc. */
} dlms_obis_entry_t;

/* ─── HDLC Parsed Frame ─────────────────────────────────────────── */
typedef struct {
    uint8_t  control;                    /* Control byte */
    uint8_t  info[DLMS_RX_BUFFER_SIZE];  /* Information field (after LLC) */
    size_t   info_len;                   /* Length of information field */
    bool     is_iframe;
    bool     is_rr;
    bool     is_ui;
    uint8_t  send_seq;    /* N(S) from I-frame */
    uint8_t  recv_seq;    /* N(R) from I-frame or RR */
} dlms_hdlc_frame_t;

/* ─── DLMS Unit Codes (IEC 62056-6-2) ────────────────────────────── */
#define DLMS_UNIT_YEAR             1
#define DLMS_UNIT_MONTH            2
#define DLMS_UNIT_WEEK             3
#define DLMS_UNIT_DAY              4
#define DLMS_UNIT_HOUR             5
#define DLMS_UNIT_MINUTE           6
#define DLMS_UNIT_SECOND           7
#define DLMS_UNIT_ACTIVE_POWER_W   27
#define DLMS_UNIT_APPARENT_POWER   28
#define DLMS_UNIT_REACTIVE_POWER   29
#define DLMS_UNIT_ACTIVE_ENERGY    30
#define DLMS_UNIT_APPARENT_ENERGY  31
#define DLMS_UNIT_REACTIVE_ENERGY  32
#define DLMS_UNIT_VOLTAGE          33
#define DLMS_UNIT_CURRENT          35
#define DLMS_UNIT_FREQUENCY        44
#define DLMS_UNIT_POWER_FACTOR     255

#ifdef __cplusplus
}
#endif

