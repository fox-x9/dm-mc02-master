#include "usb_bsp.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include <stdio.h>

#define USB_TX_BUFFER_SIZE     256U
#define USB_TX_FLUSH_THRESHOLD 192U
#define USB_LINE_MAX_SIZE      32U

extern USBD_HandleTypeDef hUsbDeviceHS;

static uint8_t tx_buffers[2][USB_TX_BUFFER_SIZE];
static uint8_t tx_write_index;
static uint16_t tx_length;

static uint8_t USB_BSP_IsReady(void)
{
    return ((hUsbDeviceHS.dev_state == USBD_STATE_CONFIGURED) &&
            (hUsbDeviceHS.pClassData != NULL));
}

static void USB_BSP_Flush(void)
{
    if (tx_length == 0U)
    {
        return;
    }

    if (!USB_BSP_IsReady())
    {
        tx_length = 0;
        tx_write_index = 0;
        return;
    }

    if (CDC_Transmit_HS(tx_buffers[tx_write_index], tx_length) == USBD_OK)
    {
        tx_write_index ^= 1U;
        tx_length = 0;
    }
}

void USB_BSP_Init(void)
{
    tx_write_index = 0;
    tx_length = 0;
    MX_USB_DEVICE_Init();
}

void USB_BSP_SendAcceleration(const float accel_mps2[3])
{
    int length;

    if (!USB_BSP_IsReady())
    {
        tx_length = 0;
        return;
    }

    if ((USB_TX_BUFFER_SIZE - tx_length) < USB_LINE_MAX_SIZE)
    {
        USB_BSP_Flush();
        if ((USB_TX_BUFFER_SIZE - tx_length) < USB_LINE_MAX_SIZE)
        {
            return;
        }
    }

    length = snprintf((char *)&tx_buffers[tx_write_index][tx_length],
                      USB_TX_BUFFER_SIZE - tx_length,
                      "%.3f,%.3f,%.3f\n",
                      accel_mps2[0], accel_mps2[1], accel_mps2[2]);
    if (length > 0)
    {
        tx_length += (uint16_t)length;
        if (tx_length >= USB_TX_FLUSH_THRESHOLD)
        {
            USB_BSP_Flush();
        }
    }
}
