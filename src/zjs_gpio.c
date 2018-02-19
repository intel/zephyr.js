// Copyright (c) 2016-2018, Intel Corporation.

// enable to use function tracing for debug purposes
#if 0
#define USE_FTRACE
static char FTRACE_PREFIX[] = "gpio";
#endif

// C includes
#include <stdlib.h>
#include <string.h>

// Zephyr includes
#ifndef ZJS_LINUX_BUILD
#include <gpio.h>
#include <misc/util.h>
#include <zephyr.h>
#endif

// ZJS includes
#include "zjs_board.h"
#include "zjs_callbacks.h"
#include "zjs_common.h"
#include "zjs_util.h"

#ifdef ZJS_GPIO_MOCK
#include "zjs_gpio_mock.h"
#else
#define DEVICE struct device *
#define zjs_gpio_mock_pre_init()
#define zjs_gpio_mock_post_init(gpio_obj)
#define zjs_gpio_mock_cleanup()
#endif

static jerry_value_t gpio_pin_prototype = 0;

// Handle for GPIO input pins, passed around between ISR/C callbacks
typedef struct gpio_handle {
    struct gpio_callback callback;  // Callback structure for zephyr
    DEVICE port;                    // Pin's port
    u32_t pin;                      // Pin associated with this handle
    u32_t value;                    // Value of the pin
    zjs_callback_id callbackId;     // ID for the C callback
    jerry_value_t pin_obj;          // Pin object returned from open()
    u32_t last;
    u8_t edge_both;
    bool active_low;
    bool closed;
} gpio_handle_t;

static void zjs_gpio_close(gpio_handle_t *handle)
{
    FTRACE("handle = %p\n", handle);
    zjs_remove_callback(handle->callbackId);
    gpio_remove_callback(handle->port, &handle->callback);
    handle->closed = true;
}

static void zjs_gpio_free_cb(void *native)
{
    FTRACE("native = %p\n", native);
    gpio_handle_t *handle = (gpio_handle_t *)native;
    if (!handle->closed)
        zjs_gpio_close(handle);

    zjs_free(handle);
}

static const jerry_object_native_info_t gpio_type_info = {
    .free_cb = zjs_gpio_free_cb
};

// non-static so that mock can access it, meant to be private though
void gpio_internal_lookup_pin(const jerry_value_t pin_obj, DEVICE *port,
                              int *pin)
{
    FTRACE("pin_obj = %p, port = %p, pin = %p\n", (void *)pin_obj, port, pin);
    gpio_handle_t *handle;
    const jerry_object_native_info_t *tmp;
    jerry_get_object_native_pointer(pin_obj, (void **)&handle, &tmp);
    if (tmp == &gpio_type_info) {
        *port = handle->port;
        *pin = handle->pin;
    }
}

// C callback from task context in response to GPIO input interrupt
static void zjs_gpio_c_callback(void *h, const void *args)
{
    FTRACE("h = %p, args = %p\n", h, args);
    gpio_handle_t *handle = (gpio_handle_t *)h;
    if (handle->closed) {
        ERR_PRINT("unexpected callback after close\n");
        return;
    }
    ZVAL onchange_func = zjs_get_property(handle->pin_obj, "onchange");

    // If pin.onChange exists, call it
    if (jerry_value_is_function(onchange_func)) {
        ZVAL event = zjs_create_object();
        u32_t *the_args = (u32_t *)args;
        // Put the numeric GPIO trigger value in the object
        zjs_obj_add_number(event, "value", the_args[0]);

        // Call the JS callback
        jerry_call_function(onchange_func, ZJS_UNDEFINED, &event, 1);
    } else {
        DBG_PRINT(("onChange has not been registered\n"));
    }
}

// Callback when a GPIO input fires
// INTERRUPT SAFE FUNCTION: No JerryScript VM, allocs, or release prints!
static void zjs_gpio_zephyr_callback(DEVICE port, struct gpio_callback *cb,
                                     u32_t pins)
{
    FTRACE("port = %p, cb = %p, pins = %x\n", (void *)port, cb, pins);
    // Get our handle for this pin
    gpio_handle_t *handle = CONTAINER_OF(cb, gpio_handle_t, callback);
    // Read the value and save it in the handle
    gpio_pin_read(port, handle->pin, &handle->value);
    if ((handle->edge_both && handle->value != handle->last) ||
        !handle->edge_both) {
        u32_t args[] = { handle->value, pins };
        // Signal the C callback, where we call the JS callback
        zjs_signal_callback(handle->callbackId, args, sizeof(args));
        handle->last = handle->value;
    }
}

