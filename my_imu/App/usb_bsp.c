#include "usb_bsp.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"

#define USB_FRAME_DATA_BYTES  (3U * sizeof(float))
#define USB_FRAME_TAIL_BYTES  4U
#define USB_FRAME_SIZE        (USB_FRAME_DATA_BYTES + USB_FRAME_TAIL_BYTES)
#define USB_BATCH_SAMPLES     3U
#define USB_TX_BUFFER_SIZE    (USB_FRAME_SIZE * USB_BATCH_SAMPLES)

typedef union
{
    float value;
    uint8_t bytes[sizeof(float)];
} USB_BSP_FloatBytes;

extern USBD_HandleTypeDef hUsbDeviceHS;

static uint8_t tx_buffer[USB_TX_BUFFER_SIZE];
static uint8_t pending_sample_count;

static uint8_t USB_BSP_IsReady(void)
{
    return ((hUsbDeviceHS.dev_state == USBD_STATE_CONFIGURED) &&
            (hUsbDeviceHS.pClassData != NULL) &&
            CDC_DataChannelOpen_HS());
}

void USB_BSP_Init(void)
{
    MX_USB_DEVICE_Init();
}

void USB_BSP_SendAcceleration(const float accel_mps2[3])
{
    USB_BSP_FloatBytes sample;
    uint8_t *frame;
    uint8_t axis;

    if (!USB_BSP_IsReady())
    {
        pending_sample_count = 0U;
        return;
    }

    frame = &tx_buffer[pending_sample_count * USB_FRAME_SIZE];

    for (axis = 0U; axis < 3U; axis++)
    {
        sample.value = accel_mps2[axis];
        frame[axis * sizeof(float) + 0U] = sample.bytes[0];
        frame[axis * sizeof(float) + 1U] = sample.bytes[1];
        frame[axis * sizeof(float) + 2U] = sample.bytes[2];
        frame[axis * sizeof(float) + 3U] = sample.bytes[3];
    }

    /* VOFA+ JustFloat frame tail: 0x7F800000, little-endian on Cortex-M. */
    frame[USB_FRAME_DATA_BYTES + 0U] = 0x00U;
    frame[USB_FRAME_DATA_BYTES + 1U] = 0x00U;
    frame[USB_FRAME_DATA_BYTES + 2U] = 0x80U;
    frame[USB_FRAME_DATA_BYTES + 3U] = 0x7FU;

    pending_sample_count++;
    if (pending_sample_count < USB_BATCH_SAMPLES)
    {
        return;
    }

    pending_sample_count = 0U;
    if (!CDC_TransmitReady_HS() ||
        (CDC_Transmit_HS(tx_buffer, USB_TX_BUFFER_SIZE) != USBD_OK))
    {
        return;
    }
}
