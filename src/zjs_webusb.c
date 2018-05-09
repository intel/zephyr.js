// Copyright (c) 2017-2018, Intel Corporation.

// C includes
#include <string.h>

// Zephyr includes
#include <uart.h>
#include "webusb_serial.h"

// ZJS includes
#include "zjs_buffer.h"
#include "zjs_event.h"
#include "zjs_util.h"

// max size of pending TX and RX data
#define RINGMAX 256

static jerry_value_t webusb_api = 0;

u8_t *webusb_origin_url = NULL;

struct k_timer uart_timer;

// The idea here is that there is one producer and one consumer of these
//   buffers; one side is in an interrupt handler and the other in the main
//   thread. The write index is only updated by the producer, and the read
//   index is only updated by the consumer. When the indexes are equal, the
//   buffer is empty; when the write index is one below the read index (mod
//   RINGMAX), the buffer is full with RINGMAX - 1 bytes.
typedef struct {
    u8_t buf[RINGMAX];
    // TODO: maybe read and write should be accessed atomically
    volatile int read;
    volatile int write;
} ringbuf_t;

static ringbuf_t *tx_ring = NULL;
static ringbuf_t *rx_ring = NULL;

/**
 * @file
 * @brief WebUSB module
 *
 * Provides WebUSB and Microsoft OS Descriptor 2.0 Platform Capability
 * descriptors. Also, it handles the WebUSB Device Requests and provides
 * the landing page and allowed origin descriptors.
 */

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

        if (index != 1 || !webusb_origin_url)
            return -ENOTSUP;

        *data = webusb_origin_url;
        *len = webusb_origin_url[0];

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

static ZJS_DECL_FUNC(zjs_webusb_set_url)
{
    // url string
    ZJS_VALIDATE_ARGS(Z_STRING);

    char *url = zjs_alloc_from_jstring(argv[0], NULL);
    if (!url) {
        return zjs_error("out of memory");
    }

    u8_t scheme = 255;  // default to no prefix
    int prefix_len = 0;
    if (!strncmp(url, "http://", 7)) {
        DBG_PRINT("Found http URL for WebUSB: Chrome only works with https!\n");
        scheme = 0;
        prefix_len = 7;
    } else if (!strncmp(url, "https://", 8)) {
        scheme = 1;
        prefix_len = 8;
    } else {
        DBG_PRINT("Found URL for WebUSB with unknown scheme\n");
    }

    int url_len = strlen(url) - prefix_len;
    if (url_len > 252) {
        return zjs_error("url too long");
    }

    zjs_free(webusb_origin_url);
    webusb_origin_url = zjs_malloc(3 + url_len);
    if (!webusb_origin_url) {
        zjs_free(url);
        return zjs_error("out of memory");
    }

    // URL Descriptor: https://wicg.github.io/webusb/#url-descriptor
    webusb_origin_url[0] = 3 + url_len;  // length
    webusb_origin_url[1] = 0x03;         // URL descriptor
    webusb_origin_url[2] = scheme;
    strncpy(webusb_origin_url + 3, url + prefix_len, url_len);
    zjs_free(url);

    return ZJS_UNDEFINED;
}

int fill_uart(struct device *dev, ringbuf_t *ring, int length)
{
    // requires: dev is a UART device, ring is a ring buffer, length is the max
    //             bytes to read, length must not be enough to overflow buffer
    //  returns: total number of bytes sent
    //  effects: fill UART dev with up to length bytes from ring at index, until
    //             the fifo is full
    int total = 0;
    while (length > 0) {
        int index = ring->read;
        int sent = uart_fifo_fill(dev, &ring->buf[index], length);
        if (sent == 0) {
            break;
        }
        index += sent;
        total += sent;
        if (index == RINGMAX) {
            index = 0;
        }
        ring->read = index;
        if (sent == length) {
            break;
        }
        length -= sent;
    }
    return total;
}

int read_uart(struct device *dev, ringbuf_t *ring, int length)
{
    // requires: dev is a UART device, ring is a ring buffer, length is the max
    //             bytes to write, length must not be enough to overflow buffer
    //  returns: true if all bytes read, false if more remain
    //  effects: consume bytes from UART dev up to length bytes into ring at
    //             index, until the fifo runs out
    int total = 0;
    while (length > 0) {
        int index = ring->write;
        int bytes = uart_fifo_read(dev, &ring->buf[index], length);
        if (bytes == 0) {
            break;
        }
        index += bytes;
        total += bytes;
        if (index == RINGMAX) {
            index = 0;
        }
        ring->write = index;
        if (bytes == length) {
            break;
        }
        length -= bytes;
    }
    return total;
}

