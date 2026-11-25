#ifndef __USB_BSP_H__
#define __USB_BSP_H__

#include "main.h"

#define USB_BSP_OUTPUT_RATE_HZ  200U

void USB_BSP_Init(void);
void USB_BSP_SendAcceleration(const float accel_mps2[3]);

#endif /* __USB_BSP_H__ */
