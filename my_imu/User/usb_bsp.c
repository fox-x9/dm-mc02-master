#include "usb_bsp.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"

#define USB_TX_BUFFER_SIZE     32U

extern USBD_HandleTypeDef hUsbDeviceHS;

static uint8_t tx_buffer[USB_TX_BUFFER_SIZE];

static uint16_t USB_BSP_AppendUnsigned(char *text, uint16_t index, uint32_t value)
{
    char digits[10];
    uint8_t count = 0;

    do
    {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (value != 0U);

    while (count > 0U)
    {
        text[index++] = digits[--count];
    }
    return index;
}

static uint16_t USB_BSP_AppendFixed3(char *text, uint16_t index, float value)
{
    int32_t scaled;
    uint32_t magnitude;
    uint32_t whole;
    uint32_t fraction;

    scaled = (int32_t)((value >= 0.0f) ? (value * 1000.0f + 0.5f)
                                       : (value * 1000.0f - 0.5f));
    if (scaled < 0)
    {
        text[index++] = '-';
        magnitude = (uint32_t)(-scaled);
    }
    else
    {
        magnitude = (uint32_t)scaled;
    }

    whole = magnitude / 1000U;
    fraction = magnitude % 1000U;
    index = USB_BSP_AppendUnsigned(text, index, whole);
    text[index++] = '.';
    text[index++] = (char)('0' + (fraction / 100U));
    text[index++] = (char)('0' + ((fraction / 10U) % 10U));
    text[index++] = (char)('0' + (fraction % 10U));
    return index;
}

static uint8_t USB_BSP_IsReady(void)
{
    return ((hUsbDeviceHS.dev_state == USBD_STATE_CONFIGURED) &&
            (hUsbDeviceHS.pClassData != NULL));
}

void USB_BSP_Init(void)
{
    MX_USB_DEVICE_Init();
}

void USB_BSP_SendAcceleration(const float accel_mps2[3])
{
    uint16_t index;

    if (!USB_BSP_IsReady() || !CDC_TransmitReady_HS())
    {
        return;
    }

    index = 0;
    index = USB_BSP_AppendFixed3((char *)tx_buffer, index, accel_mps2[0]);
    tx_buffer[index++] = ',';
    index = USB_BSP_AppendFixed3((char *)tx_buffer, index, accel_mps2[1]);
    tx_buffer[index++] = ',';
    index = USB_BSP_AppendFixed3((char *)tx_buffer, index, accel_mps2[2]);
    tx_buffer[index++] = '\n';
    (void)CDC_Transmit_HS(tx_buffer, index);
}
