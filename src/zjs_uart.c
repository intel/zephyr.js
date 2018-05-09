// Copyright (c) 2016-2018, Intel Corporation.

#ifdef BUILD_MODULE_UART

// C includes
#include <string.h>

// Zephyr includes
#include <device.h>
#include <uart.h>
#include <zephyr.h>

// ZJS includes
#ifndef ZJS_LINUX_BUILD
#include "zjs_zephyr_port.h"
#else
#include "zjs_linux_port.h"
#endif
#include "zjs_buffer.h"
#include "zjs_callbacks.h"
#include "zjs_common.h"
#include "zjs_event.h"
#include "zjs_util.h"

static jerry_value_t zjs_uart_prototype;

typedef struct {
    const char *name;
    const char *port;
} uart_dev_map;

// Struct used to hold pointers to the onread function,
// string data, and callback ID so everything can be
// properly cleaned up after the callback is made
typedef struct {
    jerry_value_t uart_obj;
    char *buf;
    u32_t size;
    u32_t min;
    u32_t max;
} uart_handle;

static uart_dev_map device_map[] = {
#ifdef CONFIG_CDC_ACM_PORT_NAME
    { CONFIG_CDC_ACM_PORT_NAME, "tty0" }
#else
    { "UART_0", "tty0" }
#endif
};

#define UART_BUFFER_INITIAL_SIZE 16

static uart_handle *handle = NULL;
static struct device *uart_dev = NULL;

// This is the CB ID for the C callback. This should not have to be a
// global ID but the UART API's do not let you add a "handle" that is
// available when the ISR is called. For this reason we must make it global.
// Other modules would include this handle when setting up the ISR e.g. GPIO
static zjs_callback_id read_id = -1;
// TX interrupt handled
static volatile bool tx = false;
// RX interrupt handled
static volatile bool rx = false;

static jerry_value_t make_uart_error(const char *name, const char *msg)
{

    ZVAL ret = zjs_create_object();
    if (name) {
        zjs_obj_add_string(ret, "name", name);
    } else {
        DBG_PRINT("error must have a name\n");
        return ZJS_UNDEFINED;
    }
    if (msg) {
        zjs_obj_add_string(ret, "message", msg);
    } else {
        DBG_PRINT("error must have a message\n");
        return ZJS_UNDEFINED;
    }
    return jerry_acquire_value(ret);
}

static uart_handle *new_uart_handle(void)
{
    uart_handle *h = zjs_malloc(sizeof(uart_handle));
    memset(h, 0, sizeof(uart_handle));

    h->min = 1;
    h->max = UART_BUFFER_INITIAL_SIZE;
    h->buf = zjs_malloc(UART_BUFFER_INITIAL_SIZE);
    h->size = 0;
    memset(h->buf, 0, UART_BUFFER_INITIAL_SIZE);

    return h;
}

static void uart_c_callback(void *h, const void *args)
{
    if (!handle) {
        DBG_PRINT("UART handle not found\n");
        return;
    }
    if (handle->size >= handle->min) {
        zjs_buffer_t *buffer;
        ZVAL buf = zjs_buffer_create(handle->size, &buffer);
        if (buffer) {
            memcpy(buffer->buffer, args, handle->size);
        }
        zjs_emit_event(handle->uart_obj, "read", &buf, 1);

        handle->size = 0;
    }
}

static void uart_irq_handler(struct device *dev)
{
    uart_irq_update(dev);

    if (!uart_irq_is_pending(dev)) {
        return;
    }

    if (uart_irq_tx_ready(dev)) {
        tx = true;
    }

    if (uart_irq_rx_ready(dev)) {
        u32_t len = uart_fifo_read(dev, handle->buf, handle->max);
        rx = true;
        handle->buf[len] = '\0';
        handle->size = len;
        zjs_signal_callback(read_id, handle->buf, len);
    }
}

static int write_data(struct device *dev, const char *buf, int len)
{
    uart_irq_tx_enable(dev);
    int sent = 0;
    int sent_total = 0;

    // Due to ZEP-2401 - Transmission via CDC_ACM on Arduino 101 is limited to
    // 4 bytes at a time, so we have to loop.
    while (len > 0) {
        tx = false;
        sent = uart_fifo_fill(dev, (const u8_t *)buf, len);
        if (!sent) {
            return sent_total;
        }

        while (tx == false) {
            ;
        }

        len -= sent;
        buf += sent;
        sent_total += sent;
    }

    uart_irq_tx_disable(dev);
    return sent_total;
}

static ZJS_DECL_FUNC(uart_write)
{
    // args: buffer
    ZJS_VALIDATE_ARGS(Z_BUFFER);

    zjs_buffer_t *buffer = zjs_buffer_find(argv[0]);

    int wrote = write_data(uart_dev, (const char *)buffer->buffer,
                           buffer->bufsize);
    if (wrote == buffer->bufsize) {
        return jerry_create_number(wrote);
    } else {
        return make_uart_error("WriteError", "incomplete write");
    }
}

