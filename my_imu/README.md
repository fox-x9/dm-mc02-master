# my_imu

STM32H723VGTx + BMI088 三轴加速度采集工程。当前调试模式：

- BMI088 加速度计配置为 `800 Hz`
- USB 输出率固定为 `500 Hz`
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
TIM2 2 ms period
        |
        v
读取 BMI088 最新 ax/ay/az 原始值
        |
        v
main loop -> USB-C CDC -> JustFloat(ax, ay, az)
```

TIM2 以 1 MHz 计数，主循环每 2 ms 读取一次最新加速度并通过 USB 发送。

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
| USB_DM | PA11 |
| USB_DP | PA12 |
| WS2812 | PA7 / SPI6_MOSI |

当前只初始化加速度计所需的 SPI2 和中断，不初始化 LCD 使用的 SPI1、LCD
控制和背光引脚。

## 主要文件

- `Core/Src/main.c`：TIM2 定时采样、USB 发送和 LED 心跳
- `Device/BMI088/BMI088driver.c`：BMI088 初始化、`±6 g` 量程和 `800 Hz` 采样率
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

调试阶段主循环保持运行，不进入 `WFI`。这样 H7 的 USB 枚举、CDC 中断和
ST-Link 调试访问不会被 Sleep 状态干扰，正常烧录不需要反复拔插 USB-C。
后续如增加低功耗模式，应单独验证 USB、调试休眠和唤醒时序。

详细编译和 ST-Link 操作见
[编译烧录流程.md](../编译烧录流程.md)。
