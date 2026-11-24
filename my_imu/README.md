# my_imu

基于 STM32H723VGTx、BMI088 六轴 IMU 和 ST7789 SPI 屏的桨叶动平衡仪原型。

工程当前版本使用 `1600 Hz` DRDY 中断采集加速度计数据，通过 USART1
以 `250 Hz` 输出 VOFA+ FireWater 格式的三轴加速度；LCD 只显示
`2 Hz` 低通后的三轴低频加速度，避免屏幕刷新影响高频采集。

## 功能

- BMI088 加速度计量程：`±6 g`
- 加速度计输出率：`1600 Hz`
- 加速度计 DRDY 上升沿触发采样
- `TIM2` 提供 `1 µs` 分辨率时间戳
- `512` 个样本的环形缓冲区
- USART1 以 `250 Hz` 输出 FireWater 兼容的三轴加速度 CSV 数据
- LCD 显示 `2 Hz` 低通后的 X、Y、Z 加速度
- LCD 同时显示实测采样率和环形缓冲区丢样数

## 数据流

```text
BMI088 ACC_DRDY (PE10)
        |
        v
EXTI15_10 interrupt
        |
        +--> 读取加速度 X/Y/Z 原始值
        +--> 记录 TIM2 微秒计数
        +--> 写入 512 深度环形缓冲区
                         |
                         v
                    main loop
                         |
             +-----------+-----------+
             |                       |
             v                       v
      USART1 高频输出          2 Hz 低通滤波
      ax,ay,az\n                     |
                                     v
                              ST7789 LCD
```

显示与采样相互解耦。LCD 刷新期间，DRDY 中断仍可继续把采样结果写入
环形缓冲区，因此屏幕不会直接决定 IMU 的采样周期。

## 硬件连接

### BMI088

| 功能 | STM32H723 引脚 | 说明 |
| --- | --- | --- |
| SPI2_SCK | PB13 | BMI088 SPI 时钟 |
| SPI2_MISO | PC2_C | BMI088 SPI 数据输入 |
| SPI2_MOSI | PC1 | BMI088 SPI 数据输出 |
| ACC_CS | PC0 | 加速度计片选 |
| GYRO_CS | PC3_C | 陀螺仪片选 |
| ACC_DRDY | PE10 | 加速度数据就绪中断 |
| GYRO_INT | PE12 | 当前运行模式下未使用 |

BMI088 的加速度计和陀螺仪共用 SPI2，但使用独立片选。当前采集流程只读取
加速度计，陀螺仪 DRDY 输出已关闭。

### ST7789 LCD

| 功能 | STM32H723 引脚 |
| --- | --- |
| SPI1_SCK | PB3 |
| SPI1_MOSI | PD7 |
| LCD_CS | PE15 |
| LCD_DC | PD10 |
| LCD_RES | PB11 |
| LCD_BLK | PB10 |

LCD 驱动使用横屏模式，分辨率为 `280 × 240`。SPI1 工作在约
`30 Mbit/s`，SPI2 工作在约 `7.5 Mbit/s`。

### USART1

| 功能 | STM32H723 引脚 | 外部连接 |
| --- | --- | --- |
| USART1_TX | PA9 | USB-TTL 的 RXD |
| USART1_RX | PA10 | USB-TTL 的 TXD，可悬空 |
| GND | GND | 必须与 USB-TTL 共地 |

串口参数：

```text
115200 baud
8 data bits
1 stop bit
no parity
no flow control
```

## VOFA+ FireWater 串口协议

