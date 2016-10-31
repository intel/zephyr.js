// Copyright (c) 2016, Intel Corporation.
#ifdef BUILD_MODULE_GPIO
// Zephyr includes
#include <zephyr.h>
#include <gpio.h>
#include <misc/util.h>
#include <string.h>

// ZJS includes
#include "zjs_gpio.h"
#include "zjs_util.h"
#include "zjs_callbacks.h"
#include "zjs_promise.h"

static const char *ZJS_DIR_IN = "in";
static const char *ZJS_DIR_OUT = "out";

static const char *ZJS_EDGE_NONE = "none";
static const char *ZJS_EDGE_RISING = "rising";
static const char *ZJS_EDGE_FALLING = "falling";
static const char *ZJS_EDGE_BOTH = "any";

static const char *ZJS_PULL_NONE = "none";
static const char *ZJS_PULL_UP = "up";
static const char *ZJS_PULL_DOWN = "down";

#ifdef CONFIG_BOARD_FRDM_K64F
#define GPIO_DEV_COUNT 5
#else
#define GPIO_DEV_COUNT 1
#endif

static struct device *zjs_gpio_dev[GPIO_DEV_COUNT];

void (*zjs_gpio_convert_pin)(uint32_t orig, int *dev, int *pin) =
    zjs_default_convert_pin;

// Handle for GPIO input pins, passed around between ISR/C callbacks
struct gpio_handle {
    struct gpio_callback callback;  // Callback structure for zephyr
    uint32_t pin;                   // Pin associated with this handle
    uint32_t devnum;
    uint32_t value;                 // Value of the pin
    int32_t callbackId;             // ID for the C callback
    jerry_value_t pin_obj;          // Pin object returned from open()
    jerry_value_t onchange_func;    // Function registered to onChange
    jerry_value_t* open_ret_args;
};

// C callback to be called after a GPIO input ISR fires
static void gpio_c_callback(void* h)
{
    struct gpio_handle *handle = (struct gpio_handle*)h;
    jerry_value_t onchange_func = zjs_get_property(handle->pin_obj, "onchange");

    // If pin.onChange exists, call it
    if (jerry_value_is_function(onchange_func)) {
        jerry_value_t event = jerry_create_object();
        // Put the boolean GPIO trigger value in the object
        zjs_obj_add_boolean(event, handle->value, "value");

        // Only aquire once, once we have it just keep using it.
        // It will be released in close()
        if (!handle->onchange_func) {
            handle->onchange_func = jerry_acquire_value(onchange_func);
        }

        // Call the JS callback
        jerry_call_function(handle->onchange_func, ZJS_UNDEFINED, &event, 1);

        jerry_release_value(event);
    } else {
        DBG_PRINT(("onChange has not been registered\n"));
    }

    jerry_release_value(onchange_func);
}

// Callback when a GPIO input fires
static void gpio_zephyr_callback(struct device *port,
                                 struct gpio_callback *cb,
                                 uint32_t pins)
{
    // Get our handle for this pin
    struct gpio_handle *handle = CONTAINER_OF(cb, struct gpio_handle, callback);
    // Read the value and save it in the handle
    gpio_pin_read(port, handle->pin, &handle->value);
    // Signal the C callback, where we call the JS callback
    zjs_signal_callback(handle->callbackId);
}

static struct gpio_handle* new_gpio_handle(void)
{

    struct gpio_handle* handle = zjs_malloc(sizeof(struct gpio_handle));
    memset(handle, 0, sizeof(struct gpio_handle));
    return handle;
}

static jerry_value_t zjs_gpio_pin_read(const jerry_value_t function_obj,
                                       const jerry_value_t this,
                                       const jerry_value_t argv[],
                                       const jerry_length_t argc)
{
    // requires: this is a GPIOPin object from zjs_gpio_open, takes no args
    //  effects: reads a logical value from the pin and returns it in ret_val_p
    uint32_t pin;
    zjs_obj_get_uint32(this, "pin", &pin);
    int devnum, newpin;
    zjs_gpio_convert_pin(pin, &devnum, &newpin);

    bool activeLow = false;
    zjs_obj_get_boolean(this, "activeLow", &activeLow);

    uint32_t value;
    int rval = gpio_pin_read(zjs_gpio_dev[devnum], newpin, &value);
    if (rval) {
        PRINT("PIN: #%d\n", newpin);
        return zjs_error("zjs_gpio_pin_read: reading from GPIO");
    }

    bool logical = false;
    if ((value && !activeLow) || (!value && activeLow))
        logical = true;

    return jerry_create_boolean(logical);
}

