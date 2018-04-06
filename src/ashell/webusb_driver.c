/*******************************************************************************
 *
 * Copyright(c) 2015 - 2018 Intel Corporation.
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
 ******************************************************************************/

/**
 * @file
 * @brief WebUSB enabled custom class driver
 *
 * This is a custom class driver to support the WebUSB.
 */
#include <zephyr.h>
#include <stdlib.h>
#include <init.h>
#include <uart.h>
#include <string.h>
#include <atomic.h>
#include <misc/byteorder.h>
#include <logging/sys_log.h>
#include "webusb_driver.h"

#define DEV_DATA(dev) \
	((struct webusb_serial_dev_data_t * const)(dev)->driver_data)

/* Misc. macros */
#define LOW_BYTE(x)  ((x) & 0xFF)
#define HIGH_BYTE(x) ((x) >> 8)

static struct device *webusb_serial_dev;
static struct webusb_req_handlers *req_handlers;

/* Device data structure */
struct webusb_serial_dev_data_t {
	/* USB device status code */
	enum usb_dc_status_code usb_status;
	/* Interface data buffer */
	u8_t interface_data[WEBUSB_CLASS_REQ_MAX_DATA_SIZE];
};

/* Structure representing the global USB description */
static const u8_t webusb_serial_usb_description[] = {
	/* Device descriptor */
	USB_DEVICE_DESC_SIZE,           /* Descriptor size */
	USB_DEVICE_DESC,                /* Descriptor type */
	LOW_BYTE(USB_2_1),
	HIGH_BYTE(USB_2_1),             /* USB version in BCD format */
	0x00,                           /* Class - Interface specific */
	0x00,                           /* SubClass - Interface specific */
	0x00,                           /* Protocol - Interface specific */
	MAX_PACKET_SIZE0,               /* Max Packet Size */
	LOW_BYTE(WEBUSB_VENDOR_ID),
	HIGH_BYTE(WEBUSB_VENDOR_ID),       /* Vendor Id */
	LOW_BYTE(WEBUSB_PRODUCT_ID),
	HIGH_BYTE(WEBUSB_PRODUCT_ID),      /* Product Id */
	LOW_BYTE(BCDDEVICE_RELNUM),
	HIGH_BYTE(BCDDEVICE_RELNUM),    /* Device Release Number */
	/* Index of Manufacturer String Descriptor */
	0x01,
	/* Index of Product String Descriptor */
	0x02,
	/* Index of Serial Number String Descriptor */
	0x03,
	WEBUSB_NUM_CONF,                   /* Number of Possible Configuration */

	/* Configuration descriptor */
	USB_CONFIGURATION_DESC_SIZE,    /* Descriptor size */
	USB_CONFIGURATION_DESC,         /* Descriptor type */
	/* Total length in bytes of data returned */
	LOW_BYTE(WEBUSB_SERIAL_CONF_SIZE),
	HIGH_BYTE(WEBUSB_SERIAL_CONF_SIZE),
	WEBUSB_NUM_ITF,                 /* Number of interfaces */
	0x01,                           /* Configuration value */
	0x00,                           /* Index of the Configuration string */
	USB_CONFIGURATION_ATTRIBUTES,   /* Attributes */
	MAX_LOW_POWER,                  /* Max power consumption */

	/* Interface descriptor */
	USB_INTERFACE_DESC_SIZE,        /* Descriptor size */
	USB_INTERFACE_DESC,             /* Descriptor type */
	0x00,                           /* Interface index */
	0x00,                           /* Alternate setting */
	WEBUSB_NUM_EP,                  /* Number of Endpoints */
	CUSTOM_CLASS,                   /* Custom Class */
	0x00,                           /* SubClass */
	0x00,                           /* Protocol */
	/* Index of the Interface String Descriptor */
	0x00,                           /* Interval */

	/* First Endpoint IN */
	USB_ENDPOINT_DESC_SIZE,         /* Descriptor size */
	USB_ENDPOINT_DESC,              /* Descriptor type */
	WEBUSB_ENDP_IN,                 /* Endpoint address */
	USB_DC_EP_BULK,                 /* Attributes */
	LOW_BYTE(WEBUSB_BULK_EP_MPS),
	HIGH_BYTE(WEBUSB_BULK_EP_MPS),  /* Max packet size */
	0x00,                           /* Interval */

	/* Second Endpoint OUT */
	USB_ENDPOINT_DESC_SIZE,         /* Descriptor size */
	USB_ENDPOINT_DESC,              /* Descriptor type */
	WEBUSB_ENDP_OUT,                /* Endpoint address */
	USB_DC_EP_BULK,                 /* Attributes */
	LOW_BYTE(WEBUSB_BULK_EP_MPS),
	HIGH_BYTE(WEBUSB_BULK_EP_MPS),  /* Max packet size */
	0x00,                           /* Interval */

	/* String descriptor language, only one, so min size 4 bytes.
	 * 0x0409 English(US) language code used
	 */
	USB_STRING_DESC_SIZE,           /* Descriptor size */
	USB_STRING_DESC,                /* Descriptor type */
	0x09,
	0x04,

	/* Manufacturer String Descriptor "Intel" */
	0x0C,
	USB_STRING_DESC,
	'I', 0, 'n', 0, 't', 0, 'e', 0, 'l', 0,

	/* Product String Descriptor "WebUSB" */
	0x0E,
	USB_STRING_DESC,
	'W', 0, 'e', 0, 'b', 0, 'U', 0, 'S', 0, 'B', 0,

	/* Serial Number String Descriptor "00.01" */
	0x0C,
	USB_STRING_DESC,
	'0', 0, '0', 0, '.', 0, '0', 0, '1', 0,
};

