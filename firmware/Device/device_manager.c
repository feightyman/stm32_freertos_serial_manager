#include "device_manager.h"


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
