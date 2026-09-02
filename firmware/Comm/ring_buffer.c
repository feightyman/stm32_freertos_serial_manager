#include "ring_buffer.h"
#include <stdint.h>
#include <stdbool.h>
#define CAPACITY 256

/* head 指向下一写入位置，tail 指向下一读取位置，count 用于区分空与满。 */
static uint8_t RingBuffer[CAPACITY];
static volatile uint16_t head = 0U;
static volatile uint16_t tail = 0U;
static volatile uint16_t count = 0U;
void RingBuffer_Init()
{
    for (int i = 0U ; i < CAPACITY; i++)
    {
        RingBuffer[i] = 0x00;
    }
    head = 0;
    tail = 0;
    count = 0;
}

bool RingBuffer_isEmpty()
{
    if (count == 0)
    {
        return true;
    }
    return false;
}
bool RingBuffer_isFull()
{
    if (count == CAPACITY)
    {
        return true;
    }
    return false;
}

bool RingBuffer_Writer(uint8_t data)
{
    if (RingBuffer_isFull())
    {
        return false;
    }
    RingBuffer[head] = data;
    head++;
    /* 索引到达数组末尾后回绕，但不改变尚未读取的数据。 */
    head == CAPACITY ? head = 0 : head;
    count++;
    return true;
}

bool RingBuffer_Read(uint8_t* data)
{
    if (RingBuffer_isEmpty())
    {
        return false;
    }
    *data = RingBuffer[tail];
    tail++;
    /* 读索引与写索引使用相同的环形回绕规则。 */
    tail == CAPACITY ? tail = 0 : tail;
    count--;
    return true;
}
