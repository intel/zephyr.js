/***************************************************************************
 *
 * Copyright(c) 2015,2016 Intel Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 * * Neither the name of Intel Corporation nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***************************************************************************/

/**
 * @file
 * @brief WebUSB enabled custom class driver header file
 *
 * Header file for WebUSB enabled custom class driver
 */

#ifndef __WEBUSB_DRIVER_H__
#define __WEBUSB_DRIVER_H__

#include <device.h>
#include <usb/usb_common.h>
#include <usb/usb_device.h>

/* Set USB version to 2.1 so that the host will request the BOS descriptor. */
#define USB_2_1     0x0210

/* Intel vendor ID */
#define WEBUSB_VENDOR_ID   0x8086

/* Product Id, random value */
#define WEBUSB_PRODUCT_ID  0xF8A1

/* BOS descriptor type */
#define DESCRIPTOR_TYPE_BOS     0x0f

/* Number of configurations for the USB Device */
#define WEBUSB_NUM_CONF    0x01

/* Number of interfaces */
#define WEBUSB_NUM_ITF         0x03

/* Number of Endpoints in the custom interface */
#define WEBUSB_NUM_EP          0x02
#define WEBUSB_ENDP_OUT        0x02
#define WEBUSB_ENDP_IN         0x83

/* Max WebUSB class request data size */
#define WEBUSB_CLASS_REQ_MAX_DATA_SIZE 8

/* Max packet size for Bulk endpoints */
#define WEBUSB_BULK_EP_MPS     64

#define WEBUSB_TX_BUFFER_SIZE  WEBUSB_BULK_EP_MPS
#define WEBUSB_RX_BUFFER_SIZE (2 * WEBUSB_BULK_EP_MPS)

/* Size in bytes of the configuration sent to
 * the Host on GetConfiguration() request
 * For Communication Device: CONF + (3 x ITF) +
 * (5 x EP) + HF + CMF + ACMF + UF -> 67 bytes
 */
#define WEBUSB_SERIAL_CONF_SIZE   (USB_CONFIGURATION_DESC_SIZE + \
	(1 * USB_INTERFACE_DESC_SIZE) + (2 * USB_ENDPOINT_DESC_SIZE))

/* WebUSB enabled Custom Class driver port name */
#define WEBUSB_SERIAL_PORT_NAME "WSERIAL"

/**
 * Callback function to send received data to the application.
 */
typedef void (*webusb_receive_handler) (u8_t *buf, size_t len);

/**
 * Callback function to request a buffer of size WEBUSB_RX_BUFFER_SIZE
 * from the application. Returns NULL if no buffer is available.
 */
typedef u8_t* (*webusb_buffer_provider) ();

/**
 * WebUSB request handlers, run in interrupt context, so keep them simple.
 */
struct webusb_req_handlers {
	/* Handler for WebUSB Vendor specific commands */
	usb_request_handler vendor_handler;
	/**
	 * The custom request handler gets a first chance at handling
	 * the request before it is handed over to the 'chapter 9' request
	 * handler
	 */
	usb_request_handler custom_handler;
	/**
	 * Handle received data. It should just save the data in a queue, and the
	 * application should cpmsume the data from that queue.
	 */
	webusb_receive_handler rx_handler;
	/**
	 * Handler for providing data buffer of size WEBUSB_RX_BUFFER_SIZE.
	 */
	webusb_buffer_provider get_buffer;
};

/**
 * @brief Register Custom and Vendor request callbacks
 *
 * Function to register Custom and Vendor request callbacks
 * for handling requests.
 *
 * @param [in] handlers Pointer to WebUSB request handlers structure
 *
 * @return N/A
 */
void webusb_register_request_handlers(struct webusb_req_handlers *handlers);

/**
 * @brief Callback used to send data to the Host
 *
 * @param data Buffer containing data to be sent to the Host.
 * @param len Buffer length.
 *
 * @return Number of bytes sent.
 */
int webusb_write(u8_t *data, size_t len);

#endif /* __WEBUSB_DRIVER_H__ */
