# my_imu

STM32H723VGTx + BMI088 三轴加速度采集工程。当前调试模式：

- BMI088 加速度输出率固定为 `200 Hz`
- 通过 USB-C CDC 虚拟串口发送 X/Y/Z 三轴加速度
- 板载 WS2812 使用绿色 `1 Hz` 心跳指示系统运行
- LCD 不初始化、不刷新
- 不输出陀螺仪数据

## USB 输出

USB-C 会枚举为 CDC 虚拟串口，数据采用与 ALL-VOFA 示例相同的
VOFA+ `JustFloat` 二进制格式。每帧包含：

```text
float ax + float ay + float az + 00 00 80 7F
```

- 共 16 字节，依次为 X、Y、Z 轴加速度和帧尾
- 单位：`m/s²`
- 浮点数为 32 位 little-endian
- 不包含表头、时间戳或陀螺仪数据

USB CDC 的波特率字段不限制实际速率。使用 Type-C 数据线连接电脑即可，
VOFA+ 中选择 `JustFloat` 和对应 COM 口。

## 数据流

```text
BMI088 ACC_DRDY (PE10)
        |
        v
EXTI15_10_IRQHandler
        |
        v
读取 ax/ay/az 原始值
        |
        v
64 点环形缓冲区
        |
        v
main loop -> USB-C CDC -> JustFloat(ax, ay, az)
```

DRDY 中断只负责读取传感器和写入环形缓冲区；USB 发送在主循环中完成。

## 状态灯

| 状态 | WS2812 |
| --- | --- |
| 初始化 | 蓝色常亮 |
| 正常运行 | 绿色 1 Hz 闪烁 |
| IMU 初始化失败 | 红色常亮 |

WS2812 由 `SPI6_MOSI (PA7)` 驱动，5V 可控电源由 `PC15` 使能。

## 硬件连接

| 功能 | STM32H723 引脚 |
| --- | --- |
| BMI088 SPI2_SCK | PB13 |
| BMI088 SPI2_MISO | PC2_C |
| BMI088 SPI2_MOSI | PC1 |
| BMI088 ACC_CS | PC0 |
| BMI088 ACC_DRDY | PE10 |
| USB_DM | PA11 |
| USB_DP | PA12 |
| WS2812 | PA7 / SPI6_MOSI |

当前只初始化加速度计所需的 SPI2 和中断，不初始化 LCD 使用的 SPI1、LCD
控制和背光引脚。

## 主要文件

- `Core/Src/main.c`：采样中断、64 点环形缓冲、USB 发送和 LED 心跳
- `Device/BMI088/BMI088driver.c`：BMI088 初始化、`±6 g` 量程和 `200 Hz` 输出率
- `User/usb_bsp.c`：JustFloat 组帧和 CDC 发送
- `Devices/WS2812/ws2812.c`：SPI6 驱动 WS2812

没有为本次功能新增源文件或目录。

## 编译与烧录

Keil 工程：

```text
MDK-ARM/my_imu.uvprojx
```

完整编译后生成：

```text
MDK-ARM/my_imu/my_imu.hex
```

烧录 `.hex` 时必须指定 Intel HEX 格式：

```powershell
& $stflash --freq 100 --connect-under-reset --format=ihex --reset write $hex
```

工程在进入 `WFI` 前调用 `HAL_DBGMCU_EnableDBGSleepMode()`，因此 H7 空闲休眠时
仍允许 ST-Link 访问调试接口并执行软件复位，正常烧录不需要反复拔插 USB-C。
如果 ST-Link 已连接 NRST，可以进一步提高连接失败时的恢复能力。

详细编译和 ST-Link 操作见
[编译烧录流程.md](../编译烧录流程.md)。