static jerry_value_t zjs_gpio_pin_write(const jerry_value_t function_obj,
                                        const jerry_value_t this,
                                        const jerry_value_t argv[],
                                        const jerry_length_t argc)
{
    // requires: this is a GPIOPin object from zjs_gpio_open, takes one arg,
    //             the logical boolean value to set to the pin (true = active)
    //  effects: writes the logical value to the pin
    if (argc < 1 || !jerry_value_is_boolean(argv[0]))
        return zjs_error("zjs_gpio_pin_write: invalid argument");

    bool logical = jerry_get_boolean_value(argv[0]);

    uint32_t pin;
    zjs_obj_get_uint32(this, "pin", &pin);
    int devnum, newpin;
    zjs_gpio_convert_pin(pin, &devnum, &newpin);

    bool activeLow = false;
    zjs_obj_get_boolean(this, "activeLow", &activeLow);

    uint32_t value = 0;
    if ((logical && !activeLow) || (!logical && activeLow))
        value = 1;
    int rval = gpio_pin_write(zjs_gpio_dev[devnum], newpin, value);
    if (rval) {
        PRINT("GPIO: #%d!n", newpin);
        return zjs_error("zjs_gpio_pin_write: error writing to GPIO");
    }

    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_gpio_pin_close(const jerry_value_t function_obj,
                                        const jerry_value_t this,
                                        const jerry_value_t argv[],
                                        const jerry_length_t argc)
{
    uintptr_t ptr;
    if (jerry_get_object_native_handle(this, &ptr)) {
        struct gpio_handle* handle = (struct gpio_handle*)ptr;
        if (handle) {
            zjs_remove_callback(handle->callbackId);
            if (handle->onchange_func) {
                jerry_release_value(handle->onchange_func);
            }

            uint32_t pin;
            zjs_obj_get_uint32(this, "pin", &pin);
            int devnum, newpin;
            zjs_gpio_convert_pin(pin, &devnum, &newpin);

            gpio_remove_callback(zjs_gpio_dev[devnum], &handle->callback);
            zjs_free(handle);
        }
    }

    return ZJS_UNDEFINED;
}

static void zjs_gpio_free_cb(const uintptr_t native)
{
    struct gpio_handle* handle = (struct gpio_handle*)native;
    zjs_remove_callback(handle->callbackId);
    if (handle->onchange_func) {
        jerry_release_value(handle->onchange_func);
    }

    gpio_remove_callback(zjs_gpio_dev[handle->devnum], &handle->callback);
    zjs_free(handle);
}

// Called after the promise is fulfilled/rejected
static void post_open_promise(void* h)
{
    struct gpio_handle* handle = (struct gpio_handle*)h;
    // Handle is the ret_args array that was malloc'ed in open()
    if (handle) {
        jerry_release_value(handle->open_ret_args[0]);
        zjs_free(handle->open_ret_args);
    }
}

static jerry_value_t zjs_gpio_open(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc,
                                   bool async)
{
    // requires: arg 0 is an object with these members: pin (int), direction
    //             (defaults to "out"), activeLow (defaults to false),
    //             edge (defaults to "any"), pull (default to undefined)
    if (argc < 1 || !jerry_value_is_object(argv[0]))
        return zjs_error("zjs_gpio_open: invalid argument");

    // data input object
    jerry_value_t data = argv[0];

    uint32_t pin;
    if (!zjs_obj_get_uint32(data, "pin", &pin))
        return zjs_error("zjs_gpio_open: missing required field");

    int devnum, newpin;
    zjs_gpio_convert_pin(pin, &devnum, &newpin);
    if (newpin == -1)
        return zjs_error("zjs_gpio_open: invalid pin");

    int flags = 0;
    bool dirOut = true;

    const int BUFLEN = 10;
    char buffer[BUFLEN];
    if (zjs_obj_get_string(data, "direction", buffer, BUFLEN)) {
        if (!strcmp(buffer, ZJS_DIR_IN))
            dirOut = false;
    }
    flags |= dirOut ? GPIO_DIR_OUT : GPIO_DIR_IN;

    bool activeLow = false;
    zjs_obj_get_boolean(data, "activeLow", &activeLow);
    flags |= activeLow ? GPIO_POL_INV : GPIO_POL_NORMAL;

    const char *edge = ZJS_EDGE_NONE;
    bool both = false;
    if (zjs_obj_get_string(data, "edge", buffer, BUFLEN)) {
        if (!strcmp(buffer, ZJS_EDGE_BOTH)) {
            flags |= GPIO_INT | GPIO_INT_DOUBLE_EDGE;
            edge = ZJS_EDGE_BOTH;
            both = true;
        }
        else if (!strcmp(buffer, ZJS_EDGE_RISING)) {
            // Zephyr triggers on active edge, so we need to be "active high"
            flags |= GPIO_INT | GPIO_INT_EDGE | GPIO_INT_ACTIVE_HIGH;
            edge = ZJS_EDGE_RISING;
            both = false;
        }
        else if (!strcmp(buffer, ZJS_EDGE_FALLING)) {
            // Zephyr triggers on active edge, so we need to be "active low"
            flags |= GPIO_INT | GPIO_INT_EDGE | GPIO_INT_ACTIVE_LOW;
            edge = ZJS_EDGE_FALLING;
            both = false;
        }
        else if (!strcmp(buffer, ZJS_EDGE_NONE)) {
            PRINT("warning: invalid edge value provided\n");
        }
    }

    // NOTE: Soletta API doesn't seem to provide a way to use Zephyr's
    //   level triggering

    const char *pull = ZJS_PULL_NONE;
    if (zjs_obj_get_string(data, "pull", buffer, BUFLEN)) {
        if (!strcmp(buffer, ZJS_PULL_UP)) {
            pull = ZJS_PULL_UP;
            flags |= GPIO_PUD_PULL_UP;
        }
        else if (!strcmp(buffer, ZJS_PULL_DOWN)) {
            pull = ZJS_PULL_DOWN;
            flags |= GPIO_PUD_PULL_DOWN;
        }
        // else ZJS_ASSERT(!strcmp(buffer, ZJS_PULL_NONE))
    }
    if (pull == ZJS_PULL_NONE)
        flags |= GPIO_PUD_NORMAL;

    int rval = gpio_pin_configure(zjs_gpio_dev[devnum], newpin, flags);
    if (rval) {
        PRINT("GPIO: #%d (RVAL: %d)\n", newpin, rval);
        return zjs_error("zjs_gpio_open: error opening GPIO pin");
    }

    // create the GPIOPin object
    jerry_value_t pinobj = jerry_create_object();
    zjs_obj_add_function(pinobj, zjs_gpio_pin_read, "read");
    zjs_obj_add_function(pinobj, zjs_gpio_pin_write, "write");
    zjs_obj_add_function(pinobj, zjs_gpio_pin_close, "close");
    zjs_obj_add_number(pinobj, pin, "pin");
    zjs_obj_add_string(pinobj, dirOut ? ZJS_DIR_OUT : ZJS_DIR_IN, "direction");
    zjs_obj_add_boolean(pinobj, activeLow, "activeLow");
    zjs_obj_add_string(pinobj, edge, "edge");
    zjs_obj_add_string(pinobj, pull, "pull");
    // TODO: When we implement close, we should release the reference on this

    struct gpio_handle* handle = NULL;
    if (async || !dirOut)
        handle = new_gpio_handle();

    // Only need the handle if this pin is an input
    if (!dirOut) {
        // Zephyr ISR callback init
        gpio_init_callback(&handle->callback, gpio_zephyr_callback,
                           BIT(newpin));
        gpio_add_callback(zjs_gpio_dev[devnum], &handle->callback);
        gpio_pin_enable_callback(zjs_gpio_dev[devnum], newpin);

        handle->pin = newpin;
        handle->pin_obj = async ? jerry_acquire_value(pinobj) : pinobj;
        handle->devnum = devnum;

        // Register a C callback (will be called after the ISR is called)
        handle->callbackId = zjs_add_c_callback(handle, gpio_c_callback);
        // Set the native handle so we can free it when close() is called
        jerry_set_object_native_handle(pinobj, (uintptr_t)handle, zjs_gpio_free_cb);
    }

    if (async) {
        // Promise obj returned by open(), will have then() and catch() funcs
        jerry_value_t promise_ret = jerry_create_object();
        handle->open_ret_args = zjs_malloc(sizeof(jerry_value_t) * 1);

        // Turn object into a promise
        zjs_make_promise(promise_ret, post_open_promise, (void*)handle);

        // TODO: Can open promise be rejected? For now, rejection is based on if
        // zjs_gpio_dev is not NULL
        if (zjs_gpio_dev[devnum]) {
            handle->open_ret_args[0] = jerry_acquire_value(pinobj);
            // Fulfill the promise
            zjs_fulfill_promise(promise_ret, handle->open_ret_args, 1);
        } else {
            handle->open_ret_args[0] = jerry_acquire_value(zjs_error("GPIO could not be opened"));
            zjs_reject_promise(promise_ret, handle->open_ret_args, 1);
        }

        return promise_ret;
    }

    return pinobj;
}

static jerry_value_t zjs_gpio_open_sync(const jerry_value_t function_obj,
                                        const jerry_value_t this,
                                        const jerry_value_t argv[],
                                        const jerry_length_t argc)
{
    return zjs_gpio_open(function_obj, this, argv, argc, false);
}

static jerry_value_t zjs_gpio_open_async(const jerry_value_t function_obj,
                                         const jerry_value_t this,
                                         const jerry_value_t argv[],
                                         const jerry_length_t argc)
{
    return zjs_gpio_open(function_obj, this, argv, argc, true);
}

jerry_value_t zjs_gpio_init()
{
    // effects: finds the GPIO driver and returns the GPIO JS object
    char devname[10];

    for (int i = 0; i < GPIO_DEV_COUNT; i++) {
        snprintf(devname, 8, "GPIO_%d", i);
        zjs_gpio_dev[i] = device_get_binding(devname);
        if (!zjs_gpio_dev[i]) {
            PRINT("DEVICE: %s\n", devname);
            return zjs_error("zjs_gpio_init: cannot find GPIO device");
        }
    }

    // create GPIO object
    jerry_value_t gpio_obj = jerry_create_object();
    zjs_obj_add_function(gpio_obj, zjs_gpio_open_sync, "open");
    zjs_obj_add_function(gpio_obj, zjs_gpio_open_async, "openAsync");
    return gpio_obj;
}
#endif // BUILD_MODULE_GPIO