static ZJS_DECL_FUNC(zjs_gpio_pin_read)
{
    // requires: this is a GPIOPin object from zjs_gpio_open, takes no args
    //  effects: reads a logical value from the pin and returns it in ret_val_p
    ZJS_GET_HANDLE(this, gpio_handle_t, handle, gpio_type_info);

    if (handle->closed) {
        return zjs_error("pin closed");
    }

    u32_t value;
    int rval = gpio_pin_read(handle->port, handle->pin, &value);
    if (rval) {
        ERR_PRINT("PIN: #%d\n", (int)handle->pin);
        return zjs_error("read failed");
    }

    if (handle->active_low) {
        value = !value;
    }

    return jerry_create_number(value);
}

static ZJS_DECL_FUNC(zjs_gpio_pin_write)
{
    // requires: this is a GPIOPin object from zjs_gpio_open, takes one arg,
    //             the logical boolean value to set to the pin (true = active)
    //  effects: writes the logical value to the pin

    // args: pin value
    ZJS_VALIDATE_ARGS(Z_BOOL Z_NUMBER);

    ZJS_GET_HANDLE(this, gpio_handle_t, handle, gpio_type_info);
    if (handle->closed) {
        return zjs_error("pin closed");
    }

    u32_t value;
    // TODO: Remove this deprecated option eventually
    if (jerry_value_is_boolean(argv[0])) {
        ZJS_PRINT("Deprecated! gpio.write() no longer takes a boolean!\n");
        value = jerry_get_boolean_value(argv[0]) ? 1 : 0;
    } else {
        value = (u32_t)jerry_get_number_value(argv[0]);
    }

    if (handle->active_low) {
        value = !value;
    }

    int rval = gpio_pin_write(handle->port, handle->pin, value);
    if (rval) {
        ERR_PRINT("GPIO: #%d!\n", (int)handle->pin);
        return zjs_error("write failed");
    }

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_gpio_pin_close)
{
    ZJS_GET_HANDLE(this, gpio_handle_t, handle, gpio_type_info);
    if (handle->closed)
        return zjs_error("already closed");

    zjs_gpio_close(handle);
    return ZJS_UNDEFINED;
}

enum {
    ZJS_DIR_INPUT,
    ZJS_DIR_OUTPUT
};

enum {
    ZJS_EDGE_NONE,
    ZJS_EDGE_RISING = GPIO_INT | GPIO_INT_EDGE | GPIO_INT_ACTIVE_HIGH,
    ZJS_EDGE_FALLING = GPIO_INT | GPIO_INT_EDGE | GPIO_INT_ACTIVE_LOW,
    ZJS_EDGE_BOTH = GPIO_INT | GPIO_INT_EDGE | GPIO_INT_DOUBLE_EDGE
};

enum {
    ZJS_PULL_NONE,
    ZJS_PULL_UP,
    ZJS_PULL_DOWN
};

