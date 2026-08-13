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
    static const uint8_t input[] = {
        0xAAU, 0x03U, 0x10U, 0x11U, 0x22U, 0x33U
    };
    const size_t input_length = sizeof(input) / sizeof(input[0]);
    ProtocolParser_t parser;
    ProtocolFrame_t frame = {0};
    bool completed = false;
    bool completed_early = false;
    size_t index;

    ProtocolParser_Init(&parser);

    for (index = 0U; index < input_length; index++)
    {
        bool result = ProtocolParser_InputByte(&parser, input[index], &frame);

        if (index < (input_length - 1U))
        {
            if (result)
            {
                completed_early = true;
            }
        }
        else
        {
            completed = result;
        }
    }

    Check(!completed_early, "no frame before final byte");
    Check(completed, "frame completes on final byte");
    Check(frame.len == 3U, "frame length matches");
    Check(frame.cmd == 0x10U, "frame command matches");
    Check(frame.data[0] == 0x11U, "frame data[0] matches");
    Check(frame.data[1] == 0x22U, "frame data[1] matches");
    Check(frame.data[2] == 0x33U, "frame data[2] matches");
    Check(parser.current_state == WAIT_SOF, "parser returns to WAIT_SOF");
    Check(parser.success_count == 0U, "parser data count resets");
}

static void TestPartialFrame(void)
{
    static const uint8_t input[] = {
    0xAAU, 0x03U, 0x10U, 0x11U, 0x22U, 0x33U
    };
    ProtocolParser_t parser;
    ProtocolFrame_t frame = { 0 };
    uint8_t index;
    bool completed = false;
    bool completed_early = false;
    ProtocolParser_Init(&parser);
    for (index = 0; index < 4; index++)
    {
        bool result = ProtocolParser_InputByte(&parser, input[index], &frame);
        if (result)
        {
            completed_early = true;
        }
    }
    Check(parser.current_state == READ_DATA, "parser state matches");
    Check(parser.success_count == 1U, "success count matches");
    for (index = 4; index < 6; index++)
    {
        bool result = ProtocolParser_InputByte(&parser, input[index], &frame);
        if (index < 5U)
        {
            if (result)
            {
                completed_early = true;
            }
        }
        else
        {
            completed = result;
        }
    }
    Check(!completed_early, "no frame before final byte");
    Check(completed, "frame completes on final byte");
    Check(frame.len == 3U, "frame length matches");
    Check(frame.cmd == 0x10U, "frame command matches");
    Check(frame.data[0] == 0x11U, "frame data[0] matches");
    Check(frame.data[1] == 0x22U, "frame data[1] matches");
    Check(frame.data[2] == 0x33U, "frame data[2] matches");
    Check(parser.current_state == WAIT_SOF, "parser returns to WAIT_SOF");
    Check(parser.success_count == 0U, "parser data count resets");
}

static void TestConsecutiveFrames(void)
{
    static const uint8_t input[] = {
        0xAAU, 0x02U, 0x10U, 0x11U, 0x22U, 0xAAU, 0x01U, 0x20U, 0x33U
    };
    const size_t input_length = sizeof(input) / sizeof(input[0]);
    ProtocolParser_t parser;
    ProtocolFrame_t frame = { 0 };
    ProtocolFrame_t next_frame = { 0 };
    bool completed = false;
    bool completed_early = false;
    bool next_completed = false;
    bool next_completed_early = false;
    size_t index;

    ProtocolParser_Init(&parser);

    for (index = 0U; index < 5U; index++)
    {
        bool result = ProtocolParser_InputByte(&parser, input[index], &frame);

        if (index < 4U)
        {
            if (result)
            {
                completed_early = true;
            }
        }
        else
        {
            completed = result;
        }
    }
    for (index = 5U; index < input_length; index++)
    {
        bool result = ProtocolParser_InputByte(&parser, input[index], &next_frame);
        if (index < (input_length - 1U))
        {
            if (result)
            {
                next_completed_early = true;
            }
        }
        else
        {
            next_completed = result;
        }
    }

    Check((!completed_early) && (!next_completed_early), "no frame before final byte");
    Check(completed && next_completed, "frame completes on final byte");
    Check(frame.len == 2U, "frame length matches");
    Check(frame.cmd == 0x10U, "frame command matches");
    Check(frame.data[0] == 0x11U, "frame data[0] matches");
    Check(frame.data[1] == 0x22U, "frame data[1] matches");

    Check(next_frame.len == 1U, "next frame length matches");
    Check(next_frame.cmd == 0x20U, "next frame command matches");
    Check(next_frame.data[0] == 0x33U, "next frame data[0] matches");

    Check(parser.current_state == WAIT_SOF, "parser returns to WAIT_SOF");
    Check(parser.success_count == 0U, "parser data count resets");
}

