#ifndef PROTOCOL_H
#define PROTOCOL_H
#include "stdint.h"
#include "stdbool.h"
#define PROTOCOL_MAX_DATA_LEN 32U
#define CMD_PING 0x01U
#define CMD_PING_RESP 0x81U   /* 响应约定：bit7 置位（0x01 | 0x80）*/
typedef enum
{
    WAIT_SOF = 0,
    READ_LEN,
    READ_CMD,
    READ_DATA,
    READ_CRC_H,
    READ_CRC_L
} ParserState_t;

typedef struct
{
    uint8_t len;
    uint8_t cmd;
    uint8_t data[PROTOCOL_MAX_DATA_LEN];
    uint8_t crc_hi;
    uint8_t crc_lo;
}ProtocolFrame_t;


typedef struct
{
    ParserState_t current_state;
    ProtocolFrame_t current_frame;
    uint8_t success_count;
}ProtocolParser_t;

void ProtocolParser_Init(ProtocolParser_t *parser);

bool ProtocolParser_InputByte(ProtocolParser_t *parser,
                              uint8_t byte,
                              ProtocolFrame_t *frame);

#endif /* PROTOCOL_H */
