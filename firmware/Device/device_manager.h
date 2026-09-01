#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H
#include "stdbool.h"
#include "stdint.h"
typedef enum {
    MODE_IDLE = 0x00,
    MODE_ACTIVE = 0x01
}DeviceMode_t;

typedef struct
{
    DeviceMode_t mode;
}DeviceState_t;

void DeviceManager_Init(void);

bool DeviceManager_SetMode(uint8_t m_mode);

DeviceState_t DeviceManager_GetState(void);
#endif /* DEVICE_MANAGER_H */
