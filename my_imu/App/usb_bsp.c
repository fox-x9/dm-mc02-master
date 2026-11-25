#include "usb_bsp.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"

#define USB_FRAME_DATA_BYTES  (3U * sizeof(float))
#define USB_FRAME_TAIL_BYTES  4U
#define USB_FRAME_SIZE        (USB_FRAME_DATA_BYTES + USB_FRAME_TAIL_BYTES)

/*
 * USB 全速 CDC 的主机轮询通常以 1 ms 帧为基础。
 * 若每 625 us 产生的样本都单独发起一次发送，TxState 很容易一直为忙。
 * 这里将 3 个 16 字节 JustFloat 帧合并为一次 48 字节传输：
 *   - 逻辑上仍保留 1600 个样本/秒；
 *   - 实际 CDC 请求约 533 次/秒；
 *   - 长度小于 64 字节，不会因满包额外触发零长度包。
 */
#define USB_BATCH_SAMPLES     3U
#define USB_TX_BUFFER_SIZE    (USB_FRAME_SIZE * USB_BATCH_SAMPLES)

/*
 * 用 union 把 float 的 IEEE-754 内存表示直接拆成字节。
 * VOFA+ JustFloat 与 Cortex-M7 都使用 little-endian。
 */
typedef union
{
    float value;
    uint8_t bytes[sizeof(float)];
} USB_BSP_FloatBytes;

extern USBD_HandleTypeDef hUsbDeviceHS;

/* USB 栈在传输完成前持有 tx_buffer，因此发送忙时不能覆盖该缓冲区。 */
static uint8_t tx_buffer[USB_TX_BUFFER_SIZE];
static uint8_t pending_sample_count;

/**
 * @brief 判断 CDC 是否已连接且主机已打开数据通道。
 */
static uint8_t USB_BSP_IsReady(void)
{
    return ((hUsbDeviceHS.dev_state == USBD_STATE_CONFIGURED) &&
            (hUsbDeviceHS.pClassData != NULL) &&
            CDC_DataChannelOpen_HS());
}

/**
 * @brief 初始化 USB 设备栈。
 */
void USB_BSP_Init(void)
{
    MX_USB_DEVICE_Init();
}

/**
 * @brief 累计 3 个样本后执行一次 CDC 批量发送。
 */
void USB_BSP_SendAcceleration(const float accel_mps2[3])
{
    USB_BSP_FloatBytes sample;
    uint8_t *frame;
    uint8_t axis;

    if (!USB_BSP_IsReady())
    {
        /* 主机未打开时清空半批数据，防止重新连接后混入旧样本。 */
        pending_sample_count = 0U;
        return;
    }

    /* 当前样本在批量缓冲区中的起始位置。 */
    frame = &tx_buffer[pending_sample_count * USB_FRAME_SIZE];

    /* 每帧依次写入 ax、ay、az。 */
    for (axis = 0U; axis < 3U; axis++)
    {
        sample.value = accel_mps2[axis];
        frame[axis * sizeof(float) + 0U] = sample.bytes[0];
        frame[axis * sizeof(float) + 1U] = sample.bytes[1];
        frame[axis * sizeof(float) + 2U] = sample.bytes[2];
        frame[axis * sizeof(float) + 3U] = sample.bytes[3];
    }

    /*
     * JustFloat 帧尾为浮点正无穷 0x7F800000。
     * Cortex-M little-endian 在线路上的字节序为 00 00 80 7F。
     */
    frame[USB_FRAME_DATA_BYTES + 0U] = 0x00U;
    frame[USB_FRAME_DATA_BYTES + 1U] = 0x00U;
    frame[USB_FRAME_DATA_BYTES + 2U] = 0x80U;
    frame[USB_FRAME_DATA_BYTES + 3U] = 0x7FU;

    pending_sample_count++;

    /* 不足 3 个样本时不发起 USB 事务。 */
    if (pending_sample_count < USB_BATCH_SAMPLES)
    {
        return;
    }

    pending_sample_count = 0U;

    /*
     * CDC 忙时丢弃整批数据，不轮询等待。
     * IMU 采样和 LCD 振动统计不依赖 USB 是否成功发送。
     */
    if (!CDC_TransmitReady_HS() ||
        (CDC_Transmit_HS(tx_buffer, USB_TX_BUFFER_SIZE) != USBD_OK))
    {
        return;
    }
}
