#include "accel_display.h"

#include <math.h>

#include "lcd.h"

#define DISPLAY_BAR_X             112U
#define DISPLAY_BAR_RIGHT         272U
#define DISPLAY_BAR_TOP_OFFSET    4U
#define DISPLAY_BAR_HEIGHT        18U
#define DISPLAY_BAR_SCALE_MPS2    60.0f
#define DISPLAY_TRACK_COLOR       0x2104U
#define DISPLAY_TOTAL_ROW_Y       192U
#define DISPLAY_WINDOW_SAMPLES    160U
#define DISPLAY_MAX_VALUE_MPS2    99.99f

static float vibration_sum[3];
static float vibration_sum_sq[3];
static uint16_t vibration_sample_count;
static float latest_rms_mps2[3];
static float latest_total_rms_mps2;
static uint8_t latest_rms_valid;
static uint16_t previous_value_hundredths[3];
static uint16_t previous_total_hundredths;

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

static uint16_t Display_ToHundredths(float value)
{
    if (value != value || value <= 0.0f)
    {
        return 0U;
    }
    if (value > DISPLAY_MAX_VALUE_MPS2)
    {
        value = DISPLAY_MAX_VALUE_MPS2;
    }

    return (uint16_t)(value * 100.0f + 0.5f);
}

static void Display_ShowValue(uint16_t x, uint16_t y, float value, uint16_t color)
{
    if (value != value || value <= 0.0f)
    {
        value = 0.0f;
    }
    if (value > DISPLAY_MAX_VALUE_MPS2)
    {
        value = DISPLAY_MAX_VALUE_MPS2;
    }

    LCD_ShowFloatNum1(x, y, value, 3U, 2U, color, BLACK, 16U);
}

void AccelDisplay_Init(void)
{
    static const uint16_t axis_colors[3] = {RED, GREEN, GBLUE};
    static const uint8_t *axis_labels[3] = {
        (const uint8_t *)"X",
        (const uint8_t *)"Y",
        (const uint8_t *)"Z"
    };
    static const uint16_t row_y[3] = {48U, 96U, 144U};
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
        Display_ShowValue(32U, row_y[axis] + 2U, 0.0f, axis_colors[axis]);
        Display_FillRect(DISPLAY_BAR_X, bar_y, DISPLAY_BAR_RIGHT,
                         bar_y + DISPLAY_BAR_HEIGHT, GRAY);
        Display_FillRect(DISPLAY_BAR_X + 1U, bar_y + 1U,
                         DISPLAY_BAR_RIGHT - 1U,
                         bar_y + DISPLAY_BAR_HEIGHT - 1U,
                         DISPLAY_TRACK_COLOR);
    }

    LCD_ShowString(8U, DISPLAY_TOTAL_ROW_Y, (const uint8_t *)"T",
                   YELLOW, BLACK, 24U, 0U);
    Display_ShowValue(32U, DISPLAY_TOTAL_ROW_Y + 2U, 0.0f, YELLOW);
    Display_FillRect(DISPLAY_BAR_X,
                     DISPLAY_TOTAL_ROW_Y + DISPLAY_BAR_TOP_OFFSET,
                     DISPLAY_BAR_RIGHT,
                     DISPLAY_TOTAL_ROW_Y + DISPLAY_BAR_TOP_OFFSET +
                         DISPLAY_BAR_HEIGHT,
                     GRAY);
    Display_FillRect(DISPLAY_BAR_X + 1U,
                     DISPLAY_TOTAL_ROW_Y + DISPLAY_BAR_TOP_OFFSET + 1U,
                     DISPLAY_BAR_RIGHT - 1U,
                     DISPLAY_TOTAL_ROW_Y + DISPLAY_BAR_TOP_OFFSET +
                         DISPLAY_BAR_HEIGHT - 1U,
                     DISPLAY_TRACK_COLOR);

    latest_rms_valid = 0U;
    previous_value_hundredths[0] = 0U;
    previous_value_hundredths[1] = 0U;
    previous_value_hundredths[2] = 0U;
    previous_total_hundredths = 0U;
}

