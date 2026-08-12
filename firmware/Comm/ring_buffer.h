#ifndef RING_BUFFER_H
#define RING_BUFFER_H
#include "stdbool.h"
#include "stdint.h"
void RingBuffer_Init(void);
bool RingBuffer_isEmpty(void);
bool RingBuffer_isFull(void);
bool RingBuffer_Writer(uint8_t data);
bool RingBuffer_Read(uint8_t* data);
#endif /* RING_BUFFER_H */
