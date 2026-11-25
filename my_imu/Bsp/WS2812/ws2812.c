#include "ws2812.h"

#define WS2812_LowLevel    0xC0     // 0Ты
#define WS2812_HighLevel   0xF0     // 1Ты

void WS2812_Ctrl(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t txbuf[124] = {0};
    for (int i = 0; i < 8; i++)
    {
        txbuf[7-i]  = (((g>>i)&0x01) ? WS2812_HighLevel : WS2812_LowLevel)>>1;
        txbuf[15-i] = (((r>>i)&0x01) ? WS2812_HighLevel : WS2812_LowLevel)>>1;
        txbuf[23-i] = (((b>>i)&0x01) ? WS2812_HighLevel : WS2812_LowLevel)>>1;
    }
    (void)HAL_SPI_Transmit(&WS2812_SPI_UNIT, txbuf, sizeof(txbuf), 0xFFFFU);
}
