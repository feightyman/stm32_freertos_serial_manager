#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H
#include "stdbool.h"
#include "stdint.h"
/* DeviceManager 保存单例运行期状态，当前由 DeviceTask 串行访问，不负责持久化。 */
/* 设备当前仅有空闲和活动两种可设置模式。 */
typedef enum {
    MODE_IDLE = 0x00,
    MODE_ACTIVE = 0x01
}DeviceMode_t;

/* DeviceManager_GetState() 以值拷贝方式返回该状态快照。 */
typedef struct
{
    DeviceMode_t mode;
}DeviceState_t;

/** 将运行期状态初始化为 MODE_IDLE。 */
void DeviceManager_Init(void);

/**
 * @brief 设置运行期模式。
 * @param m_mode 待设置的 DeviceMode_t 数值。
 * @return m_mode 为 MODE_IDLE 或 MODE_ACTIVE 时返回 true；非法值返回 false，状态保持不变。
 * @note 当前由 DeviceTask 串行调用，模块本身不提供线程同步或持久化。
 */
bool DeviceManager_SetMode(uint8_t m_mode);

/** 返回当前运行期状态的值拷贝。 */
DeviceState_t DeviceManager_GetState(void);
#endif /* DEVICE_MANAGER_H */
