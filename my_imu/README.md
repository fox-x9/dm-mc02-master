# my_imu

STM32H723VGTx + BMI088 三轴加速度采集工程。

- BMI088 加速度计采样率：`1600 Hz`
- USB-C CDC 输出：X/Y/Z 三个 `float`，VOFA+ `JustFloat`
- LCD：显示 X/Y/Z 三轴振动 RMS、总振动 RMS 和强度条
- LCD 刷新率：`10 Hz`
- WS2812：绿色 `1 Hz` 心跳表示系统运行
- 不采集、不输出陀螺仪数据

## USB 输出

USB-C 会枚举为 CDC 虚拟串口。每帧为 16 字节：

```text
float ax + float ay + float az + 00 00 80 7F
```

- 三轴单位均为 `m/s²`
- 浮点数为 32 位 little-endian
- VOFA+ 数据引擎选择 `JustFloat`
- 通道数选择 `3`

CDC 波特率字段不限制实际 USB 速率，VOFA+ 中可保留 `115200`。

## 数据流

```text
TIM2 每 625 us 触发一次
        |
        v
中断读取 BMI088 X/Y/Z 原始数据
        |
        v
256 点环形缓冲区
        |
        +--> USB-C CDC -> VOFA+ JustFloat
        |
        +--> 10 Hz LCD 数值和强度条
```

采样在 TIM2 中断中完成。即使 LCD 正在刷新，传感器采样也不会停止；主循环从
环形缓冲区读取并发送数据。

## LCD 显示

- 逻辑分辨率：`280 x 240`，横屏
- X、Y、Z 三行分别显示振动 RMS 数值和红、绿、青三色强度条
- 每 `160` 个采样点（100 ms）结算一次 RMS，显示窗口不随主循环负载变化
- 底部显示三轴合成的 `TOTAL` RMS
- USB 每 `3` 个采样帧合并为一次 CDC 发送，降低全速 USB 的包调度压力
- 强度条量程：`0~60 m/s²`
- 没有 USB 连接时仍可使用 LCD 单独观察三轴强度

LCD 使用 SPI1，与 BMI088 的 SPI2 相互独立。

## LCD 启动顺序

当前硬件已验证的稳定顺序为：

1. `MX_GPIO_Init()`，PC15 输出高电平，`LCD_CS` 初始低电平。
2. `MX_SPI1_Init()`。
3. `AccelDisplay_Init()` 完成 LCD 初始化和首屏绘制。
4. 初始化 SPI6、SPI2、BMI088 和 USB。
5. 最后启动 TIM2 的 IMU 采样中断。

不要在 LCD 初始化前启动 IMU 定时器或 USB，也不要移动
`AccelDisplay_Init()` 到其他外设初始化之后，否则 LCD 可能偶发黑屏。

`my_imu/Bsp/LCD/lcd.c` 与官方 `CtrBoard-H7_LCD/User/lcd.c` 保持一致。
ST7789 硬件复位后不能省略 `SLPOUT (0x11)`；删除该命令会导致只有背光、
没有图像。首次启动时可能出现一次短暂闪屏，属于正常现象。

## 当前版本备注（2026-11-25）

### 供电

- 当前测试条件为 ST-Link 提供 3.3 V 调试电源、USB-C 提供 5 V。
- 仅接 USB-C 时，`VCC_5VS` 由 `USB-VBUS` 经 `D14` 提供。
- PC15 对应 `Power_5V_EN`，控制外部 `VCC_IN` 的 5 V 转换支路，
  不是 USB-C VBUS 的直接开关。
- 如果以后只使用外部 `VCC_IN` 供电，需要重新确认 PC15 使能配置。

### 测量与显示

- BMI088 加速度 ODR 配置为芯片上限 `1600 Hz`。
- RMS 使用固定 `160 个样本` 窗口，即 100 ms，不随 USB 或 LCD 负载变化。
- 每个窗口会减去均值，因此重力、静态安装倾角和零偏不会直接计入振动强度。
- 三轴强度条量程为 `0~60 m/s²`；总振动使用黄色第四根条。
- 总振动定义：`sqrt(RMS_X² + RMS_Y² + RMS_Z²)`。
- 数值显示分辨率为 `0.01 m/s²`，数值过大时显示上限为 `99.99`。