static ZJS_DECL_FUNC(zjs_gpio_open)
{
    // args: initialization object or int/string pin number
    ZJS_VALIDATE_ARGS(Z_NUMBER Z_STRING Z_OBJECT);

    ZVAL_MUTABLE pin_str = 0;
    jerry_value_t pin_val = argv[0];
    jerry_value_t init = 0;

    if (jerry_value_is_object(argv[0])) {
        init = argv[0];
        pin_str = zjs_get_property(init, "pin");
        pin_val = pin_str;
    }

    char devname[20];
    int pin = zjs_board_find_gpio(pin_val, devname, 20);
    if (pin == FIND_PIN_INVALID) {
        return TYPE_ERROR("bad pin argument");
    } else if (pin == FIND_DEVICE_FAILURE) {
        return zjs_error("device not found");
    } else if (pin < 0) {
        return zjs_error("pin not found");
    }
    DEVICE gpiodev = device_get_binding(devname);
    if (!gpiodev) {
        return zjs_error("device not found");
    }

    // ignore mapping for now as we can do it automatically

    // set defaults
    bool active_low = false;
    int dir = ZJS_DIR_OUTPUT;
    int edge = ZJS_EDGE_NONE;
    int pull = ZJS_PULL_NONE;

    if (init) {
        // TODO: Remove warning eventually
        ZVAL dirprop = zjs_get_property(init, "direction");
        if (!jerry_value_is_undefined(dirprop)) {
            ZJS_PRINT("Deprecated! gpio.open() direction property is "
                      "now 'mode'\n");
        }

        // override defaults where specified
        ZJS_REQUIRE_BOOL_IF_PROP(init, "activeLow", &active_low);

        str2int_t mode_map[] = {
            { "in", ZJS_DIR_INPUT },
            { "out", ZJS_DIR_OUTPUT },
            { NULL, 0 }
        };
        ZJS_REQUIRE_STR_IF_PROP_MAP(init, "mode", mode_map, 10, &dir);

        str2int_t edge_map[] = {
            { "none", ZJS_EDGE_NONE },
            { "rising", ZJS_EDGE_RISING },
            { "falling", ZJS_EDGE_FALLING },
            { "any", ZJS_EDGE_BOTH },
            { NULL, 0 }
        };
        ZJS_REQUIRE_STR_IF_PROP_MAP(init, "edge", edge_map, 10, &edge);

        str2int_t pull_map[] = {
            { "none", ZJS_PULL_NONE },
            { "pullup", ZJS_PULL_UP },
            { "pulldown", ZJS_PULL_DOWN },
            { NULL, 0 }
        };
        ZJS_REQUIRE_STR_IF_PROP_MAP(init, "state", pull_map, 10, &pull);
    }

    int flags = 0;
    flags |= active_low ? GPIO_POL_INV : GPIO_POL_NORMAL;
    if (dir == ZJS_DIR_INPUT) {
        flags |= GPIO_DIR_IN | edge;
    } else {
        flags |= GPIO_DIR_OUT;
    }
    if (pull == ZJS_PULL_NONE) {
        flags |= GPIO_PUD_NORMAL;
    } else {
        flags |= (pull == ZJS_PULL_UP) ? GPIO_PUD_PULL_UP : GPIO_PUD_PULL_DOWN;
    }

    int rval = gpio_pin_configure(gpiodev, pin, flags);
    if (rval) {
        ERR_PRINT("GPIO: #%d (RVAL: %d)\n", (int)pin, rval);
        return zjs_error("pin configure failed");
    }

    // create the GPIOPin object
    ZVAL pin_obj = zjs_create_object();
    jerry_set_prototype(pin_obj, gpio_pin_prototype);

    gpio_handle_t *handle = zjs_malloc(sizeof(gpio_handle_t));
    if (!handle) {
        return zjs_error("out of memory");
    }
    memset(handle, 0, sizeof(gpio_handle_t));
    handle->pin = pin;
    handle->pin_obj = pin_obj;  // weak reference
    handle->port = gpiodev;
    handle->callbackId = -1;
    handle->active_low = active_low;

    // Set the native handle so we can free it when close() is called
    jerry_set_object_native_pointer(pin_obj, handle, &gpio_type_info);

    if (dir == ZJS_DIR_INPUT) {
        // Zephyr ISR callback init
        gpio_init_callback(&handle->callback, zjs_gpio_zephyr_callback,
                           BIT(pin));
        gpio_add_callback(gpiodev, &handle->callback);
        gpio_pin_enable_callback(gpiodev, pin);

        // Register a C callback (will be called after the ISR is called)
        handle->callbackId = zjs_add_c_callback(handle, zjs_gpio_c_callback);
        handle->edge_both = (edge == ZJS_EDGE_BOTH) ? 1 : 0;
    }

    return jerry_acquire_value(pin_obj);
}

static void zjs_gpio_cleanup(void *native)
{
    FTRACE("native = %p\n", native);
    zjs_gpio_mock_cleanup();
    jerry_release_value(gpio_pin_prototype);
    gpio_pin_prototype = 0;
}

static const jerry_object_native_info_t gpio_module_type_info = {
    .free_cb = zjs_gpio_cleanup
};

static jerry_value_t zjs_gpio_init()
{
    FTRACE("\n");
    zjs_gpio_mock_pre_init();

    // create GPIO pin prototype object
    zjs_native_func_t array[] = {
        { zjs_gpio_pin_read, "read" },
        { zjs_gpio_pin_write, "write" },
        { zjs_gpio_pin_close, "close" },
        { NULL, NULL }
    };
    gpio_pin_prototype = zjs_create_object();
    zjs_obj_add_functions(gpio_pin_prototype, array);

    // create GPIO object
    jerry_value_t gpio_api = zjs_create_object();
    zjs_obj_add_function(gpio_api, "open", zjs_gpio_open);

    // Set up cleanup function for when the object gets freed
    jerry_set_object_native_pointer(gpio_api, NULL, &gpio_module_type_info);

    zjs_gpio_mock_post_init(gpio_api);

    return gpio_api;
}

JERRYX_NATIVE_MODULE(gpio, zjs_gpio_init)
