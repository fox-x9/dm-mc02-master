



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
#include "gpio.h"
#include "spi.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "BMI088driver.h"
#include "accel_display.h"
#include "usb_bsp.h"
#include "ws2812.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/*
 * TIM2 以 1600 Hz 触发加速度采样，单次周期为 625 us。
 * 该频率与 BMI088 当前配置的 ODR 一致，避免采样快于传感器更新。
 */
#define IMU_OUTPUT_RATE_HZ   1600U
#define IMU_PERIOD_US        (1000000U / IMU_OUTPUT_RATE_HZ)

/*
 * 环形缓冲区保存尚未被主循环消费的原始加速度计数。
 * 256 个槽位约等于 160 ms 的缓存，主循环短暂阻塞时可吸收抖动。
 * 通过 2 的幂掩码代替取模，降低 ISR 中的计算开销。
 */
#define IMU_RING_SIZE        256U
#define IMU_RING_MASK        (IMU_RING_SIZE - 1U)

/*
 * TIM2 内核时钟为 240 MHz，因此 1 个 tick 正好为 1 us。
 * 625 us 周期对应 PSC=239、ARR=624。
 */
#define TIM2_CLOCK_HZ        240000000U
#define TIM2_TICKS_PER_US    (TIM2_CLOCK_HZ / 1000000U)

/* WS2812 每 500 ms 翻转一次，LCD 保持 10 Hz 刷新。 */
#define LED_HEARTBEAT_MS     500U
#define LED_BRIGHTNESS       16U
#define LCD_REFRESH_MS       100U
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/*
 * 生产者是 TIM2 ISR，消费者是主循环。
 * ISR 只写 imu_ring_head，主循环只写 imu_ring_tail；行数据的可见性由 __DMB()
 * 保证。volatile 防止编译器把跨中断访问的变量缓存到寄存器。
 */
static volatile int16_t imu_accel_raw[IMU_RING_SIZE][3];
static volatile uint16_t imu_ring_head;
static volatile uint16_t imu_ring_tail;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void IMU_SampleTimer_Init(void)
{
  /*
   * 直接配置 TIM2 寄存器而不走 HAL_TIM 初始化，减少初始化层级。
   * 先停止计时器并清空计数值，再配置预分频、自动重装值和更新中断。
   */
  __HAL_RCC_TIM2_CLK_ENABLE();
  TIM2->CR1 = 0U;
  TIM2->PSC = TIM2_TICKS_PER_US - 1U;
  TIM2->ARR = IMU_PERIOD_US - 1U;
  TIM2->EGR = TIM_EGR_UG;
  TIM2->CNT = 0U;
  TIM2->SR = 0U;
  TIM2->DIER = TIM_DIER_UIE;
  HAL_NVIC_SetPriority(TIM2_IRQn, 1U, 0U);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);
  TIM2->CR1 = TIM_CR1_CEN;
}

