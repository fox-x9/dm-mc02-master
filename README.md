# dm-mc02-master

STM32H723VGTx 控制板 Keil MDK 例程集合。

## 例程

| 工程 | 功能 |
| --- | --- |
| `CtrBoard-H7_ADC` | ADC 采样 |
| `CtrBoard-H7_ALL` | 综合例程 |
| `CtrBoard-H7_BUZZER` | 蜂鸣器 |
| `CtrBoard-H7_CAN` | CAN |
| `CtrBoard-H7_DBUS` | DBUS 遥控接收 |
| `CtrBoard-H7_FDCAN` | FDCAN |
| `CtrBoard-H7_IMU` | BMI088 基础读写 |
| `CtrBoard-H7_IMU_TempCtrl` | BMI088 温控 |
| `CtrBoard-H7_KEY` | 按键与 ADC 按键 |
| `CtrBoard-H7_LCD` | SPI LCD |
| `CtrBoard-H7_MPU` | MPU 传感器 |
| `CtrBoard-H7_OV5640` | OV5640 摄像头 |
| `CtrBoard-H7_RS485` | RS485 |
| `CtrBoard-H7_TIM` | 定时器 |
| `CtrBoard-H7_UART` | UART |
| `CtrBoard-H7_USB` | USB |
| `CtrBoard-H7_W25Q64` | W25Q64 Flash |
| `CtrBoard-H7_WS2812` | WS2812 |
| `my_imu` | BMI088 + ST7789 动平衡仪原型 |

## my_imu

`my_imu` 使用 STM32H723 + BMI088 + ST7789：

详细接线、串口协议、参数和构建说明见
[my_imu/README.md](my_imu/README.md)。

- BMI088 加速度计 `±6 g`、`1600 Hz` DRDY 采样。
- 屏幕显示 `2 Hz` 低通后的三轴加速度。
- USART1 使用 PA9(TX)/PA10(RX)，`921600 8N1`。
- 串口数据格式：`t_us,ax_raw,ay_raw,az_raw`。
- `m/s² = raw * 0.00179443359375`。

## 构建

使用 Keil MDK 打开各例程 `MDK-ARM` 目录下的 `.uvprojx` 文件。生成目录中的编译中间文件不会提交到仓库，可烧录的 `.hex` 文件会保留。