// a zjs_pre_emit callback
static bool prepare_read(void *unused, jerry_value_t argv[], u32_t *argc,
                         const char *buffer, u32_t bytes)
{
    // effects: reads as many bytes as possible into a buffer argument
    int read = rx_ring->read;
    int write = rx_ring->write;
    int len = 0;

    if (read < write) {
        len = write - read;
    } else {
        len = write + RINGMAX - read;
    }

    zjs_buffer_t *zbuf;
    ZVAL buf = zjs_buffer_create(len, &zbuf);
    if (!zbuf) {
        ERR_PRINT("out of memory\n");
        return false;
    }

    len = 0;
    if (read > write) {
        // first read to RINGMAX
        len = RINGMAX - read;
        memcpy(zbuf->buffer, &rx_ring->buf[read], len);
        read = 0;
    }
    if (read < write) {
        // read the rest
        memcpy(zbuf->buffer + len, &rx_ring->buf[read], write - read);
        read = write;
    }
    rx_ring->read = read;

    argv[0] = jerry_acquire_value(buf);
    CHECK_REF_COUNT("prepare", buf);
    *argc = 1;
    return true;
}

static void fill_uart_from_tx(struct device *dev)
{
    static bool filling = false;
    if (filling) {
        return;
    }
    filling = true;

    int read = tx_ring->read;
    int write = tx_ring->write;
    if (read == write) {
        // empty
        uart_irq_tx_disable(dev);
    } else {
        int max = 0;
        bool cont = false;
        if (read < write) {
            max = write - read;
        } else {
            max = RINGMAX - read;
            cont = true;
        }
        if (fill_uart(dev, tx_ring, max) == max && cont) {
            // try wraparound section
            max = write - tx_ring->read;
            fill_uart(dev, tx_ring, max);
        }
    }

    filling = false;
}

static void uart_interrupt_handler(struct device *dev)
{
    uart_irq_update(dev);

    if (uart_irq_tx_ready(dev)) {
        // send buffer is what JS has written that we will read and send out
        fill_uart_from_tx(dev);
    }

    if (uart_irq_rx_ready(dev)) {
        // receive buffer is what we will write for JS to read
        int read = rx_ring->read;
        int write = rx_ring->write;
        bool wrote = false;
        if (write + 1 == read || (write == RINGMAX - 1 && read == 0)) {
            // full
            ERR_PRINT("receive overflow\n");
        } else {
            int max = 0;
            bool cont = false;
            if (write < read) {
                max = read - 1 - write;
            } else {
                max = RINGMAX - write;
                if (read == 0) {
                    --max;
                } else {
                    cont = true;
                }
            }
            int bytes = read_uart(dev, rx_ring, max);
            if (bytes > 0) {
                wrote = true;
            }
            if (bytes == max && cont) {
                // try wraparound section
                max = read - 1 - tx_ring->write;
                if (read_uart(dev, rx_ring, max) == max) {
                    // based on docs for uart_fifo_read, not continuing to read
                    //   here could cause problems
                    ERR_PRINT("read buffer full\n");
                }
            }
        }

        // TODO: could make event to be cancellable to implement newline-based
        zjs_defer_emit_event(webusb_api, "read", NULL, 0, prepare_read,
                             zjs_release_args);
    }
}

