// Copyright (c) 2016, Intel Corporation.

#ifdef BUILD_MODULE_UART

#include <zephyr.h>
#include <device.h>
#include <uart.h>

#include "string.h"

#ifndef ZJS_LINUX_BUILD
#include "zjs_zephyr_port.h"
#else
#include "zjs_linux_port.h"
#endif
#include "zjs_common.h"
#include "zjs_util.h"
#include "zjs_promise.h"
#include "zjs_callbacks.h"
#include "zjs_event.h"
#include "zjs_buffer.h"

typedef struct {
    const char* name;
    const char* port;
} uart_dev_map;

// Struct used to hold pointers to the onread function,
// string data, and callback ID so everything can be
// properly cleaned up after the callback is made
typedef struct {
    jerry_value_t uart_obj;
    jerry_value_t buf_obj;
    char* buf;
    uint32_t size;
    uint32_t min;
    uint32_t max;
} uart_handle;

static uart_dev_map device_map[] = {
#ifdef CONFIG_CDC_ACM_PORT_NAME
    { CONFIG_CDC_ACM_PORT_NAME, "tty0" }
#else
    { "UART_0", "tty0" }
#endif
};

#define UART_BUFFER_INITIAL_SIZE    16

static uart_handle* handle = NULL;
static struct device* uart_dev = NULL;

// This is the CB ID for the C callback. This should not have to be a
// global ID but the UART API's do not let you add a "handle" that is
// available when the ISR is called. For this reason we must make it global.
// Other modules would include this handle when setting up the ISR e.g. GPIO
static zjs_callback_id read_id = -1;
// TX interrupt handled
static volatile bool tx = false;
// RX interrupt handled
static volatile bool rx = false;

static jerry_value_t make_uart_error(const char* name, const char* msg)
{

    jerry_value_t ret = jerry_create_object();
    if (name) {
        zjs_obj_add_string(ret, name, "name");
    } else {
        DBG_PRINT("error must have a name\n");
        jerry_release_value(ret);
        return ZJS_UNDEFINED;
    }
    if (msg) {
        zjs_obj_add_string(ret, msg, "message");
    } else {
        DBG_PRINT("error must have a message\n");
        jerry_release_value(ret);
        return ZJS_UNDEFINED;
    }
    return ret;
}

static uart_handle* new_uart_handle(void)
{
    uart_handle* h = zjs_malloc(sizeof(uart_handle));
    memset(h, 0, sizeof(uart_handle));

    h->min = 1;
    h->max = UART_BUFFER_INITIAL_SIZE;
    h->buf = zjs_malloc(UART_BUFFER_INITIAL_SIZE);
    h->size = 0;
    memset(h->buf, 0, UART_BUFFER_INITIAL_SIZE);

    return h;
}

static void post_event(void* h)
{
    jerry_release_value(handle->buf_obj);
}

static void uart_c_callback(void* h, void* args)
{
    if (!handle) {
        DBG_PRINT("UART handle not found\n");
        return;
    }
    if (handle->size >= handle->min) {
        handle->buf_obj = zjs_buffer_create(handle->size);
        zjs_buffer_t* buffer = zjs_buffer_find(handle->buf_obj);

        memcpy(buffer->buffer, args, handle->size);

        zjs_trigger_event_now(handle->uart_obj, "read", &handle->buf_obj, 1, post_event, NULL);

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
        uint32_t len = uart_fifo_read(dev, handle->buf, handle->max);
        rx = true;
        handle->buf[len] = '\0';
        handle->size = len;
        zjs_signal_callback(read_id, handle->buf, len);
    }
}

static void write_data(struct device *dev, const char *buf, int len)
{
    uart_irq_tx_enable(dev);

    tx = false;
    uart_fifo_fill(dev, buf, len);
    while (tx == false);

    uart_irq_tx_disable(dev);
}

static jerry_value_t uart_write(const jerry_value_t function_obj,
                                const jerry_value_t this,
                                const jerry_value_t argv[],
                                const jerry_length_t argc)
{
    jerry_value_t promise = jerry_create_object();

    zjs_make_promise(promise, NULL, NULL);

    if (!jerry_value_is_object(argv[0])) {
        DBG_PRINT("first parameter must be a Buffer");
        jerry_value_t error = make_uart_error("TypeMismatchError",
                "first parameter must be a Buffer");
        zjs_reject_promise(promise, &error, 1);
        return promise;
    }

    zjs_buffer_t* buffer = zjs_buffer_find(argv[0]);

    write_data(uart_dev, (const char*)buffer->buffer, buffer->bufsize);

    zjs_fulfill_promise(promise, NULL, 0);

    return promise;
}