void AccelDisplay_ProcessSample(const float accel_mps2[3])
{
    uint8_t axis;

    for (axis = 0U; axis < 3U; axis++)
    {
        float sample = accel_mps2[axis];

        if (sample != sample || sample > DISPLAY_MAX_VALUE_MPS2 ||
            sample < -DISPLAY_MAX_VALUE_MPS2)
        {
            sample = 0.0f;
        }
        vibration_sum[axis] += sample;
        vibration_sum_sq[axis] += sample * sample;
    }

    vibration_sample_count++;
    if (vibration_sample_count >= DISPLAY_WINDOW_SAMPLES)
    {
        float inverse_count = 1.0f / (float)vibration_sample_count;
        float total_squared = 0.0f;

        for (axis = 0U; axis < 3U; axis++)
        {
            float mean = vibration_sum[axis] * inverse_count;
            float variance = vibration_sum_sq[axis] * inverse_count - mean * mean;

            if (variance != variance || variance < 0.0f)
            {
                variance = 0.0f;
            }

            latest_rms_mps2[axis] = sqrtf(variance);
            total_squared += latest_rms_mps2[axis] * latest_rms_mps2[axis];
            vibration_sum[axis] = 0.0f;
            vibration_sum_sq[axis] = 0.0f;
        }

        latest_total_rms_mps2 = sqrtf(total_squared);
        latest_rms_valid = 1U;
        vibration_sample_count = 0U;
    }
}

void AccelDisplay_Update(void)
{
    static const uint16_t axis_colors[3] = {RED, GREEN, GBLUE};
    static const uint16_t row_y[3] = {48U, 96U, 144U};
    static uint16_t previous_width[3];
    static uint16_t previous_total_width;
    const uint16_t bar_inner_width = DISPLAY_BAR_RIGHT - DISPLAY_BAR_X - 2U;
    const uint16_t total_bar_y =
        DISPLAY_TOTAL_ROW_Y + DISPLAY_BAR_TOP_OFFSET;
    uint8_t axis;

    if (latest_rms_valid == 0U)
    {
        return;
    }

    for (axis = 0U; axis < 3U; axis++)
    {
        float magnitude = latest_rms_mps2[axis];
        uint16_t magnitude_hundredths = Display_ToHundredths(magnitude);
        uint16_t bar_y = row_y[axis] + DISPLAY_BAR_TOP_OFFSET;
        uint16_t fill_width;

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

        if (magnitude_hundredths != previous_value_hundredths[axis])
        {
            Display_ShowValue(32U, row_y[axis] + 2U,
                              latest_rms_mps2[axis], axis_colors[axis]);
            previous_value_hundredths[axis] = magnitude_hundredths;
        }
    }

    {
        float total_bar_magnitude = latest_total_rms_mps2;
        uint16_t total_fill_width;

        if (total_bar_magnitude > DISPLAY_BAR_SCALE_MPS2)
        {
            total_bar_magnitude = DISPLAY_BAR_SCALE_MPS2;
        }
        total_fill_width =
            (uint16_t)((total_bar_magnitude * (float)bar_inner_width) /
                       DISPLAY_BAR_SCALE_MPS2);
        if (total_fill_width > previous_total_width)
        {
            Display_FillRect(DISPLAY_BAR_X + 1U + previous_total_width,
                             total_bar_y + 1U,
                             DISPLAY_BAR_X + 1U + total_fill_width,
                             total_bar_y + DISPLAY_BAR_HEIGHT - 1U,
                             YELLOW);
        }
        else if (total_fill_width < previous_total_width)
        {
            Display_FillRect(DISPLAY_BAR_X + 1U + total_fill_width,
                             total_bar_y + 1U,
                             DISPLAY_BAR_X + 1U + previous_total_width,
                             total_bar_y + DISPLAY_BAR_HEIGHT - 1U,
                             DISPLAY_TRACK_COLOR);
        }
        previous_total_width = total_fill_width;
    }

    if (Display_ToHundredths(latest_total_rms_mps2) != previous_total_hundredths)
    {
        previous_total_hundredths = Display_ToHundredths(latest_total_rms_mps2);
        Display_ShowValue(32U, DISPLAY_TOTAL_ROW_Y + 2U,
                          latest_total_rms_mps2, YELLOW);
    }

}
