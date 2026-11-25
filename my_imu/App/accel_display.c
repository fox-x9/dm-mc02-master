#include "accel_display.h"

#include "lcd.h"

#define DISPLAY_BAR_X             112U
#define DISPLAY_BAR_RIGHT         272U
#define DISPLAY_BAR_TOP_OFFSET    4U
#define DISPLAY_BAR_HEIGHT        18U
#define DISPLAY_BAR_SCALE_MPS2    60.0f
#define DISPLAY_TRACK_COLOR       0x2104U

static void Display_FillRect(uint16_t xsta, uint16_t ysta,
                             uint16_t xend, uint16_t yend, uint16_t color)
{
    static uint8_t buffer[1024];
    uint32_t pixels;
    uint32_t index;

    if ((xend <= xsta) || (yend <= ysta))
    {
        return;
    }

    for (index = 0U; index < 512U; index++)
    {
        buffer[index * 2U] = (uint8_t)(color >> 8);
        buffer[index * 2U + 1U] = (uint8_t)color;
    }

    pixels = (uint32_t)(xend - xsta) * (uint32_t)(yend - ysta);
    LCD_Address_Set(xsta, ysta, xend - 1U, yend - 1U);
    LCD_CS_Clr();
    LCD_DC_Set();
    while (pixels > 0U)
    {
        uint32_t chunk = (pixels > 512U) ? 512U : pixels;
        (void)HAL_SPI_Transmit(&hspi1, buffer, (uint16_t)(chunk * 2U), 0xFFFFU);
        pixels -= chunk;
    }
    LCD_CS_Set();
}

void AccelDisplay_Init(void)
{
    static const uint16_t axis_colors[3] = {RED, GREEN, GBLUE};
    static const uint8_t *axis_labels[3] = {
        (const uint8_t *)"X",
        (const uint8_t *)"Y",
        (const uint8_t *)"Z"
    };
    static const uint16_t row_y[3] = {50U, 114U, 178U};
    uint8_t axis;

    LCD_Init();
    Display_FillRect(0U, 0U, LCD_W, LCD_H, BLACK);

    LCD_ShowString(8U, 8U, (const uint8_t *)"BMI088 ACCEL", WHITE, BLACK, 16U, 0U);
    LCD_ShowString(216U, 8U, (const uint8_t *)"1600 Hz", GBLUE, BLACK, 16U, 0U);
    LCD_DrawLine(8U, 32U, LCD_W - 8U, 32U, GRAY);

    for (axis = 0U; axis < 3U; axis++)
    {
        uint16_t bar_y = row_y[axis] + DISPLAY_BAR_TOP_OFFSET;

        LCD_ShowString(8U, row_y[axis], axis_labels[axis],
                       axis_colors[axis], BLACK, 24U, 0U);
        Display_FillRect(DISPLAY_BAR_X, bar_y, DISPLAY_BAR_RIGHT,
                         bar_y + DISPLAY_BAR_HEIGHT, GRAY);
        Display_FillRect(DISPLAY_BAR_X + 1U, bar_y + 1U,
                         DISPLAY_BAR_RIGHT - 1U,
                         bar_y + DISPLAY_BAR_HEIGHT - 1U,
                         DISPLAY_TRACK_COLOR);
    }

    LCD_ShowString(8U, 216U, (const uint8_t *)"ABS ACCEL  M/S2",
                   LGRAY, BLACK, 16U, 0U);
    LCD_ShowString(180U, 216U, (const uint8_t *)"SCALE 0-60",
                   LGRAY, BLACK, 16U, 0U);
}

void AccelDisplay_Update(const float accel_mps2[3])
{
    static const uint16_t axis_colors[3] = {RED, GREEN, GBLUE};
    static const uint16_t row_y[3] = {50U, 114U, 178U};
    static uint16_t previous_width[3];
    const uint16_t bar_inner_width = DISPLAY_BAR_RIGHT - DISPLAY_BAR_X - 2U;
    uint8_t axis;

    for (axis = 0U; axis < 3U; axis++)
    {
        float magnitude = accel_mps2[axis];
        uint16_t bar_y = row_y[axis] + DISPLAY_BAR_TOP_OFFSET;
        uint16_t fill_width;

        if (magnitude != magnitude)
        {
            magnitude = 0.0f;
        }
        if (magnitude < 0.0f)
        {
            magnitude = -magnitude;
        }
        if (magnitude > DISPLAY_BAR_SCALE_MPS2)
        {
            magnitude = DISPLAY_BAR_SCALE_MPS2;
        }

        fill_width = (uint16_t)((magnitude * (float)bar_inner_width) /
                                DISPLAY_BAR_SCALE_MPS2);
        if (fill_width > previous_width[axis])
        {
            Display_FillRect(DISPLAY_BAR_X + 1U + previous_width[axis], bar_y + 1U,
                             DISPLAY_BAR_X + 1U + fill_width,
                             bar_y + DISPLAY_BAR_HEIGHT - 1U,
                             axis_colors[axis]);
        }
        else if (fill_width < previous_width[axis])
        {
            Display_FillRect(DISPLAY_BAR_X + 1U + fill_width, bar_y + 1U,
                             DISPLAY_BAR_X + 1U + previous_width[axis],
                             bar_y + DISPLAY_BAR_HEIGHT - 1U,
                             DISPLAY_TRACK_COLOR);
        }
        previous_width[axis] = fill_width;
    }
}
