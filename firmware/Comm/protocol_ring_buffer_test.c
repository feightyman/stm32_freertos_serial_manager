/* PC 侧集成测试：模拟一个协议帧被两个接收批次分割后经 Ring Buffer 送入 Parser。 */
#include "ring_buffer.h"
#include "protocol.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
static uint32_t failure_count = 0U;

static void Check(bool condition, const char *name)
{
    if (condition)
    {
        printf("[PASS] %s\n", name);
    }
    else
    {
        printf("[FAIL] %s\n", name);
        failure_count++;
    }
}

static void TestCompleteSingleFrame(void)
{
    /* 第一批停在 DATA[0]，第二批补齐其余 DATA 和 CRC。 */
    static const uint8_t batch_1[] = {
        0xAAU, 0x03U, 0x10U, 0x11U
    };
    static const uint8_t batch_2[] = {
        0x22U, 0x33U, 0xF6, 0x9E
    };
    const size_t batch1_length = sizeof(batch_1) / sizeof(batch_1[0]);
    const size_t batch2_length = sizeof(batch_2) / sizeof(batch_2[0]);
    ProtocolParser_t parser;
    ProtocolFrame_t frame = {0};
    uint32_t frame_count = 0U;
    size_t index;
    uint8_t byte = 0U;
    RingBuffer_Init();
    ProtocolParser_Init(&parser);

    for (index = 0U; index < batch1_length; index++)
    {
        Check(RingBuffer_Writer(batch_1[index]),
      "write first batch byte");
    }

    while (RingBuffer_Read(&byte))
    {
        bool result = ProtocolParser_InputByte(&parser, byte, &frame);

        if (result)
        {
            frame_count++;
        }
    }
    /* 批次耗尽不应重置 Parser，未完成帧的状态必须保留。 */
    Check(frame_count == 0U, "frame count 0U");
    Check(RingBuffer_isEmpty(), "ringbuffer empty");
    Check(parser.current_state == READ_DATA, "parser state READ_DATA");
    Check(parser.success_count == 1U, "parser count 1U");
    for (index = 0U; index < batch2_length; index++)
    {
        Check(RingBuffer_Writer(batch_2[index]),
      "write second batch byte");
    }

    while (RingBuffer_Read(&byte))
    {
        bool result = ProtocolParser_InputByte(&parser, byte, &frame);

        if (result)
        {
            frame_count++;
        }
    }

    Check(frame_count == 1U, "exactly one frame completed");
    Check(frame.len == 3U, "frame length matches");
    Check(frame.cmd == 0x10U, "frame command matches");
    Check(frame.data[0] == 0x11U, "frame data[0] matches");
    Check(frame.data[1] == 0x22U, "frame data[1] matches");
    Check(frame.data[2] == 0x33U, "frame data[2] matches");
    Check(frame.crc_hi == 0xF6U, "crc_hi matches");
    Check(frame.crc_lo == 0x9EU, "crc_lo matches");
    Check(RingBuffer_isEmpty(), "ringbuffer empty");
    Check(parser.current_state == WAIT_SOF, "parser returns to WAIT_SOF");
    Check(parser.success_count == 0U, "parser data count resets");
}

int main(void)
{
    TestCompleteSingleFrame();
    if (failure_count == 0U)
    {
        printf("ALL TESTS PASSED\n");
        return 0;
    }

    printf("TESTS FAILED: %lu\n", (unsigned long)failure_count);
    return 1;
}
