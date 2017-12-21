// Copyright (c) 2017, Intel Corporation.

/**
* @file
* @brief WebUSB specific configuration file
*
* Provides WebUSB and Microsoft OS Descriptor 2.0 Platform Capability
* descriptors. Also, it handles the WebUSB Device Requests and provides
* the landing page and allowed origin descriptors.
*/

#include "webusb_serial.h"

/* WebUSB Platform Capability Descriptor */
static const u8_t webusb_bos_descriptor[] = {
    /* Binary Object Store descriptor */
    0x05, 0x0F, 0x39, 0x00, 0x02,

    /* WebUSB Platform Capability Descriptor:
     * https://wicg.github.io/webusb/#webusb-platform-capability-descriptor
     */
    0x18, 0x10, 0x05, 0x00,

    /* PlatformCapability UUID */
    0x38, 0xB6, 0x08, 0x34, 0xA9, 0x09, 0xA0, 0x47,
    0x8B, 0xFD, 0xA0, 0x76, 0x88, 0x15, 0xB6, 0x65,

    /* Version, VendorCode, Landing Page */
    0x00, 0x01, 0x01, 0x01,

    /* Microsoft OS 2.0 Platform Capability Descriptor */
    0x1C, 0x10, 0x05, 0x00,

    /* MS OS 2.0 Platform Capability ID */
    0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C,
    0x9C, 0xD2, 0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F,

    /* Windows version */
    0x00, 0x00, 0x03, 0x06,

    /* Descriptor set length, Vendor code, Alternate enumeration code */
    0xB2, 0x00, 0x02, 0x00
};

/* Microsoft OS 2.0 Descriptor Set */
static const u8_t ms_os_20_descriptor_set[] = {
    0x0A, 0x00,              // wLength
    0x00, 0x00,              // MS OS 2.0 descriptor set header
    0x00, 0x00, 0x03, 0x06,  // Windows 8.1
    0xB2, 0x00,              // Size, MS OS 2.0 descriptor set

    // Configuration subset header
    0x08, 0x00,  // wLength
    0x01, 0x00,  // wDescriptorType
    0x00,        // bConfigurationValue
    0x00,        // bReserved
    0xA8, 0x00,  // wTotalLength of this subset header

    // Function subset header
    0x08, 0x00,  // wLength
    0x02, 0x00,  // wDescriptorType
    0x02,        // bFirstInterface
    0x00,        // bReserved
    0xA0, 0x00,  // wTotalLength of this subset header

    // Compatible ID descriptor
    0x14, 0x00,                                      // wLength
    0x03, 0x00,                                      // wDescriptorType
    'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,        // compatible ID
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // subCompatibleID

    // Extended properties descriptor with interface GUID
    0x84, 0x00,  // wLength
    0x04, 0x00,  // wDescriptorType
    0x07, 0x00,  // wPropertyDataType
    0x2A, 0x00,  // wPropertyNameLength
    // Property name : DeviceInterfaceGUIDs
    'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00,
    'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00, 'r', 0x00, 'f', 0x00,
    'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00,
    'D', 0x00, 's', 0x00, 0x00, 0x00,
    0x50, 0x00,  // wPropertyDataLength
    // Property data: {9D32F82C-1FB2-4486-8501-B6145B5BA336}
    '{', 0x00, '9', 0x00, 'D', 0x00, '3', 0x00, '2', 0x00, 'F', 0x00,
    '8', 0x00, '2', 0x00, 'C', 0x00, '-', 0x00, '1', 0x00, 'F', 0x00,
    'B', 0x00, '2', 0x00, '-', 0x00, '4', 0x00, '4', 0x00, '8', 0x00,
    '6', 0x00, '-', 0x00, '8', 0x00, '5', 0x00, '0', 0x00, '1', 0x00,
    '-', 0x00, 'B', 0x00, '6', 0x00, '1', 0x00, '4', 0x00, '5', 0x00,
    'B', 0x00, '5', 0x00, 'B', 0x00, 'A', 0x00, '3', 0x00, '3', 0x00,
    '6', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00
};

/* Microsoft OS 2.0 descriptor request */
#define MS_OS_20_REQUEST_DESCRIPTOR 0x07

/* URL Descriptor: https://wicg.github.io/webusb/#url-descriptor */
static const u8_t webusb_origin_url[] = {
    0x1F,  // Length
    0x03,  // URL descriptor
    0x01,  // Scheme https://
    'i', 'n', 't', 'e', 'l', '.', 'g', 'i', 't', 'h', 'u', 'b', '.',
    'i', 'o', '/', 'z', 'e', 'p', 'h', 'y', 'r', 'j', 's', '-', 'i',
    'd', 'e'
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
int webusb_custom_handler(struct usb_setup_packet *pSetup, s32_t *len,
                          u8_t **data)
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
int webusb_vendor_handler(struct usb_setup_packet *pSetup, s32_t *len,
                          u8_t **data)
{
    /* Get URL request */
    if (pSetup->bRequest == 0x01 && pSetup->wIndex == 0x02) {
        u8_t index = GET_DESC_INDEX(pSetup->wValue);

        if (index != 1)
            return -ENOTSUP;

        *data = (u8_t *)(&webusb_origin_url);
        *len = sizeof(webusb_origin_url);

        return 0;
    } else if (pSetup->bRequest == 0x02 &&
               pSetup->wIndex == MS_OS_20_REQUEST_DESCRIPTOR) {

        *data = (u8_t *)(ms_os_20_descriptor_set);
        *len = sizeof(ms_os_20_descriptor_set);

        return 0;
    }

    return -ENOTSUP;
}

/* Custom and Vendor request handlers */
static struct webusb_req_handlers req_handlers = {
    .custom_handler = webusb_custom_handler,
    .vendor_handler = webusb_vendor_handler,
};

/*
 * @brief register a callback for handling custom and vendor requests
 *
 * @return N/A
 */
void webusb_init()
{
    /* Set the custom and vendor request handlers */
    webusb_register_request_handlers(&req_handlers);
}
