#ifndef PROTOCOL_H
#define PROTOCOL_H
#include "stdint.h"
#include "stdbool.h"
/*
 * 线上帧格式：AA | LEN | CMD | DATA[LEN] | CRC_H | CRC_L。
 * LEN 只表示 DATA 长度；CRC 由上层对 [LEN, CMD, DATA...] 校验，并按高字节在前传输。
 */
#define PROTOCOL_MAX_DATA_LEN 32U
/* 成功请求—响应关系：PING 0x01→0x81，SET_MODE 0x02→0x82，GET_STATUS 0x03→0x83。 */
#define CMD_PING 0x01U
#define CMD_GET_STATUS 0x03U
#define RESP_GET_STATUS 0x83U
#define CMD_PING_RESP 0x81U
#define CMD_SET_MODE 0x02U
#define RESP_SET_MODE 0x82U

/* 错误响应 DATA 固定为 [failed_cmd, error_code]。 */
#define RESP_ERROR 0xFFU
#define ERR_INVALID_LENGTH 0x01U
#define ERR_INVALID_PARAM 0x02U
#define ERR_UNKNOWN_CMD 0x03U

/* Parser 每接收一个字节推进一个状态，并在跨批次调用时保留当前状态。 */
typedef enum
{
    WAIT_SOF = 0,
    READ_LEN,
    READ_CMD,
    READ_DATA,
    READ_CRC_H,
    READ_CRC_L
} ParserState_t;

/* Parser 输出不保存 SOF；其余线上字段按接收结果分别保存在下列成员中。 */
typedef struct
{
    uint8_t len;                              /* DATA 的实际字节数。 */
    uint8_t cmd;                              /* 请求或响应命令码。 */
    uint8_t data[PROTOCOL_MAX_DATA_LEN];      /* Parser 仅写入前 len 个字节。 */
    uint8_t crc_hi;                           /* 接收到的 CRC 高字节。 */
    uint8_t crc_lo;                           /* 接收到的 CRC 低字节。 */
}ProtocolFrame_t;


/* Parser 实例必须由单一执行上下文串行访问，不包含内部同步。 */
typedef struct
{
    ParserState_t current_state;
    ProtocolFrame_t current_frame;
    uint8_t success_count;                    /* 当前帧已经接收的 DATA 字节数。 */
}ProtocolParser_t;

/** 将有效的 parser 实例清空并置为等待 SOF 的初始状态。 */
void ProtocolParser_Init(ProtocolParser_t *parser);

/**
 * @brief 向流式 Parser 输入一个字节。
 * @param parser 已初始化且由调用者独占的 Parser 实例。
 * @param byte 当前输入字节。
 * @param frame 必须指向有效输出位置；仅在返回 true 时写入。
 * @return 收齐 CRC_L 并输出一帧时返回 true，否则返回 false。
 * @note 返回 true 只表示帧结构完整，不表示 CRC 或命令语义有效。
 */
bool ProtocolParser_InputByte(ProtocolParser_t *parser,
                              uint8_t byte,
                              ProtocolFrame_t *frame);

#endif /* PROTOCOL_H */
