#ifndef __UART_BSP_H__
#define __UART_BSP_H__

#include "main.h"

#define UART_BSP_OUTPUT_RATE_HZ  500U

void UART_BSP_Init(void);
void UART_BSP_SendAcceleration(uint64_t timestamp_us, const float accel_mps2[3]);

#endif /* __UART_BSP_H__ */
