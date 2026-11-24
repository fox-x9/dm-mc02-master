#include "uart_bsp.h"
#include "usart.h"
#include <stdio.h>

#define UART_BSP_OUTPUT_PERIOD_US  (1000000U / UART_BSP_OUTPUT_RATE_HZ)

static uint64_t next_output_time_us;

void UART_BSP_Init(void)
{
    next_output_time_us = 0;
    MX_USART1_UART_Init();
}

void UART_BSP_SendAcceleration(uint64_t timestamp_us, const float accel_mps2[3])
{
    char text[48];
    int length;

    if (timestamp_us < next_output_time_us)
    {
        return;
    }

    do
    {
        next_output_time_us += UART_BSP_OUTPUT_PERIOD_US;
    }
    while (next_output_time_us <= timestamp_us);

    length = snprintf(text, sizeof(text), "%.3f,%.3f,%.3f\n",
                      accel_mps2[0], accel_mps2[1], accel_mps2[2]);
    if (length > 0)
    {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)length, 20);
    }
}
