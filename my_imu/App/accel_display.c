#include "accel_display.h"

#include <math.h>

#include "lcd.h"

/*
 * LCD 布局：
 * - 左侧显示轴名称和 RMS 数值；
 * - 右侧统一从 x=112 到 x=272 绘制强度条；
 * - X/Y/Z 三行和总振动行的 Y 起点间隔均为 48 像素。
 */
#define DISPLAY_BAR_X             112U
#define DISPLAY_BAR_RIGHT         272U
#define DISPLAY_BAR_TOP_OFFSET    4U
#define DISPLAY_BAR_HEIGHT        18U
#define DISPLAY_BAR_SCALE_MPS2    60.0f
#define DISPLAY_TRACK_COLOR       0x2104U
#define DISPLAY_TOTAL_ROW_Y       192U
/* 1600 Hz / 160 samples gives a deterministic 100 ms RMS window. */
#define DISPLAY_WINDOW_SAMPLES    160U
#define DISPLAY_MAX_VALUE_MPS2    99.99f

/*
 * 原始加速度累计量。sum 用于计算窗口均值，sum_sq 用于计算均方值。
 * 方差 = E[x^2] - E[x]^2，可同时去除重力、安装倾角和传感器零偏。
 */
static float vibration_sum[3];
static float vibration_sum_sq[3];
static uint16_t vibration_sample_count;

/*
 * 统计结果只在完整窗口结束时更新，因此 LCD 刷新与 USB 负载不会改变测量
 * 窗口。Update() 只读取 latest_*，不直接读取正在累计的数据。
 */
static float latest_rms_mps2[3];
/* Total vibration magnitude: |RMS| = sqrt(X^2 + Y^2 + Z^2). */
static float latest_total_rms_mps2;
static uint8_t latest_rms_valid;
static uint16_t previous_value_hundredths[3];
static uint16_t previous_total_hundredths;

/**
 * @brief 用单色快速填充 LCD 矩形。
 *
 * 先把颜色预先扩展为 512 像素的缓冲区，再按块调用 HAL_SPI_Transmit。
 * 这样比逐像素调用 HAL 更高效，同时避免申请动态内存。
 */
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

/**
 * @brief 将浮点值量化为百分之一单位。
 *
 * 用于判断显示数字是否真的发生变化。若数值未跨过 0.01，Update() 可以跳过
 * 数字区域的清屏和重绘，降低 LCD 总线占用。
 */
static uint16_t Display_ToHundredths(float value)
{
    /* Quantize first so unchanged display values do not redraw over SPI. */
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

/**
 * @brief 在指定位置显示一个累计 m/s^2 数值。
 *
 * 显示函数只接受非负值，因此 NaN、负数和过大值都会先被钳位。
 */
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

/**
 * @brief 绘制固定标题、四条轨道、数值占位符并清零统计状态。
 */
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

    /* 整屏先清黑，避免 ST7789 上电后随机帧缓存内容短暂显现。 */
    Display_FillRect(0U, 0U, LCD_W, LCD_H, BLACK);

    LCD_ShowString(8U, 8U, (const uint8_t *)"BMI088 ACCEL", WHITE, BLACK, 16U, 0U);
    LCD_ShowString(216U, 8U, (const uint8_t *)"1600 Hz", GBLUE, BLACK, 16U, 0U);
    LCD_DrawLine(8U, 32U, LCD_W - 8U, 32U, GRAY);

    /* X/Y/Z 三轴使用不同颜色，便于快速区分振动来源。 */
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

    /* 第四行显示三轴合成的总振动强度，使用黄色与分轴条区分。 */
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

    /* 初始化缓存状态。第一条完整 RMS 窗口结束前，Update() 不会改写数值。 */
    latest_rms_valid = 0U;
    previous_value_hundredths[0] = 0U;
    previous_value_hundredths[1] = 0U;
    previous_value_hundredths[2] = 0U;
    previous_total_hundredths = 0U;
}

/**
 * @brief 累计一个 1600 Hz IMU 样本，并在满 160 点时结算 RMS。
 */
void AccelDisplay_ProcessSample(const float accel_mps2[3])
{
    uint8_t axis;

    for (axis = 0U; axis < 3U; axis++)
    {
        float sample = accel_mps2[axis];

        /* 非有限值或超过显示上限的数据按 0 处理，防止污染整个统计窗口。 */
        if (sample != sample || sample > DISPLAY_MAX_VALUE_MPS2 ||
            sample < -DISPLAY_MAX_VALUE_MPS2)
        {
            sample = 0.0f;
        }
        vibration_sum[axis] += sample;
        vibration_sum_sq[axis] += sample * sample;
    }

    /*
     * 使用样本计数而不是 HAL_GetTick() 划分窗口，确保每个 RMS 结果都来自
     * 严格的 160 个样本。
     */
    vibration_sample_count++;
    if (vibration_sample_count >= DISPLAY_WINDOW_SAMPLES)
    {
        float inverse_count = 1.0f / (float)vibration_sample_count;
        float total_squared = 0.0f;

        for (axis = 0U; axis < 3U; axis++)
        {
            float mean = vibration_sum[axis] * inverse_count;
            /*
             * 标准差代表去除直流分量后的振动有效值：
             * 静止时约为 0；只有动态振动会增大该值。
             */
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

        /*
         * 三轴矢量和能够给出不依赖安装方向的整体振动强度：
         * |A| = sqrt(RMS_X^2 + RMS_Y^2 + RMS_Z^2)。
         */
        latest_total_rms_mps2 = sqrtf(total_squared);
        latest_rms_valid = 1U;
        vibration_sample_count = 0U;
    }
}

/**
 * @brief 刷新强度条和数字显示。
 */
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
        /* 第一条完整统计窗口尚未完成，保留初始化占位画面。 */
        return;
    }

    /* 先处理 X/Y/Z 三条分轴数据。 */
    for (axis = 0U; axis < 3U; axis++)
    {
        float magnitude = latest_rms_mps2[axis];
        uint16_t magnitude_hundredths = Display_ToHundredths(magnitude);
        uint16_t bar_y = row_y[axis] + DISPLAY_BAR_TOP_OFFSET;
        uint16_t fill_width;

        /* 强度条最大量程为 60 m/s^2，超过量程时保持满格。 */
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

        /* 只有显示值跨过 0.01 m/s^2 才重绘数字，节省 SPI 带宽。 */
        if (magnitude_hundredths != previous_value_hundredths[axis])
        {
            Display_ShowValue(32U, row_y[axis] + 2U,
                              latest_rms_mps2[axis], axis_colors[axis]);
            previous_value_hundredths[axis] = magnitude_hundredths;
        }
    }

    /* 总振动条和分轴条共用相同量程及填充算法。 */
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

    /* 总振动数值同样按百分位变化触发重绘。 */
    if (Display_ToHundredths(latest_total_rms_mps2) != previous_total_hundredths)
    {
        previous_total_hundredths = Display_ToHundredths(latest_total_rms_mps2);
        Display_ShowValue(32U, DISPLAY_TOTAL_ROW_Y + 2U,
                          latest_total_rms_mps2, YELLOW);
    }

}
