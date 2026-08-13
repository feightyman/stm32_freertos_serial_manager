#ifndef PROTOCOL_H
#define PROTOCOL_H
#include "stdint.h"
#include "stdbool.h"
#define PROTOCOL_MAX_DATA_LEN 32U

typedef enum
{
    WAIT_SOF = 0,
    READ_LEN,
    READ_CMD,
    READ_DATA
} ParserState_t;

typedef struct
{
    uint8_t len;
    uint8_t cmd;
    uint8_t data[PROTOCOL_MAX_DATA_LEN];
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
