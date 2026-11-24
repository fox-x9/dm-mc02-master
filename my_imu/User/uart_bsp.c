#include "uart_bsp.h"
#include "usart.h"
#include <stdio.h>

void UART_BSP_Init(void)
{
    static const uint8_t header[] = "t_us,ax_raw,ay_raw,az_raw\r\n";

    MX_USART1_UART_Init();
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)header, sizeof(header) - 1U, 5);
}

void UART_BSP_SendImuSample(uint64_t timestamp_us, const int16_t accel_raw[3])
{
    char text[48];
    int length;

    length = snprintf(text, sizeof(text), "%llu,%d,%d,%d\r\n",
                      (unsigned long long)timestamp_us,
                      accel_raw[0], accel_raw[1], accel_raw[2]);
    if (length > 0)
    {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)length, 3);
    }
}