static jerry_value_t uart_set_read_range(const jerry_value_t function_obj,
                                         const jerry_value_t this,
                                         const jerry_value_t argv[],
                                         const jerry_length_t argc)
{
    if (!jerry_value_is_number(argv[0]) || !jerry_value_is_number(argv[1])) {
        DBG_PRINT("parameters must be min and max read size\n");
        return ZJS_UNDEFINED;
    }

    uint32_t min = jerry_get_number_value(argv[0]);
    uint32_t max = jerry_get_number_value(argv[1]);

    if (handle->buf) {
        void* old = handle->buf;
        handle->buf = zjs_malloc(max);
        memcpy(handle->buf, old, handle->size);
        handle->buf[handle->size] = '\0';
        zjs_free(old);
        //if (handle->size >= min) {
        //    jerry_value_t data = jerry_create_string((const jerry_char_t *)handle->buf);
        //    zjs_trigger_event(handle->uart_obj, "read", &data, 1, NULL, NULL);
        //}
    } else {
        handle->buf = zjs_malloc(max);
        memset(handle->buf, 0, max);
        handle->size = 0;
    }
    handle->min = min;
    handle->max = max;

    return ZJS_UNDEFINED;
}

static jerry_value_t uart_init(const jerry_value_t function_obj,
                               const jerry_value_t this,
                               const jerry_value_t argv[],
                               const jerry_length_t argc)
{
    int i;
    int baud = 115200;
#ifdef CONFIG_UART_LINE_CTRL
    int ret, dtr;
#endif
    jerry_value_t promise = jerry_create_object();

    zjs_make_promise(promise, NULL, NULL);

    if (!jerry_value_is_object(argv[0])) {
        DBG_PRINT("first parameter must be UART options object\n");
        jerry_value_t error = make_uart_error("TypeMismatchError",
                "first parameter must be UART options");
        zjs_reject_promise(promise, &error, 1);
        return promise;
    }

    jerry_value_t port_val = zjs_get_property(argv[0], "port");

    int sz = jerry_get_string_size(port_val);
    if (sz > 16) {
        DBG_PRINT("port length is too long\n");
        jerry_value_t error = make_uart_error("TypeMismatchError",
                "port length is too long");
        zjs_reject_promise(promise, &error, 1);
        return promise;
    }

    char port[sz + 1];
    int len = jerry_string_to_char_buffer(port_val, (jerry_char_t *)port, sz);
    port[len] = '\0';

    jerry_release_value(port_val);

    if (sz != len) {
        DBG_PRINT("size mismatch, sz=%u, len=%u\n", sz, len);
        jerry_value_t error = make_uart_error("TypeMismatchError",
                "size mismatch");
        zjs_reject_promise(promise, &error, 1);
        return promise;
    }

    jerry_value_t baud_val = zjs_get_property(argv[0], "baud");

    if (jerry_value_is_number(baud_val)) {
        baud = (int)jerry_get_number_value(baud_val);
    }
    for (i = 0; i < (sizeof(device_map) / sizeof(device_map[0])); ++i) {
        if (strncmp(device_map[0].port, port, 4) == 0) {
            uart_dev = device_get_binding((char*)device_map[0].name);
            break;
        }
    }
    if (uart_dev == NULL) {
        DBG_PRINT("could not find port %s\n", port);
        jerry_value_t error = make_uart_error("NotFoundError",
                "could not find port provided");
        zjs_reject_promise(promise, &error, 1);
        return promise;
    }

    // Hardware initialization
#ifdef CONFIG_UART_LINE_CTRL
    DBG_PRINT("waiting for DTR\n");
    while (1) {
        uart_line_ctrl_get(uart_dev, LINE_CTRL_DTR, (uint32_t*)&dtr);
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

    int test_baud = baud;

    ret = uart_line_ctrl_get(uart_dev, LINE_CTRL_BAUD_RATE, (uint32_t*)&test_baud);
    if (ret) {
        DBG_PRINT("failed to get baudrate, ret code %d\n", ret);
    } else {
        if (test_baud != baud) {
            DBG_PRINT("baudrate was not set successfully");
            jerry_value_t error = make_uart_error("InternalError",
                    "baud could not be set");
            zjs_reject_promise(promise, &error, 1);
            return promise;
        }
    }
    DBG_PRINT("baudrate set successfully: %ld\n", baud);
#endif
    // Set the interrupt handler
    uart_irq_callback_set(uart_dev, uart_irq_handler);

    uart_irq_rx_enable(uart_dev);

    // JerryScript object initialization
    handle = new_uart_handle();

    handle->uart_obj = jerry_create_object();

    zjs_make_event(handle->uart_obj);

    zjs_obj_add_function(handle->uart_obj, uart_write, "write");
    zjs_obj_add_function(handle->uart_obj, uart_set_read_range, "setReadRange");

    zjs_fulfill_promise(promise, &handle->uart_obj, 1);

    read_id = zjs_add_c_callback(handle, uart_c_callback);

    return promise;
}

jerry_value_t zjs_init_uart(void)
{
    jerry_value_t uart_obj = jerry_create_object();

    zjs_obj_add_function(uart_obj, uart_init, "init");

    return uart_obj;
}

#endif // BUILD_MODULE_UART
