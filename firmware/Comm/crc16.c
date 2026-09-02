#include "crc16.h"
#include <stdint.h>
/* CCITT-FALSE 使用 0x1021 多项式，并按非反射方式从最高位开始处理。 */
#define CRC16_CCITT_FALSE_POLY 0x1021U

uint16_t CRC16_CCITT_FALSE_Calc(const uint8_t* data, uint16_t length)
{
    uint16_t crc = CRC16_CCITT_FALSE_INIT;

    if (length == 0)
    {
        /* 空输入不会解引用 data，因此调用者可以传入 NULL。 */
        return crc;
    }
    for (uint16_t i = 0U; i < length; i++)
    {
        /* 非反射算法先把当前输入字节对齐到 CRC 寄存器的高 8 位。 */
        crc ^= (uint16_t)data[i] << 8U;
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1U) ^ CRC16_CCITT_FALSE_POLY);
            }
            else
            {
                crc = (uint16_t)(crc << 1U);
            }
        }
    }
    return crc;
}