static void TestGarbageBeforeValidFrame(void)
{
    static const uint8_t input[] = {
        0x00U, 0x55, 0xFF, 0x7E,
        0xAAU, 0x02U, 0x30U, 0x11U, 0x22U
    };
    const size_t input_length = sizeof(input) / sizeof(input[0]);
    ProtocolParser_t parser;
    ProtocolFrame_t frame = {0};
    bool completed = false;
    bool completed_early = false;
    size_t index;

    ProtocolParser_Init(&parser);

    for (index = 0U; index < input_length; index++)
    {
        bool result = ProtocolParser_InputByte(&parser, input[index], &frame);

        if (index < (input_length - 1U))
        {
            if (result)
            {
                completed_early = true;
            }
        }
        else
        {
            completed = result;
        }
    }

    Check(!completed_early, "no frame before final byte");
    Check(completed, "frame completes on final byte");
    Check(frame.len == 2U, "frame length matches");
    Check(frame.cmd == 0x30U, "frame command matches");
    Check(frame.data[0] == 0x11U, "frame data[0] matches");
    Check(frame.data[1] == 0x22U, "frame data[1] matches");

    Check(parser.current_state == WAIT_SOF, "parser returns to WAIT_SOF");
    Check(parser.success_count == 0U, "parser data count resets");
}

static void TestInvalidLengthRecovery(void)
{
    static const uint8_t input[] = {
        0xAAU, 0x21U,
        0xAAU, 0x02U, 0x40U, 0x11U, 0x22U
    };
    const size_t input_length = sizeof(input) / sizeof(input[0]);
    ProtocolParser_t parser;
    ProtocolFrame_t frame = {0};
    bool completed = false;
    bool completed_early = false;
    size_t index;

    ProtocolParser_Init(&parser);

    for (index = 0U; index < input_length; index++)
    {
        bool result = ProtocolParser_InputByte(&parser, input[index], &frame);

        if (index < (input_length - 1U))
        {
            if (result)
            {
                completed_early = true;
            }
        }
        else
        {
            completed = result;
        }
    }

    Check(!completed_early, "no frame before final byte");
    Check(completed, "frame completes on final byte");
    Check(frame.len == 2U, "frame length matches");
    Check(frame.cmd == 0x40U, "frame command matches");
    Check(frame.data[0] == 0x11U, "frame data[0] matches");
    Check(frame.data[1] == 0x22U, "frame data[1] matches");
    Check(parser.current_state == WAIT_SOF, "parser returns to WAIT_SOF");
    Check(parser.success_count == 0U, "parser data count resets");
}

int main(void)
{
    TestCompleteSingleFrame();
    TestPartialFrame();
    TestConsecutiveFrames();
    TestGarbageBeforeValidFrame();
    TestInvalidLengthRecovery();
    if (failure_count == 0U)
    {
        printf("ALL TESTS PASSED\n");
        return 0;
    }

    printf("TESTS FAILED: %lu\n", (unsigned long)failure_count);
    return 1;
}
