#include "protocol.h"

void ProtocolParser_Init(ProtocolParser_t *parser)
{
    parser->current_state = WAIT_SOF;
    parser->success_count = 0U;
    parser->current_frame = (ProtocolFrame_t){0};
}

bool ProtocolParser_InputByte(ProtocolParser_t* parser, uint8_t byte, ProtocolFrame_t* frame)
{
    switch (parser->current_state)
    {
    case WAIT_SOF:
    {
        if (byte == 0xAA)
        {
            parser->success_count = 0U;
            parser->current_state++;
        }
        else
        {
            return false;
        }
    }
        break;
    case READ_LEN:
    {
        if (byte <= PROTOCOL_MAX_DATA_LEN)
        {
            parser->current_frame.len = byte;
            parser->current_state++;
        }
        else if (byte == 0xAA)
        {
            parser->success_count = 0U;
            parser->current_state = READ_LEN;
        }
        else
        {
            parser->current_state = WAIT_SOF;
        }
    }
        break;
    case READ_CMD:
    {
        parser->current_frame.cmd = byte;
        if (parser->current_frame.len == 0U)
        {
            *frame = parser->current_frame;
            parser->success_count = 0U;
            parser->current_state = WAIT_SOF;
            return true;
        }
        else
        {
            parser->current_state++;
        }
    }
        break;
    case READ_DATA:
    {
        parser->current_frame.data[parser->success_count] = byte;
        parser->success_count++;
        if (parser->success_count == parser->current_frame.len)
        {
            *frame = parser->current_frame;
            parser->success_count = 0U;
            parser->current_state = WAIT_SOF;
            return true;
        }
    }
        break;
    default:
        return false;
    }
    return false;
}
