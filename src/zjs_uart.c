#include <zephyr.h>
#include <uart.h>

#include "zjs_uart.h"
#include "zjs_util.h"

#define UART_BUFFER_SIZE    128

static struct device* uart_dev = NULL;
// Bytes of string data
static int position = 0;
// Holds string data between onread callbacks
static char uartBuf[UART_BUFFER_SIZE];

// Global UART pin object (since there is only one UART)
static jerry_object_t* uart_pin = NULL;

// This is the CB ID for the C callback. This should not have to be a
// global ID but the UART API's do not let you add a "handle" that is
// available when the ISR is called. For this reason we must make it global.
// Other modules would include this handle when setting up the ISR e.g. GPIO
static int32_t uart_cb_id = -1;

// Struct used to hold pointers to the onread function,
// string data, and callback ID so everything can be
// properly cleaned up after the callback is made
struct uart_cb_data {
    jerry_object_t* onread_func;
    int32_t onread_id;
};

// TX interrupt handled
static volatile bool tx = false;
// RX interrupt handled
static volatile bool rx = false;
static char byte;

// Any normal module ISR would also have a void* handle parameter or let you
// use CONTAINER_OF to retrieve it. Unfortunately we have to make the ID
// global to reference it.
static void uart_irq_handler(struct device *dev)
{
    uart_irq_update(dev);

    if (uart_irq_tx_ready(dev)) {
        tx = true;
    }

    if (uart_irq_rx_ready(dev)) {
        uart_fifo_read(dev, &byte, 1);
        rx = true;
        uartBuf[position] = byte;
        position++;
        uartBuf[position] = '\0';
        zjs_signal_callback(uart_cb_id);
    }
}

// Workaround for not being able to add a setter for an object property.
// Since this is not possible, we must have a C callback that will setup
// a JS callback once a UART interrupt comes in. This is essentially moving
// the logic that needs to be done in the ISR to a task context, where
// allocations are possible.
static void uart_c_callback(void* handle)
{
    struct uart_cb_data* data = (struct uart_cb_data*)handle;
    jerry_value_t onread_func_val = jerry_get_object_field_value(uart_pin,
            (const jerry_char_t *)"onread");
    /*
     * TODO: Adding the callback here is not the best way to do it.
     * Unfortunately jerryscript has no way of setting a "setter" to an
     * object property. So in this case where you have JavaScript that
     * does:
     *
     * uart.onread = function() {...}
     *
     * You will never be able to tell that onread was set until you
     * actually need to use it. This is why we essentially move the code that
     * initialized and calls the JS function i.e. uart.onread to here, in a C
     * callback that is in a task/fiber context, not an ISR. For this reason
     * we do not need to utilize the JS callback mechanism (we could but it
     * would just be unneeded complexity) and can just do it all here.
     */
    // If pin.onread has been set...
    if (jerry_value_is_function(onread_func_val)) {
        // Alloc for the saved char's
        jerry_value_t data_val[1];
        // Create a jerry string
        jerry_string_t *string  = jerry_create_string_sz(uartBuf, position);
        jerry_value_t string_value = jerry_create_string_value(string);
        // Aquire the string
        jerry_value_t string_ret = jerry_acquire_value(string_value);
        //data->data_val[0] = string_ret;
        data_val[0] = string_ret;

        // Get and aquire the function set to "uart.onread"
        data->onread_func = jerry_acquire_object(jerry_get_object_value(onread_func_val));
        /*
         * Since we are in a task context and in the main task loop, we can make
         * the JS call here without the worry of a recursion loop. Normally you
         * would want to have a JS callback already setup, and signal it in the
         * ISR, but the UART module does not make this possible.
         */
        jerry_call_function(data->onread_func, NULL, data_val, 1);

        jerry_release_object(data->onread_func);
        /*
         * TODO: jerryscript does have a feature that allows you to cleanup
         * an object when garbage collection destroys it (i.e. finalizer). This
         * means that our data handle will never get freed!!! But we need to keep
         * it around for the life of the UART object.
         */
        //task_free(data);
        position = 0;

    } else {
        DBG_PRINT("[uart] zjs_service_uart(): onread callback has not been set\n");
    }
}

// Write a string to UART
static bool uart_write(const jerry_object_t *function_obj_p,
                       const jerry_value_t this_val,
                       const jerry_value_t args_p[],
                       const jerry_length_t args_cnt,
                       jerry_value_t *ret_val_p)
{
    char* input_string;
    int i;
    jerry_string_t* data = jerry_get_string_value(args_p[0]);
    jerry_size_t len = jerry_get_string_size(data);

    input_string = task_malloc(sizeof(char*) * len + 1);
    int wlen = jerry_string_to_char_buffer(data, input_string, len);
    input_string[wlen] = '\0';

    DBG_PRINT("[uart] uart_write(): '%s'\n", input_string);

    uart_irq_tx_enable(uart_dev);

    for (i = 0; i < len; i++) {
        tx = false;
        uart_fifo_fill(uart_dev, input_string + i, 1);
        while (tx == false);
    }

    uart_irq_tx_disable(uart_dev);

    task_free(input_string);

    return true;
}

// Open a UART pin, returns a UART pin object
bool zjs_uart_open(const jerry_object_t *function_obj_p,
                   const jerry_value_t this_val,
                   const jerry_value_t args_p[],
                   const jerry_length_t args_cnt,
                   jerry_value_t *ret_val_p)
{
    DBG_PRINT("[uart] zjs_uart_open(): Opening UART\n");
    // This handle is not entirely needed, since we save the ID globally,
    // we only need that to reference the CB. This handle is temporary to show
    // the proper use of callbacks in "normal" situations e.g. GPIO.
    struct uart_cb_data* handle = task_malloc(sizeof(struct uart_cb_data));
    memset(handle, 0, sizeof(struct uart_cb_data));
    handle->onread_id = -1;
    uart_cb_id = zjs_add_c_callback(handle, uart_c_callback);
    // This is all we can do at this point, more data will be added to this
    // handle once we get a UART ISR and the C callback is called i.e. the JS
    // object data (string) for servicing the JS callback.

    uart_pin = jerry_create_object();
    jerry_value_t uart_pin_val = jerry_create_object_value(uart_pin);

    zjs_obj_add_function(uart_pin, uart_write, "write");

    *ret_val_p = uart_pin_val;

    return true;
}

// Initialize UART, returns the UART object from require('uart')
jerry_object_t* zjs_uart_init(void)
{
    uart_dev = device_get_binding("UART_0");
    if (!uart_dev) {
        DBG_PRINT("[uart] zjs_uart_init(): Cannot find UART_0 device\n");
        return NULL;
    }
    // Set the interrupt handler
    uart_irq_callback_set(uart_dev, uart_irq_handler);
    uart_irq_rx_enable(uart_dev);

    jerry_object_t *uart_obj = jerry_create_object();
    zjs_obj_add_function(uart_obj, zjs_uart_open, "open");

    return uart_obj;
}
