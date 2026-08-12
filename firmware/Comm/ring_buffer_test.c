#include "ring_buffer.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define TEST_CAPACITY 256U

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

static void TestEmptyRead(void)
{
    uint8_t data = 0xA5U;

    RingBuffer_Init();
    Check(RingBuffer_isEmpty(), "empty after init");
    Check(!RingBuffer_isFull(), "not full after init");
    Check(!RingBuffer_Read(&data), "reject read from empty buffer");
    Check(data == 0xA5U, "empty read does not modify output");
}

static void TestNormalWriteRead(void)
{
    uint8_t data = 0U;

    RingBuffer_Init();
    Check(RingBuffer_Writer(0x5AU), "normal write");
    Check(!RingBuffer_isEmpty(), "not empty after write");
    Check(RingBuffer_Read(&data), "normal read");
    Check(data == 0x5AU, "normal read data matches");
    Check(RingBuffer_isEmpty(), "empty after normal read");
}

static void TestFullAndOverflow(void)
{
    uint16_t index;
    uint8_t data = 0U;
    bool write_ok = true;
    bool read_ok = true;
    bool data_ok = true;

    RingBuffer_Init();

    for (index = 0U; index < TEST_CAPACITY; index++)
    {
        if (!RingBuffer_Writer((uint8_t)index))
        {
            write_ok = false;
            break;
        }
    }

    Check(write_ok, "write exactly to capacity");
    Check(RingBuffer_isFull(), "full after capacity writes");
    Check(!RingBuffer_Writer(0xA5U), "reject write beyond capacity");

    for (index = 0U; index < TEST_CAPACITY; index++)
    {
        if (!RingBuffer_Read(&data))
        {
            read_ok = false;
            break;
        }
        if (data != (uint8_t)index)
        {
            data_ok = false;
        }
    }

    Check(read_ok, "read all full-buffer data");
    Check(data_ok, "full-buffer data order matches");
    Check(RingBuffer_isEmpty(), "empty after full-buffer read");
}

static void TestWrapAround(void)
{
    uint16_t index;
    uint8_t data = 0U;
    bool operation_ok = true;
    bool data_ok = true;

    RingBuffer_Init();

    for (index = 0U; index < 200U; index++)
    {
        if (!RingBuffer_Writer((uint8_t)index))
        {
            operation_ok = false;
            break;
        }
    }

    for (index = 0U; index < 150U; index++)
    {
        if (!RingBuffer_Read(&data))
        {
            operation_ok = false;
            break;
        }
        if (data != (uint8_t)index)
        {
            data_ok = false;
        }
    }

    for (index = 200U; index < 350U; index++)
    {
        if (!RingBuffer_Writer((uint8_t)index))
        {
            operation_ok = false;
            break;
        }
    }

    for (index = 150U; index < 350U; index++)
    {
        if (!RingBuffer_Read(&data))
        {
            operation_ok = false;
            break;
        }
        if (data != (uint8_t)index)
        {
            data_ok = false;
        }
    }

    Check(operation_ok, "wrap-around operations succeed");
    Check(data_ok, "wrap-around data order matches");
    Check(RingBuffer_isEmpty(), "empty after wrap-around test");
}

int main(void)
{
    TestEmptyRead();
    TestNormalWriteRead();
    TestFullAndOverflow();
    TestWrapAround();

    if (failure_count == 0U)
    {
        printf("ALL TESTS PASSED\n");
        return 0;
    }

    printf("TESTS FAILED: %lu\n", (unsigned long)failure_count);
    return 1;
}