USART1 使用
[FireWater](https://www.vofa.plus/plugin_detail/?name=firewater) CSV
字符串协议。每个采样点输出一行：

```text
0.123,-0.235,9.807\n
```

`\n` 表示实际换行符，不是反斜杠和字符 `n`。FireWater 只有在收到换行后
才会解析当前数据帧。

| 通道 | 单位 | 说明 |
| --- | --- | --- |
| 0 | `m/s²` | X 轴加速度 |
| 1 | `m/s²` | Y 轴加速度 |
| 2 | `m/s²` | Z 轴加速度 |

- 数据帧不使用前缀，三个数值直接对应 X、Y、Z 三个通道。
- 每个通道保留三位小数。
- 固件内部将 BMI088 的 `int16_t` 原始计数乘以
  `0.00179443359375`，对应 `±6 g` 量程。
- 串口不再输出原始计数和时间戳，也没有启动表头。
- 横坐标由 VOFA+ 按连续样本序号生成。

在 VOFA+ 中依次设置：

1. 数据源选择对应串口和 `115200` 波特率。
2. 数据引擎选择 `FireWater`。
3. 打开串口后，在图形控件中会看到三路加速度通道。

IMU 仍以 `1600 Hz` 采样，FireWater 按 `4 ms` 时间片限制输出，因此串口
输出率为 `250 Hz`。每行约 20–25 字节时，串口数据率约为 5–6.3 KB/s，低于
115200 波特率 8N1 模式下的理论值 11.52 KB/s。

降低串口输出率是为了保留 1600 Hz 的原始采样和 2 Hz 屏幕显示，同时避免
低波特率下的阻塞发送导致环形缓冲区持续丢样。

## LCD 显示

屏幕内容包括：

```text
BMI088 LPF m/s^2

X:     +0.12
Y:     -0.08
Z:     +9.81

FS:1600Hz DROP:0
```

- X、Y、Z 为 `2 Hz` 一阶低通后的加速度。
- `FS` 为根据时间戳估算的采样率。
- `DROP` 表示环形缓冲区已满、新样本被丢弃的累计数量。
- LCD 每 `200 ms` 刷新一次。
- BMI088 初始化失败时，屏幕显示 `IMU ERROR: 0xXX`。

该屏幕不用于显示高频振动波形。高频数据应通过串口采集，并在 PC 端进行
绘图、频谱分析或阶次分析。

## 工程结构

```text
my_imu/
├── Core/
│   ├── Inc/
│   │   ├── main.h
│   │   ├── spi.h
│   │   ├── usart.h
│   │   └── ...
│   └── Src/
│       ├── main.c
│       ├── spi.c
│       ├── usart.c
│       └── ...
├── Device/
│   └── BMI088/
│       ├── BMI088driver.c
│       ├── BMI088Middleware.c
│       └── inc/
├── User/
│   ├── lcd.c
│   ├── lcd.h
│   ├── lcdfont.h
│   ├── uart_bsp.c
│   └── uart_bsp.h
├── MDK-ARM/
│   ├── my_imu.uvprojx
│   └── my_imu/my_imu.hex
├── my_imu.ioc
└── README.md
```

### 关键文件

- [main.c](./Core/Src/main.c)：DRDY 采样、环形缓冲区、时间戳和 LCD 低通显示
- [BMI088driver.c](./Device/BMI088/BMI088driver.c)：BMI088 初始化、量程/输出率配置和原始数据读取
- [usart.c](./Core/Src/usart.c)：USART1 外设配置
- [uart_bsp.c](./User/uart_bsp.c)：FireWater 三通道数据帧发送
- [lcd.c](./User/lcd.c)：ST7789 LCD 驱动
- [my_imu.ioc](./my_imu.ioc)：STM32CubeMX 引脚和外设配置

## 编译与烧录

开发环境：

- Keil MDK 5
- Arm Compiler 5
- STM32H7xx DFP
- STM32CubeProgrammer 或 ST-Link Utility

打开 [my_imu.uvprojx](./MDK-ARM/my_imu.uvprojx)，选择目标 `my_imu`，
执行 Rebuild 后生成：

```text
MDK-ARM/my_imu/my_imu.hex
```

当前源码已验证编译结果为 `0 Error(s), 0 Warning(s)`。

使用 ST-Link 烧录时，目标芯片应识别为：

```text
STM32H72x/STM32H73x
Cortex-M7
1 MB Flash
```

## 可调参数

| 参数 | 位置 | 当前值 |
| --- | --- | --- |
| 加速度量程 | `Device/BMI088/BMI088driver.c` | `±6 g` |
| 加速度输出率 | `Device/BMI088/BMI088driver.c` | `1600 Hz` |
| 环形缓冲区深度 | `Core/Src/main.c` | `512` 样本 |
| 显示低通截止频率 | `Core/Src/main.c` | `2 Hz` |
| LCD 刷新周期 | `Core/Src/main.c` | `200 ms` |
| FireWater 输出率 | `User/uart_bsp.c` | `250 Hz` |
| USART1 波特率 | `Core/Src/usart.c` | `115200` |

如果 `±6 g` 仍然饱和，可继续改为 `±12 g` 或 `±24 g`。修改时必须同步更新：

1. `BMI088_ACC_RANGE_*` 寄存器配置。
2. `BMI088_ACCEL_SEN` 换算系数。
3. LCD 低通计算中使用的灵敏度常量。

## 动平衡使用说明

本工程提供的是振动数据采集前端，尚未计算不平衡量大小和相位。

VOFA+ 可以直接显示三轴时域曲线；导出数据后，PC 端还可以执行：

1. 按样本顺序和实测采样率恢复时间轴。
2. 对三轴数据去除重力/直流分量。
3. 做 FFT 或包络分析，寻找转子基频及其倍频。
4. 若转速变化，使用转速同步采样或阶次分析。

`1600 Hz` 采样的理论 Nyquist 频率为 `800 Hz`。实际做动平衡时建议每个
转子周期至少保留 8–16 个采样点。例如 `12000 RPM = 200 Hz` 时，
1600 Hz 对应每个周期约 8 个点。对于更高转速，应提高采样率或改用更高速
的采集方案。

## 当前限制

- 仅进行传感器读取、滤波和显示，没有零偏、轴间误差或温漂标定。
- 串口发送换算后的三轴加速度，不包含时间戳或陀螺仪数据。
- 未输出陀螺仪数据。
- 未实现 FFT、不平衡幅值、相位或配重计算。
- LCD 只显示低频信息，不能用于观察完整的高频振动波形。
