// Copyright (c) 2017, Intel Corporation.

/**
* @file
* @brief WebUSB specific configuration file
*
* Provides WebUSB and Microsoft OS Descriptor 2.0 Platform Capability
* descriptors. Also, it handles the WebUSB Device Requests and provides
* the landing page and allowed origin descriptors.
*/

#include <stdio.h>
#include <string.h>
#include <device.h>
#include <zephyr.h>

#include "ide-comms.h"
#include "ide-webusb.h"

/* WebUSB Platform Capability Descriptor */
static const u8_t webusb_bos_descriptor[] = {
  /* Binary Object Store descriptor */
  0x05, 0x0F, 0x1D, 0x00, 0x01,

  /* WebUSB Platform Capability Descriptor:
   * https://wicg.github.io/webusb/#webusb-platform-capability-descriptor
   */
  0x18, 0x10, 0x05, 0x00,

  /* PlatformCapability UUID */
  0x38, 0xB6, 0x08, 0x34, 0xA9, 0x09, 0xA0, 0x47,
  0x8B, 0xFD, 0xA0, 0x76, 0x88, 0x15, 0xB6, 0x65,

  /* Version, VendorCode, Landing Page */
  0x00, 0x01, 0x01, 0x01,
};

/* URL Descriptor: https://wicg.github.io/webusb/#url-descriptor */
static const u8_t webusb_origin_url[] = {
  /* Length, DescriptorType, Scheme */
  0x11, 0x03, 0x00,
  'l', 'o', 'c', 'a', 'l', 'h', 'o', 's', 't', ':', '8', '0', '0', '0'
};

/**
 * @brief Custom handler for standard requests in
 *        order to catch the request and return the
 *        WebUSB Platform Capability Descriptor.
 *
 * @param pSetup    Information about the request to execute.
 * @param len       Size of the buffer.
 * @param data      Buffer containing the request result.
 *
 * @return  0 on success, negative errno code on fail
 */
int custom_handle_req(struct usb_setup_packet *pSetup,
    s32_t *len, u8_t **data)
{
  if (GET_DESC_TYPE(pSetup->wValue) == DESCRIPTOR_TYPE_BOS) {
    *data = (u8_t *)(&webusb_bos_descriptor);
    *len = sizeof(webusb_bos_descriptor);

    return 0;
  }

  return -ENOTSUP;
}

/**
 * @brief Handler called for WebUSB vendor specific commands.
 *
 * @param pSetup    Information about the request to execute.
 * @param len       Size of the buffer.
 * @param data      Buffer containing the request result.
 *
 * @return  0 on success, negative errno code on fail.
 */
int vendor_handle_req(struct usb_setup_packet *pSetup,
    s32_t *len, u8_t **data)
{
  /* Get URL request */
  if (pSetup->bRequest == 0x01 && pSetup->wIndex == 0x02) {
    u8_t index = GET_DESC_INDEX(pSetup->wValue);

    if (index != 1)
      return -ENOTSUP;

    *data = (u8_t *)(&webusb_origin_url);
    *len = sizeof(webusb_origin_url);
    return 0;
  }

  return -ENOTSUP;
}

/*
 *  Receive buffer management for the WebUSB driver.
 */
#define WEBUSB_RX_POOL_SIZE    4

typedef struct {
    u32_t _for_kernel_use;
    u8_t data[WEBUSB_RX_BUFFER_SIZE];
    size_t length;
    atomic_t lock;
} webusb_rx_buf_t;

// The WebUSB driver will use these buffers.
webusb_rx_buf_t webusb_rx_buffers[WEBUSB_RX_POOL_SIZE];
static u32_t webusb_rx_index = 0;

// Release an rx buffer (from the ide_process).
static void webusb_release_buffer(webusb_rx_buf_t *buf)
{
    atomic_set(&(buf->lock), 0);
    buf->length = 0;
    memset(buf->data, 0, WEBUSB_RX_BUFFER_SIZE);
}

// Provide a buffer to the WebUSB driver. Return a pointer to the data part.
u8_t *get_rx_buf()
{
    for (int i = 0; i < WEBUSB_RX_POOL_SIZE; i++) {
        webusb_rx_buf_t *buf = &(webusb_rx_buffers[webusb_rx_index++]);
        webusb_rx_index %= WEBUSB_RX_POOL_SIZE;
        if (atomic_get(&(buf->lock)) == 0) {
          atomic_set(&(buf->lock), 1);
          return buf->data;
        }
    }
    return NULL;
}

// Obtain the webusb_rx_buf_t buffer pointer from the data pointer.
static inline webusb_rx_buf_t * buffer_from_data(u8_t *data)
{
    return (webusb_rx_buf_t *)(data - sizeof(u32_t));
}

// Use this queue to store buffers available from the WebUSB driver.
// The ide_process will consume them from here.
static struct k_fifo rx_queue;

// Consume a buffer (part of a message) in WebUSB driver context.
static void webusb_receive(u8_t *data, size_t len)
{
    webusb_rx_buf_t *buf = buffer_from_data(data);
    buf->length = len;
    k_fifo_put(&rx_queue, buf);
}

// Initialize Custom and Vendor request handlers, and buffer handling callbacks.
static struct webusb_req_handlers req_handlers = {
  .custom_handler = custom_handle_req,
  .vendor_handler = vendor_handle_req,
  .rx_handler = webusb_receive,
  .get_buffer = get_rx_buf
};

// Store the receive callback provided by the application here.
static webusb_process_cb webusb_cb = NULL;

void webusb_init(webusb_process_cb cb)
{
  k_fifo_init(&rx_queue);
  webusb_cb = cb;
  webusb_register_request_handlers(&req_handlers);
}

void webusb_receive_process()
{
  webusb_rx_buf_t *buf;
  while ((buf = (webusb_rx_buf_t *) k_fifo_get(&rx_queue, K_FOREVER))) {
    if (webusb_cb) {
        webusb_cb(buf->data, buf->length);
        webusb_release_buffer(buf);
    }
    k_yield();
  }
}
