/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "spi.h"
#include "gpio.h"
#include <stdio.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "BMI088driver.h"
#include "lcd.h"
#include "uart_bsp.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define IMU_RING_SIZE        512U
#define IMU_RING_MASK        (IMU_RING_SIZE - 1U)
#define IMU_LOW_PASS_HZ      2.0f
#define LCD_REFRESH_MS       200U
#define UART_OUTPUT_DIVIDER  8U
#define TIMER_CLOCK_HZ       240000000U
#define TIM2_TICKS_PER_US    (TIMER_CLOCK_HZ / 1000000U)

typedef struct
{
  uint32_t timestamp_us;
  int16_t accel_raw[3];
} IMU_Sample;

static volatile IMU_Sample imu_ring[IMU_RING_SIZE];
static volatile uint16_t imu_ring_head;
static volatile uint16_t imu_ring_tail;
static volatile uint32_t imu_dropped_samples;
static volatile uint8_t imu_capture_enabled;

static float low_pass_accel[3];
static uint8_t low_pass_initialized;
static uint64_t previous_sample_time_us;
static float measured_sample_rate_hz = 1600.0f;

static void IMU_TimestampTimer_Init(void)
{
  __HAL_RCC_TIM2_CLK_ENABLE();
  TIM2->CR1 = 0;
  TIM2->PSC = TIM2_TICKS_PER_US - 1U;
  TIM2->ARR = 0xFFFFFFFFU;
  TIM2->EGR = TIM_EGR_UG;
  TIM2->CNT = 0;
  TIM2->SR = 0;
  TIM2->CR1 = TIM_CR1_CEN;
}

static uint64_t IMU_ExtendTimestamp(uint32_t raw_timestamp)
{
  static uint8_t initialized;
  static uint32_t previous_raw_timestamp;
  static uint64_t extended_timestamp_us;

  if (!initialized)
  {
    initialized = 1;
    extended_timestamp_us = raw_timestamp;
  }
  else
  {
    extended_timestamp_us += (uint32_t)(raw_timestamp - previous_raw_timestamp);
  }

  previous_raw_timestamp = raw_timestamp;
  return extended_timestamp_us;
}

static void IMU_ConvertAcceleration(const IMU_Sample *sample, float accel[3])
{
  uint8_t axis;

  for (axis = 0; axis < 3; axis++)
  {
    accel[axis] = sample->accel_raw[axis] * BMI088_ACCEL_6G_SEN;
  }
}

static void IMU_UpdateLowPass(const float accel[3], uint64_t timestamp_us)
{
  float dt_s;
  float rc;
  float alpha;
  uint8_t axis;

  if (!low_pass_initialized)
  {
    for (axis = 0; axis < 3; axis++)
    {
      low_pass_accel[axis] = accel[axis];
    }
    low_pass_initialized = 1;
    previous_sample_time_us = timestamp_us;
    return;
  }

  dt_s = (float)(timestamp_us - previous_sample_time_us) * 0.000001f;
  previous_sample_time_us = timestamp_us;
  if (dt_s <= 0.0f)
  {
    return;
  }

  if ((dt_s >= 0.0001f) && (dt_s <= 0.005f))
  {
    float measured = 1.0f / dt_s;
    measured_sample_rate_hz += 0.02f * (measured - measured_sample_rate_hz);
  }

  rc = 1.0f / (2.0f * 3.14159265f * IMU_LOW_PASS_HZ);
  alpha = dt_s / (rc + dt_s);
  for (axis = 0; axis < 3; axis++)
  {
    low_pass_accel[axis] += alpha * (accel[axis] - low_pass_accel[axis]);
  }
}

static void LCD_ShowAcceleration(uint16_t y, float value, uint16_t color)
{
  char text[16];

  snprintf(text, sizeof(text), "%+7.2f", value);
  LCD_ShowString(88, y, (const uint8_t *)text, color, BLACK, 24, 0);
}

static void LCD_ShowIMUError(uint8_t error)
{
  char text[24];

  snprintf(text, sizeof(text), "IMU ERROR: 0x%02X", error);
  LCD_ShowString(24, 196, (const uint8_t *)text, RED, BLACK, 24, 0);
}

