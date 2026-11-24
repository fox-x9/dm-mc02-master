#ifndef __UART_BSP_H__
#define __UART_BSP_H__

#include "main.h"

void UART_BSP_Init(void);
void UART_BSP_SendAcceleration(const float accel_mps2[3]);

#endif /* __UART_BSP_H__ */