### USB 与 VOFA+

- 输出格式仍为 VOFA+ `JustFloat`，每帧三个 `float`。
- 逻辑上保留全部 `1600` 个样本/秒；每 3 帧合并为一次 48 字节 CDC 传输，
  降低全速 USB 的事务频率。
- CDC `SET_LINE_CODING` 会保存主机设置的串口参数，
  `GET_LINE_CODING` 返回同一组参数。
- 发送通道在收到 `SET_LINE_CODING` 或 DTR 有效后启用，兼容 VOFA+ 的打开方式。
- USB 数据属于尽力发送，不提供硬实时保证，也不在 LCD 上显示丢帧状态。

### 当前边界

- 暂未实现转速同步、振动相位和自动平衡角度计算。
- 暂未实现自动量程、传感器多点标定和频带滤波。
- 修改 LCD 初始化顺序、ST7789 命令或 USB 批处理参数后，应至少进行多次
  断电重启和 VOFA+ 连续采集验证。

## 状态灯

| 状态 | WS2812 |
| --- | --- |
| 系统初始化 | 蓝色 |
| 正常运行 | 绿色 1 Hz 闪烁 |
| BMI088 初始化失败 | 红色常亮 |

## 外设引脚

| 功能 | STM32H723 引脚 |
| --- | --- |
| BMI088 SPI2_SCK | PB13 |
| BMI088 SPI2_MISO | PC2_C |
| BMI088 SPI2_MOSI | PC1 |
| BMI088 ACC_CS | PC0 |
| BMI088 GYRO_CS | PC3_C |
| USB_DM / USB_DP | PA11 / PA12 |
| WS2812 | PA7 / SPI6_MOSI |
| LCD SPI1_SCK | PB3 |
| LCD SPI1_MOSI | PD7 |
| LCD_CS | PE15 |
| LCD_DC | PD10 |
| LCD_RES | PB11 |
| LCD_BLK | PB10 |
| Power_5V_EN（外部 VCC_IN 5V 转换使能） | PC15 |

仅接 USB-C 时，LCD 的 `VCC_5VS` 由 USB-C `VBUS` 经 `D14` 提供，
不由 PC15 直接控制。

## 目录结构

```text
my_imu/
├── App/
│   ├── accel_display.c/h    LCD 加速度界面
│   └── usb_bsp.c/h          USB JustFloat 输出
├── Bsp/
│   ├── BMI088/              IMU 驱动
│   ├── LCD/                 ST7789 LCD 驱动
│   └── WS2812/              状态灯驱动
├── Core/                    CubeMX 应用与 HAL 配置
├── USB_DEVICE/              USB CDC 应用与底层适配
├── Middlewares/             STM32 USB Device Library
├── Drivers/                 STM32H7 HAL/CMSIS
├── cmake/                   GCC 启动文件、链接脚本和工具链
├── MDK-ARM/                 Keil 工程
├── CMakeLists.txt
├── CMakePresets.json
└── README.md
```

## 编译与烧录

### GCC + CMake + Ninja（推荐）

使用 VS Code STM32Cube 扩展已有的 GNU Arm GCC、CMake 和 Ninja：

```powershell
$cmake = "$env:LOCALAPPDATA\stm32cube\bundles\cmake\4.4.3+st.1\bin\cmake.exe"
& $cmake --preset gcc-debug
& $cmake --build --preset gcc-debug
```

输出文件：

```text
build/gcc-debug/my_imu.hex
build/gcc-debug/my_imu.bin
```

### Keil MDK

Keil 工程：

```text
MDK-ARM/my_imu.uvprojx
```

完整编译后输出：

```text
MDK-ARM/my_imu/my_imu.hex
```

烧录 `.hex` 必须指定 Intel HEX 格式：

```powershell
& $stflash --freq 100 --connect-under-reset --format=ihex --reset write $hex
```

详细工具链与 ST-Link 操作见 [编译烧录流程.md](../编译烧录流程.md)。