static void LCD_ShowLowPassData(void)
{
  char text[32];

  LCD_ShowAcceleration(60, low_pass_accel[0], RED);
  LCD_ShowAcceleration(104, low_pass_accel[1], GREEN);
  LCD_ShowAcceleration(148, low_pass_accel[2], BLUE);

  snprintf(text, sizeof(text), "FS:%4.0fHz DROP:%lu",
           measured_sample_rate_hz, (unsigned long)imu_dropped_samples);
  LCD_ShowString(8, 214, (const uint8_t *)text, WHITE, BLACK, 16, 0);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  int16_t accel_raw[3];
  uint16_t next_head;

  if ((GPIO_Pin != ACC_INT_Pin) || !imu_capture_enabled)
  {
    return;
  }

  next_head = (imu_ring_head + 1U) & IMU_RING_MASK;
  if (next_head == imu_ring_tail)
  {
    imu_dropped_samples++;
    return;
  }

  BMI088_read_acceleration(accel_raw);
  imu_ring[imu_ring_head].timestamp_us = TIM2->CNT;
  imu_ring[imu_ring_head].accel_raw[0] = accel_raw[0];
  imu_ring[imu_ring_head].accel_raw[1] = accel_raw[1];
  imu_ring[imu_ring_head].accel_raw[2] = accel_raw[2];
  __DMB();
  imu_ring_head = next_head;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */
    UART_BSP_Init();
    LCD_Init();
    LCD_Fill(0, 0, LCD_W, LCD_H, BLACK);
    LCD_ShowString(32, 12, (const uint8_t *)"BMI088 LPF m/s^2", CYAN, BLACK, 24, 0);
    LCD_DrawLine(12, 44, LCD_W - 12, 44, GRAY);
    LCD_ShowString(36, 60, (const uint8_t *)"X:", RED, BLACK, 24, 0);
    LCD_ShowString(36, 104, (const uint8_t *)"Y:", GREEN, BLACK, 24, 0);
    LCD_ShowString(36, 148, (const uint8_t *)"Z:", BLUE, BLACK, 24, 0);

    uint8_t imu_error;
    while((imu_error = BMI088_init()) != BMI088_NO_ERROR)
    {
        LCD_ShowIMUError(imu_error);
        HAL_Delay(500);
    }
    LCD_Fill(0, 194, LCD_W, LCD_H, BLACK);

    IMU_TimestampTimer_Init();
    imu_capture_enabled = 1;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t last_display_tick = HAL_GetTick() - LCD_REFRESH_MS;
  uint8_t uart_output_divider = 0;
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
        uint8_t processed_sample = 0;
        while (imu_ring_tail != imu_ring_head)
        {
            IMU_Sample sample;
            uint16_t tail = imu_ring_tail;
            uint64_t timestamp_us;
            float accel_mps2[3];

            sample.timestamp_us = imu_ring[tail].timestamp_us;
            sample.accel_raw[0] = imu_ring[tail].accel_raw[0];
            sample.accel_raw[1] = imu_ring[tail].accel_raw[1];
            sample.accel_raw[2] = imu_ring[tail].accel_raw[2];
            __DMB();
            imu_ring_tail = (tail + 1U) & IMU_RING_MASK;

            timestamp_us = IMU_ExtendTimestamp(sample.timestamp_us);
            IMU_ConvertAcceleration(&sample, accel_mps2);
            IMU_UpdateLowPass(accel_mps2, timestamp_us);
            if (++uart_output_divider >= UART_OUTPUT_DIVIDER)
            {
                uart_output_divider = 0;
                UART_BSP_SendAcceleration(accel_mps2);
            }
            processed_sample = 1;
        }

        if ((HAL_GetTick() - last_display_tick) >= LCD_REFRESH_MS)
        {
            last_display_tick = HAL_GetTick();
            LCD_ShowLowPassData();
        }

        if (!processed_sample)
        {
            __disable_irq();
            if (imu_ring_tail == imu_ring_head)
            {
                __WFI();
            }
            __enable_irq();
        }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 2;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = 1;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
