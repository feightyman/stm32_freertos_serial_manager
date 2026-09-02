#include "device_manager.h"


/* 状态只保存在 RAM 中，由 DeviceTask 初始化并串行访问。 */
static DeviceState_t state;
void DeviceManager_Init(void)
{
    state.mode = MODE_IDLE;
}

bool DeviceManager_SetMode(uint8_t m_mode)
{
    if (m_mode == (uint8_t)MODE_IDLE || m_mode == (uint8_t)MODE_ACTIVE)
    {
        state.mode = (DeviceMode_t)m_mode;
        return true;
    }
    return false;
}

DeviceState_t DeviceManager_GetState(void)
{
    return state;
}
