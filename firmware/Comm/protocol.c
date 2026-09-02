#include "protocol.h"

/* 本模块只负责从字节流恢复帧边界；CRC 校验和命令分发分别由上层任务完成。 */
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
        /* SOF 之前的无关字节不会改变 Parser 状态。 */
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
            /* 非法 LEN 本身又是 SOF 时，把它作为下一候选帧的起点。 */
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
        parser->success_count = 0U;
        
        /* LEN 为 0 的帧没有 DATA，命令字节后直接接收 CRC。 */
        parser->current_state = (parser->current_frame.len == 0U) ? READ_CRC_H : READ_DATA;
    }
        break;
    case READ_DATA:
    {
        parser->current_frame.data[parser->success_count] = byte;
        parser->success_count++;
        if (parser->success_count == parser->current_frame.len)
        {
            parser->current_state++;
        }
    }
    break;
    case READ_CRC_H:
    {
        parser->current_frame.crc_hi = byte;
        parser->current_state++;
    }
    break;
    case READ_CRC_L:
    {
        parser->current_frame.crc_lo = byte;
        /* 输出接收值后立即复位，以便下一个输入字节可以开始新帧。 */
        *frame = parser->current_frame;
        parser->success_count = 0U;
        parser->current_state = WAIT_SOF;
        return true;
    }
    default:
        return false;
    }
    return false;
}