static ZJS_DECL_FUNC(uart_set_read_range)
{
    // args: min, max
    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER);

    u32_t min = jerry_get_number_value(argv[0]);
    u32_t max = jerry_get_number_value(argv[1]);

    if (handle->buf) {
        void *old = handle->buf;
        handle->buf = zjs_malloc(max);
        memcpy(handle->buf, old, handle->size);
        handle->buf[handle->size] = '\0';
        zjs_free(old);
    } else {
        handle->buf = zjs_malloc(max);
        memset(handle->buf, 0, max);
        handle->size = 0;
    }
    handle->min = min;
    handle->max = max;

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(uart_init)
{
    // args: initialization object
    ZJS_VALIDATE_ARGS(Z_OBJECT);

    int i;
    u32_t baud = 115200;
#ifdef CONFIG_UART_LINE_CTRL
    int ret, dtr;
#endif

    ZVAL port_val = zjs_get_property(argv[0], "port");

    const int MAX_PORT_LENGTH = 16;
    jerry_size_t size = MAX_PORT_LENGTH;
    char port[size];

    zjs_copy_jstring(port_val, port, &size);

    if (!size) {
        DBG_PRINT("port length is too long\n");
        return make_uart_error("TypeMismatchError", "port length is too long");
    }

    ZVAL baud_val = zjs_get_property(argv[0], "baud");
    if (jerry_value_is_number(baud_val)) {
        baud = (int)jerry_get_number_value(baud_val);
    }

    for (i = 0; i < (sizeof(device_map) / sizeof(device_map[0])); ++i) {
        // FIXME: we allowed 16-char string above but are only looking at 4?
        if (strncmp(device_map[0].port, port, 4) == 0) {
            uart_dev = device_get_binding((char *)device_map[0].name);
            break;
        }
    }
    if (uart_dev == NULL) {
        DBG_PRINT("could not find port %s\n", port);
        return make_uart_error("NotFoundError", "could not find port provided");
    }

    // Hardware initialization
#ifdef CONFIG_UART_LINE_CTRL
    DBG_PRINT("waiting for DTR\n");
    while (1) {
        uart_line_ctrl_get(uart_dev, LINE_CTRL_DTR, (u32_t *)&dtr);
        if (dtr) {
            break;
        }
    }

    ret = uart_line_ctrl_set(uart_dev, LINE_CTRL_DCD, 1);
    if (ret) {
        DBG_PRINT("failed to set DCD, ret code %d\n", ret);
    }

    ret = uart_line_ctrl_set(uart_dev, LINE_CTRL_DSR, 1);
    if (ret) {
        DBG_PRINT("failed to set DSR, ret code %d\n", ret);
    }

    /* Wait 1 sec for the host to do all settings */
    zjs_sleep(1000);

    ret = uart_line_ctrl_set(uart_dev, LINE_CTRL_BAUD_RATE, baud);
    if (ret) {
        DBG_PRINT("Failed to set baud, ret code %d\n", ret);
    }

    u32_t test_baud = baud;

    ret = uart_line_ctrl_get(uart_dev, LINE_CTRL_BAUD_RATE,
                             (u32_t *)&test_baud);
    if (ret) {
        DBG_PRINT("failed to get baudrate, ret code %d\n", ret);
    } else {
        if (test_baud != baud) {
            DBG_PRINT("baudrate was not set successfully\n");
            return make_uart_error("InternalError", "baud could not be set");
        }
    }
    DBG_PRINT("baudrate set successfully: %ld\n", baud);
#endif
    // Set the interrupt handler
    uart_irq_callback_set(uart_dev, uart_irq_handler);

    uart_irq_rx_enable(uart_dev);

    // JerryScript object initialization
    handle = new_uart_handle();

    handle->uart_obj = zjs_create_object();

    zjs_make_emitter(handle->uart_obj, zjs_uart_prototype, NULL, NULL);

    read_id = zjs_add_c_callback(handle, uart_c_callback);

    return handle->uart_obj;
}

static void zjs_uart_cleanup(void *native)
{
    jerry_release_value(zjs_uart_prototype);
}

static const jerry_object_native_info_t uart_module_type_info = {
    .free_cb = zjs_uart_cleanup
};

static jerry_value_t zjs_uart_init()
{
    zjs_native_func_t array[] = {
        { uart_write, "write" },
        { uart_set_read_range, "setReadRange" },
        { NULL, NULL }
    };
    zjs_uart_prototype = zjs_create_object();
    zjs_obj_add_functions(zjs_uart_prototype, array);

    jerry_value_t uart_obj = zjs_create_object();
    zjs_obj_add_function(uart_obj, "init", uart_init);
    // Set up cleanup function for when the object gets freed
    jerry_set_object_native_pointer(uart_obj, NULL, &uart_module_type_info);
    return uart_obj;
}

JERRYX_NATIVE_MODULE(uart, zjs_uart_init)
#endif  // BUILD_MODULE_UART
