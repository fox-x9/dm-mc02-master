#ifndef MY_IMU_ACCEL_DISPLAY_H
#define MY_IMU_ACCEL_DISPLAY_H

#include <stdint.h>

/**
 * @brief 初始化 LCD 静态界面和振动统计状态。
 */
void AccelDisplay_Init(void);

/**
 * @brief 输入一个已换算为 m/s^2 的 IMU 样本。
 *
 * 函数连续累计 160 个样本后锁存一次 RMS 结果。调用频率应为 1600 Hz，
 * 因而一次统计窗口固定为 100 ms。
 */
void AccelDisplay_ProcessSample(const float accel_mps2[3]);

/**
 * @brief 将最近一次锁存的 RMS 结果刷新到 LCD。
 *
 * 该函数只在数值发生 0.01 m/s^2 变化时更新数字，以减少 SPI 写屏时间。
 */
void AccelDisplay_Update(void);

#endif
