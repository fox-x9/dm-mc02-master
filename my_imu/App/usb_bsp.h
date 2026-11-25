#ifndef __USB_BSP_H__
#define __USB_BSP_H__

#include "main.h"

#define USB_BSP_OUTPUT_RATE_HZ  1600U

/**
 * @brief 初始化 USB CDC 设备。
 */
void USB_BSP_Init(void);

/**
 * @brief 将一个三轴加速度样本加入 USB 发送批次。
 *
 * 函数不会等待 USB 完成；主机未打开或端点忙时直接丢弃当前批次，
 * 以保证采样和 LCD 主流程不被 USB 阻塞。
 */
void USB_BSP_SendAcceleration(const float accel_mps2[3]);

#endif /* __USB_BSP_H__ */
