#ifndef PRINTER_H
#define PRINTER_H

#include <user_interface.h>

typedef enum printer_state_t {
    SYNC1,
    SYNC2,
    CMD,
    COMPRESS,
    LEN1,
    LEN2,
    PAYLOAD,
    CHECKSUM1,
    CHECKSUM2,
    ACK,
    STAT
} printer_state_t;

typedef enum printer_cmd_t {
    NONE    = -1,
    INIT    = 0x01,
    DATA    = 0x04,
    PRINT   = 0x02,
    INQUIRY = 0x0F
} printer_cmd_t;

typedef struct printer_t {
    printer_state_t state;
    printer_cmd_t cmd;

    uint8_t inp;
    uint8_t out;

    int bit;

    uint16_t payload_len;
    
    char http_header[300];
    uint8_t image[6400];
    int image_pos;
} printer_t;

#endif