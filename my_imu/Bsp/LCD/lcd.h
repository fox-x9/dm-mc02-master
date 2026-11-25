#ifndef MY_IMU_LCD_H
#define MY_IMU_LCD_H

#include "main.h"
#include "spi.h"

#define USE_ANALOG_SPI  0
#define USE_HORIZONTAL  2

#if (USE_HORIZONTAL == 0) || (USE_HORIZONTAL == 1)
#define LCD_W  240U
#define LCD_H  280U
#else
#define LCD_W  280U
#define LCD_H  240U
#endif

#define LCD_RES_Clr()  HAL_GPIO_WritePin(LCD_RES_GPIO_Port, LCD_RES_Pin, GPIO_PIN_RESET)
#define LCD_RES_Set()  HAL_GPIO_WritePin(LCD_RES_GPIO_Port, LCD_RES_Pin, GPIO_PIN_SET)
#define LCD_DC_Clr()   HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET)
#define LCD_DC_Set()   HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET)
#define LCD_CS_Clr()   HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET)
#define LCD_CS_Set()   HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET)
#define LCD_BLK_Clr()  HAL_GPIO_WritePin(LCD_BLK_GPIO_Port, LCD_BLK_Pin, GPIO_PIN_RESET)
#define LCD_BLK_Set()  HAL_GPIO_WritePin(LCD_BLK_GPIO_Port, LCD_BLK_Pin, GPIO_PIN_SET)

void LCD_Init(void);
void LCD_Writ_Bus(uint8_t dat);
void LCD_WR_DATA8(uint8_t dat);
void LCD_WR_DATA(uint16_t dat);
void LCD_WR_REG(uint8_t dat);
void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

void LCD_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color);
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void Draw_Circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);

void LCD_ShowChinese(uint16_t x, uint16_t y, uint8_t *s, uint16_t fc, uint16_t bc,
                     uint8_t sizey, uint8_t mode);
void LCD_ShowChinese12x12(uint16_t x, uint16_t y, uint8_t *s, uint16_t fc, uint16_t bc,
                          uint8_t sizey, uint8_t mode);
void LCD_ShowChinese16x16(uint16_t x, uint16_t y, uint8_t *s, uint16_t fc, uint16_t bc,
                          uint8_t sizey, uint8_t mode);
void LCD_ShowChinese24x24(uint16_t x, uint16_t y, uint8_t *s, uint16_t fc, uint16_t bc,
                          uint8_t sizey, uint8_t mode);
void LCD_ShowChinese32x32(uint16_t x, uint16_t y, uint8_t *s, uint16_t fc, uint16_t bc,
                          uint8_t sizey, uint8_t mode);

void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc,
                  uint8_t sizey, uint8_t mode);
void LCD_ShowString(uint16_t x, uint16_t y, const uint8_t *p, uint16_t fc, uint16_t bc,
                    uint8_t sizey, uint8_t mode);
uint32_t mypow(uint8_t m, uint8_t n);
void LCD_ShowIntNum(uint16_t x, uint16_t y, uint16_t num, uint8_t len,
                    uint16_t fc, uint16_t bc, uint8_t sizey);
void LCD_ShowFloatNum(uint16_t x, uint16_t y, float num, uint8_t len, uint8_t decimal,
                      uint16_t fc, uint16_t bc, uint8_t sizey);
void LCD_ShowFloatNum1(uint16_t x, uint16_t y, float num, uint8_t len, uint8_t decimal,
                       uint16_t fc, uint16_t bc, uint8_t sizey);
void LCD_ShowPicture(uint16_t x, uint16_t y, uint16_t length, uint16_t width,
                     const uint8_t pic[]);

#define WHITE       0xFFFFU
#define BLACK       0x0000U
#define BLUE        0x001FU
#define BRED        0xF81FU
#define GRED        0xFFE0U
#define GBLUE       0x07FFU
#define RED         0xF800U
#define MAGENTA     0xF81FU
#define GREEN       0x07E0U
#define CYAN        0x7FFFU
#define YELLOW      0xFFE0U
#define BROWN       0xBC40U
#define BRRED       0xFC07U
#define GRAY        0x8430U
#define DARKBLUE    0x01CFU
#define LIGHTBLUE   0x7D7CU
#define GRAYBLUE    0x5458U
#define LIGHTGREEN  0x841FU
#define LGRAY       0xC618U
#define LGRAYBLUE   0xA651U
#define LBBLUE      0x2B12U

#endif
