#ifndef RING_BUFFER_H
#define RING_BUFFER_H
#include "stdbool.h"
#include "stdint.h"
/*
 * 256-byte 单例 FIFO，当前由 CommTask 在任务上下文中独占使用。
 * 模块内部不加锁；若由多个上下文访问，调用者必须自行串行化。
 */
/** 清空缓冲区并复位读写索引。 */
void RingBuffer_Init(void);
/** 返回缓冲区当前是否为空。 */
bool RingBuffer_isEmpty(void);
/** 返回缓冲区当前是否已占满 256 bytes。 */
bool RingBuffer_isFull(void);
/** 写入一个字节；已满时返回 false，且不覆盖未读数据。 */
bool RingBuffer_Writer(uint8_t data);
/** 读取最早写入的字节；data 必须有效，为空时返回 false 且不修改其指向的值。 */
bool RingBuffer_Read(uint8_t* data);
#endif /* RING_BUFFER_H */