void IMU_TimerSampleISR(void)
{
  int16_t accel_raw[3];
  uint16_t next_head = (imu_ring_head + 1U) & IMU_RING_MASK;

  /*
   * 保持一个空槽用于区分“空”和“满”：
   * next_head == tail 时缓冲区已满，本次样本直接丢弃，避免覆盖未消费数据。
   */
  if (next_head == imu_ring_tail)
  {
    return;
  }

  /*
   * 这里执行一次 6 字节 BMI088 SPI 读取。SPI2 的配置应保证该阻塞时间远小于
   * 625 us 周期；若以后提高 SPI 分频或增加传感器读取，必须重新评估 ISR 时长。
   */
  BMI088_read_acceleration(accel_raw);
  imu_accel_raw[imu_ring_head][0] = accel_raw[0];
  imu_accel_raw[imu_ring_head][1] = accel_raw[1];
  imu_accel_raw[imu_ring_head][2] = accel_raw[2];

  /* 确保三轴数据写入完成后，主循环才可能看到新的 head。 */
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
  /* USER CODE BEGIN 2 */
    /*
     * LCD 必须优先于 IMU、USB 和采样定时器启动。
     * 该顺序已在当前硬件上通过多次断电重启验证；提前启动 IMU 或 USB
     * 会提高 ST7789 偶发黑屏概率。
     */
    MX_SPI1_Init();
    AccelDisplay_Init();

    /* LCD 稳定后再初始化 WS2812 和 BMI088。 */
    MX_SPI6_Init();
    MX_SPI2_Init();

    if (bmi088_accel_init() != BMI088_NO_ERROR)
    {
        /* 初始化失败时用红灯常亮提示，并停止后续数据采集。 */
        WS2812_Ctrl(LED_BRIGHTNESS, 0U, 0U);
        Error_Handler();
    }

    /*
     * USB 仅作为数据观测通道；LCD 和 IMU 不依赖 USB 是否连接。
     * USB 初始化后再等待主机完成枚举。
     */
    USB_BSP_Init();
    HAL_Delay(300U);
    WS2812_Ctrl(0U, LED_BRIGHTNESS, 0U);

    /* 最后才启动高频采样中断，避免初始化 LCD 时被 IMU ISR 抢占。 */
    IMU_SampleTimer_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t led_tick = HAL_GetTick();
  uint32_t lcd_tick = HAL_GetTick();
  uint8_t led_on = 1U;
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
        /*
         * 主循环负责消费环形缓冲。原始计数先换算成 m/s^2，再分别送入
         * 振动统计和 USB 输出。即使 USB 忙，LCD 的振动统计仍会继续更新。
         */
        while (imu_ring_tail != imu_ring_head)
        {
            uint16_t tail = imu_ring_tail;
            int16_t accel_raw[3];
            float accel_mps2[3];

            accel_raw[0] = imu_accel_raw[tail][0];
            accel_raw[1] = imu_accel_raw[tail][1];
            accel_raw[2] = imu_accel_raw[tail][2];

            /* 与生产者相同的 DMA 屏障，保证先读数据再释放槽位。 */
            __DMB();
            imu_ring_tail = (tail + 1U) & IMU_RING_MASK;

            /* BMI088 6G 量程灵敏度换算，单位 m/s^2。 */
            accel_mps2[0] = accel_raw[0] * BMI088_ACCEL_6G_SEN;
            accel_mps2[1] = accel_raw[1] * BMI088_ACCEL_6G_SEN;
            accel_mps2[2] = accel_raw[2] * BMI088_ACCEL_6G_SEN;

            /* 振动统计在 160 个样本后锁存一次结果，不依赖 LCD 刷新时刻。 */
            AccelDisplay_ProcessSample(accel_mps2);

            /* USB 是尽力发送；主机未连接或发送端点忙时不会阻塞采样链路。 */
            USB_BSP_SendAcceleration(accel_mps2);
        }

        /* 1 Hz 绿色心跳用于确认主循环仍在运行。 */
        if ((HAL_GetTick() - led_tick) >= LED_HEARTBEAT_MS)
        {
            led_tick = HAL_GetTick();
            led_on ^= 1U;
            WS2812_Ctrl(0U, led_on ? LED_BRIGHTNESS : 0U, 0U);
        }

        /*
         * 控制台每 100 ms 刷新一次。测量窗口由样本数固定，LCD 刷新变慢
         * 只会影响显示延迟，不会改变 RMS 的 100 ms 时间口径。
         */
        if ((HAL_GetTick() - lcd_tick) >= LCD_REFRESH_MS)
        {
            lcd_tick = HAL_GetTick();
            AccelDisplay_Update();
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48 | RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 2;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = 1;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  /*
   * 当前调试版本在不可恢复错误时关闭中断并停在死循环，便于 SWD 捕获现场。
   * 量产版本如果需要自动恢复，应增加看门狗和按错误类型区分的重启策略。
   */
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