/**
 * @brief Handler called for Class requests not handled by the USB stack.
 *
 * @param pSetup    Information about the request to execute.
 * @param len       Size of the buffer.
 * @param data      Buffer containing the request result.
 *
 * @return  0 on success, negative errno code on fail.
 */
int webusb_serial_class_handle_req(struct usb_setup_packet *pSetup,
		s32_t *len, u8_t **data)
{
	return 0;
}

/**
 * @brief Custom handler for standard requests in order to
 *        catch the request and return the WebUSB Platform
 *        Capability Descriptor.
 *
 * @param pSetup    Information about the request to execute.
 * @param len       Size of the buffer.
 * @param data      Buffer containing the request result.
 *
 * @return  0 on success, negative errno code on fail.
 */
int webusb_serial_custom_handle_req(struct usb_setup_packet *pSetup,
		s32_t *len, u8_t **data)
{
	/* Call the callback */
	if ((req_handlers && req_handlers->custom_handler) &&
		(!req_handlers->custom_handler(pSetup, len, data)))
		return 0;

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
int webusb_serial_vendor_handle_req(struct usb_setup_packet *pSetup,
		s32_t *len, u8_t **data)
{
	/* Call the callback */
	if ((req_handlers && req_handlers->vendor_handler) &&
		(!req_handlers->vendor_handler(pSetup, len, data)))
		return 0;

	return -ENOTSUP;
}

/**
 * @brief Register Custom and Vendor request callbacks
 *
 * This function registers Custom and Vendor request callbacks
 * for handling the device requests.
 *
 * @param [in] handlers Pointer to WebUSB request handlers structure
 *
 * @return N/A
 */
void webusb_register_request_handlers(struct webusb_req_handlers *handlers)
{
	req_handlers = handlers;
}

/**
 * @brief EP Bulk IN handler, used to send data to the Host
 *
 * @param ep        Endpoint address.
 * @param ep_status Endpoint status code.
 *
 * @return  N/A.
 */
static void webusb_serial_bulk_in(u8_t ep,
		enum usb_dc_ep_cb_status_code ep_status)
{
	SYS_LOG_DBG("ep %x status %d", ep, ep_status);
}

/**
 * @brief Callback used to send data to the Host
 *
 * @param buf Buffer containing data to be sent to the Host.
 *
 * @return Number of bytes sent..
 */
int webusb_write(u8_t *data, size_t len)
{
	u32_t bytes_written = 0;
	struct webusb_serial_dev_data_t * const dev_data =
	    DEV_DATA(webusb_serial_dev);

	if (!dev_data || dev_data->usb_status != USB_DC_CONFIGURED) {
		return 0;
	}

	for (size_t size = 0; len > 0; len -= size) {
		size = (len < WEBUSB_TX_BUFFER_SIZE ? len : WEBUSB_TX_BUFFER_SIZE);
		if (usb_write(WEBUSB_ENDP_IN, data, len, &size) != 0) {
			SYS_LOG_ERR("USB write failed\n");
		}
		bytes_written += size;
	}
	return bytes_written;
}

/**
 * @brief EP Bulk OUT handler, used to read the data received from the Host
 *
 * @param ep        Endpoint address.
 * @param ep_status Endpoint status code.
 *
 * @return  N/A.
 */
static void read_webusb(u8_t ep, size_t len)
{
	size_t read, bytes;

	u8_t *buf = req_handlers->get_buffer();  // request a buffer from the app
	if (buf == NULL) {
		SYS_LOG_ERR("WebUSB read failed: no free buffers");
		return;
	}

	// Quark SE USB controller FIFO entries are 32-bit words.
	for (bytes = 0; bytes < len; bytes+= read) {
		usb_read(ep, buf + bytes, 4, &read);
	}

	req_handlers->rx_handler(buf, len);
}

static void webusb_serial_bulk_out(u8_t ep, enum usb_dc_ep_cb_status_code ep_status)
{
	ARG_UNUSED(ep_status);
	int len = 0;

	if (!req_handlers || !req_handlers->rx_handler) {
		return;
	}

	if (usb_read(ep, NULL, 0, &len) < 0) {  // read available data length
		SYS_LOG_ERR("USB read failed");
		return;
	}

	for (; len > 0; len -= WEBUSB_RX_BUFFER_SIZE) {
		read_webusb(ep, len);
	}
}

/**
 * @brief Callback used to know the USB connection status
 *
 * @param status USB device status code.
 *
 * @return  N/A.
 */
static void webusb_serial_dev_status_cb(enum usb_dc_status_code status,
	  u8_t *param)
{
	struct webusb_serial_dev_data_t * const dev_data =
	    DEV_DATA(webusb_serial_dev);

	ARG_UNUSED(param);

	/* Store the new status */
	dev_data->usb_status = status;

	/* Check the USB status and do needed action if required */
	switch (status) {
	case USB_DC_ERROR:
		SYS_LOG_DBG("USB device error");
		break;
	case USB_DC_RESET:
		SYS_LOG_DBG("USB device reset detected");
		break;
	case USB_DC_CONNECTED:
		SYS_LOG_DBG("USB device connected");
		break;
	case USB_DC_CONFIGURED:
		SYS_LOG_DBG("USB device configured");
		break;
	case USB_DC_DISCONNECTED:
		SYS_LOG_DBG("USB device disconnected");
		break;
	case USB_DC_SUSPEND:
		SYS_LOG_DBG("USB device supended");
		break;
	case USB_DC_RESUME:
		SYS_LOG_DBG("USB device resumed");
		break;
	case USB_DC_UNKNOWN:
	default:
		SYS_LOG_DBG("USB unknown state");
		break;
	}
}

/* Describe EndPoints configuration */
static struct usb_ep_cfg_data webusb_serial_ep_data[] = {
	{
		.ep_cb	= webusb_serial_bulk_out,
		.ep_addr = WEBUSB_ENDP_OUT
	},
	{
		.ep_cb = webusb_serial_bulk_in,
		.ep_addr = WEBUSB_ENDP_IN
	}
};

/* Configuration of the WebUSB Device send to the USB Driver */
static struct usb_cfg_data webusb_serial_config = {
	.usb_device_description = webusb_serial_usb_description,
	.cb_usb_status = webusb_serial_dev_status_cb,
	.interface = {
		.class_handler = webusb_serial_class_handle_req,
		.custom_handler = webusb_serial_custom_handle_req,
		.vendor_handler = webusb_serial_vendor_handle_req,
		.payload_data = NULL,
	},
	.num_endpoints = WEBUSB_NUM_EP,
	.endpoint = webusb_serial_ep_data
};

/**
 * @brief Initialize WebUSB
 *
 * This routine is called to reset the chip in a quiescent state.
 *
 * @param dev WebUSB device struct.
 *
 * @return 0 on success, negative errno code on fail
 */
static int webusb_device_init(struct device *dev)
{
	struct webusb_serial_dev_data_t * const dev_data = DEV_DATA(dev);
	int ret;

	webusb_serial_config.interface.payload_data = dev_data->interface_data;
	webusb_serial_dev = dev;

	/* Initialize the WebUSB driver with the right configuration */
	ret = usb_set_config(&webusb_serial_config);
	if (ret < 0) {
		SYS_LOG_ERR("Failed to config USB");
		return ret;
	}

	/* Enable WebUSB driver */
	ret = usb_enable(&webusb_serial_config);
	if (ret < 0) {
		SYS_LOG_ERR("Failed to enable USB");
		return ret;
	}

	return 0;
}

static struct webusb_serial_dev_data_t webusb_serial_dev_data = {
	.usb_status = USB_DC_UNKNOWN,
};

DEVICE_INIT(webusb_serial, WEBUSB_SERIAL_PORT_NAME, &webusb_device_init,
			&webusb_serial_dev_data, NULL,
			APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
