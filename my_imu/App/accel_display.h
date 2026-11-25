#ifndef MY_IMU_ACCEL_DISPLAY_H
#define MY_IMU_ACCEL_DISPLAY_H

#include <stdint.h>

void AccelDisplay_Init(void);
void AccelDisplay_ProcessSample(const float accel_mps2[3]);
void AccelDisplay_Update(void);

#endif