static ZJS_DECL_FUNC(zjs_webusb_write)
{
    // buffer to write
    ZJS_VALIDATE_ARGS(Z_BUFFER);

    zjs_buffer_t *buffer = zjs_buffer_find(argv[0]);

    // read == write means empty, leave one byte empty for full buffer
    int read = tx_ring->read;
    int write = tx_ring->write;

    int copied = 0;
    int size = buffer->bufsize;
    if (write >= read) {
        // first write to RINGMAX
        int len = RINGMAX - write;
        if (read == 0) {
            --len;
        }

        if (size < len) {
            len = size;
        }
        memcpy(&tx_ring->buf[write], buffer->buffer, len);
        copied = len;
        write += len;
        if (write == RINGMAX) {
            write = 0;
        }
    }
    if (size > copied && write < read - 1) {
        int len = read - 1 - write;
        if (size < len) {
            len = size;
        }
        memcpy(&tx_ring->buf[write + copied], buffer->buffer + copied, len);
        copied += len;
        write += len;
    }
    tx_ring->write = write;

    if (copied > 0) {
        struct device *dev = device_get_binding(WEBUSB_SERIAL_PORT_NAME);
        uart_irq_tx_enable(dev);
        // FIXME: supposedly this is bad to call uart from outside ISR
        fill_uart_from_tx(dev);
    }
    if (size > copied) {
        return zjs_error("output buffer full");
    }

    return ZJS_UNDEFINED;
}

typedef enum {
    UART_STATE_DTR_WAIT,
    UART_STATE_BAUDRATE_WAIT,
    UART_STATE_READY
} uart_state_t;
static uart_state_t uart_state = UART_STATE_DTR_WAIT;

static void check_uart(struct k_timer *timer)
{
    static int count = 0;
    int rval;
    u32_t result = 0;

    struct device *dev = device_get_binding(WEBUSB_SERIAL_PORT_NAME);
    if (!dev) {
        ERR_PRINT("serial port not found\n");
        k_timer_stop(timer);
        return;
    }

    switch (uart_state) {
    case UART_STATE_DTR_WAIT:
        uart_line_ctrl_get(dev, LINE_CTRL_DTR, &result);
        if (result) {
            uart_state = UART_STATE_BAUDRATE_WAIT;
            count = 0;
        }
        break;

    case UART_STATE_BAUDRATE_WAIT:
        rval = uart_line_ctrl_get(dev, LINE_CTRL_BAUD_RATE, &result);
        if (rval) {
            // failed to get baudrate, wait up to five seconds
            if (++count >= 50) {
                ERR_PRINT("failed to get baudrate\n");
                k_timer_stop(timer);
            }
        } else {
            DBG_PRINT("baudrate: %d\n", result);
            // apparently we don't actually need to use the baudrate, just need
            //   to make sure one has been set?
            uart_state = UART_STATE_READY;
            k_timer_stop(timer);
            uart_irq_tx_disable(dev);
            uart_irq_rx_disable(dev);
            uart_irq_callback_set(dev, uart_interrupt_handler);
            uart_irq_rx_enable(dev);
        }
        break;

    default:
        // only valid state left is ready, and timer shouldn't fire
        ZJS_ASSERT(false, "unexpected state");
        break;
    }
}

static void zjs_webusb_cleanup()
{
    zjs_free(tx_ring);
    zjs_free(rx_ring);
    tx_ring = rx_ring = NULL;
    jerry_release_value(webusb_api);
    webusb_api = 0;
    zjs_free(webusb_origin_url);
    webusb_origin_url = NULL;
}

static jerry_value_t zjs_webusb_init()
{
    if (webusb_api) {
        return jerry_acquire_value(webusb_api);
    }

    // create WebUSB object
    ZVAL prototype = zjs_create_object();
    zjs_native_func_t array[] = {
        { zjs_webusb_set_url, "setURL" },
        { zjs_webusb_write, "write" },
        { NULL, NULL }
    };
    zjs_obj_add_functions(prototype, array);

    webusb_api = zjs_create_object();
    zjs_make_emitter(webusb_api, prototype, NULL, zjs_webusb_cleanup);

    // set the custom and vendor request handlers
    webusb_register_request_handlers(&req_handlers);

    k_timer_init(&uart_timer, check_uart, NULL);
    k_timer_start(&uart_timer, 0, 100);

    tx_ring = zjs_malloc(sizeof(ringbuf_t));
    rx_ring = zjs_malloc(sizeof(ringbuf_t));
    if (!tx_ring || !rx_ring) {
        zjs_free(tx_ring);
        zjs_free(rx_ring);
        tx_ring = rx_ring = NULL;
        return zjs_error_context("out of memory", 0, 0);
    }

    memset(tx_ring, 0, sizeof(ringbuf_t));
    memset(rx_ring, 0, sizeof(ringbuf_t));
    return webusb_api;
}

JERRYX_NATIVE_MODULE(webusb, zjs_webusb_init)
