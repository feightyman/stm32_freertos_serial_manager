/* PC 侧 CRC 单元测试：覆盖标准向量、协议向量以及空输入边界。 */
#include "crc16.h"
#include "stdbool.h"
#include "stdio.h"

static uint32_t failure_count = 0U;

static void Check(bool condition, const char* name)
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

static void StdCheck(void)
{
    static const uint8_t input[] = "123456789";
    uint16_t length = (uint16_t)(sizeof(input) - 1U);
    uint16_t crc = CRC16_CCITT_FALSE_Calc(input, length);
    Check(crc == 0x29B1, "CRC 0x29B1");
}

static void UserCheck(void)
{
    /* 输入按协议 CRC 范围组织为 [LEN, CMD, DATA...]，不包含 SOF。 */
    uint8_t input[] = {
        0x03U, 0x10U, 0x11U, 0x22U, 0x33U
    };
    uint16_t length = sizeof(input) / sizeof(input[0]);
    uint16_t crc = CRC16_CCITT_FALSE_Calc(input, length);
    Check(crc == 0xF69E, "CRC 0xF69E");
}

static void PingCheck(void)
{
    /* PING 无 DATA，因此 CRC 输入只有 LEN=0 和 CMD=0x01。 */
    uint8_t input[] = {
        0x00U, 0x01U
    };
    uint16_t length = sizeof(input) / sizeof(input[0]);
    uint16_t crc = CRC16_CCITT_FALSE_Calc(input, length);
    Check(crc == 0x0D2E, "PING CHECK");
}

static void EmptyCheck(void)
{

    /* length=0 时实现不得访问 data，允许使用 NULL 验证初始值。 */
    uint16_t crc = CRC16_CCITT_FALSE_Calc(NULL, 0U);
    Check(crc == 0xFFFF, "EMPTY CHECK");
}

int main(void)
{
    StdCheck();
    UserCheck();
    PingCheck();
    EmptyCheck();

    if (failure_count == 0U)
    {
        printf("ALL TESTS PASSED\n");
        return 0;
    }

    printf("TESTS FAILED: %lu\n", (unsigned long)failure_count);
    return 1;
}
