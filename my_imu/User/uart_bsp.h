#ifndef __UART_BSP_H__
#define __UART_BSP_H__

#include "main.h"

void UART_BSP_Init(void);
void UART_BSP_SendImuSample(uint64_t timestamp_us, const int16_t accel_raw[3]);

#endif /* __UART_BSP_H__ */
