#ifndef CRC16_H
#define CRC16_H
#include "stdint.h"
#define CRC16_CCITT_FALSE_INIT 0xFFFFU

uint16_t CRC16_CCITT_FALSE_Calc(const uint8_t* data, uint16_t length);

#endif
