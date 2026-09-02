#ifndef CRC16_H
#define CRC16_H
#include "stdint.h"
/* CRC-16/CCITT-FALSE 的初始值；空输入的计算结果也保持为该值。 */
#define CRC16_CCITT_FALSE_INIT 0xFFFFU

/**
 * @brief 按 CRC-16/CCITT-FALSE 计算校验值。
 * @param data 输入字节序列；当 length 大于 0 时必须指向有效内存。
 * @param length 参与计算的字节数；为 0 时允许 data 为 NULL。
 * @return 16 位 CRC。协议层对 [LEN, CMD, DATA...] 调用本函数，SOF 与 CRC 字段不参与计算。
 * @note 参数为 poly=0x1021、init=0xFFFF、RefIn/RefOut=false、xorout=0x0000。
 */
uint16_t CRC16_CCITT_FALSE_Calc(const uint8_t* data, uint16_t length);

#endif
