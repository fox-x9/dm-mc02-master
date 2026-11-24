#include "uart_bsp.h"
#include "usart.h"
#include <stdio.h>

void UART_BSP_Init(void)
{
    MX_USART1_UART_Init();
}

void UART_BSP_SendAcceleration(const float accel_mps2[3])
{
    char text[48];
    int length;

    length = snprintf(text, sizeof(text), "accel:%.6f,%.6f,%.6f\n",
                      accel_mps2[0], accel_mps2[1], accel_mps2[2]);
    if (length > 0)
    {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)length, 3);
    }
}
