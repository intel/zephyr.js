// Copyright (c) 2016, Intel Corporation.
#ifdef BUILD_MODULE_GPIO
// Zephyr includes
#include <zephyr.h>
#include <gpio.h>
#include <misc/util.h>
#include <string.h>
#include <stdlib.h>

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

static jerry_value_t zjs_gpio_pin_prototype;

void (*zjs_gpio_convert_pin)(uint32_t orig, int *dev, int *pin) =
    zjs_default_convert_pin;

// Handle for GPIO input pins, passed around between ISR/C callbacks
typedef struct gpio_handle {
    struct gpio_callback callback;  // Callback structure for zephyr
    uint32_t pin;                   // Pin associated with this handle
    struct device *port;            // Pin's port
    uint32_t value;                 // Value of the pin
    zjs_callback_id callbackId;             // ID for the C callback
    jerry_value_t pin_obj;          // Pin object returned from open()
    jerry_value_t onchange_func;    // Function registered to onChange
    jerry_value_t open_rval;
    uint32_t last;
    uint8_t edge_both;
    bool closed;
} gpio_handle_t;

static jerry_value_t lookup_pin(const jerry_value_t pin_obj,
                                struct device **port, int *pin)
{
    char pin_id[32];
    int newpin;
    struct device *gpiodev;

    if (zjs_obj_get_string(pin_obj, "pin", pin_id, sizeof(pin_id))) {
        // Pin ID can be a string of format "GPIODEV.num", where
        // GPIODEV is Zephyr's native device name for GPIO port,
        // this is usually GPIO_0, GPIO_1, etc., but some Zephyr
        // ports have completely different naming, so don't assume
        // anything! "num" is a numeric pin number within the port,
        // usually within range 0-31.
        char *pinstr = strrchr(pin_id, '.');
        bool error = true;
        if (pinstr && pinstr[1] != '\0') {
            *pinstr = '\0';
            newpin = strtol(pinstr + 1, &pinstr, 10);
            if (*pinstr == '\0')
                error = false;
        }
        if (error)
            return zjs_error("zjs_gpio_open: invalid pin id");
        gpiodev = device_get_binding(pin_id);
        if (!gpiodev)
            return zjs_error("zjs_gpio_open: cannot find GPIO device");
    } else {
        // .. Or alternative, pin ID can be a board-specific, encoded
        // number, which we decode using zjs_gpio_convert_pin.
        uint32_t pin;
        if (!zjs_obj_get_uint32(pin_obj, "pin", &pin))
            return zjs_error("zjs_gpio_open: missing required field");

        int devnum;
        zjs_gpio_convert_pin(pin, &devnum, &newpin);
        if (newpin == -1)
            return zjs_error("zjs_gpio_open: invalid pin");
        gpiodev = zjs_gpio_dev[devnum];
    }

    *port = gpiodev;
    *pin = newpin;
    // Means success
    return ZJS_UNDEFINED;
}

// C callback to be called after a GPIO input ISR fires
static void gpio_c_callback(void *h, void *args)
{
    gpio_handle_t *handle = (gpio_handle_t *)h;
    if (handle->closed) {
        ERR_PRINT("unexpected callback after close");
        return;
    }
    jerry_value_t onchange_func = zjs_get_property(handle->pin_obj, "onchange");

    // If pin.onChange exists, call it
    if (jerry_value_is_function(onchange_func)) {
        jerry_value_t event = jerry_create_object();
        uint32_t val = 0;
        memcpy(&val, args, 4);
        // Put the boolean GPIO trigger value in the object
        zjs_obj_add_boolean(event, val, "value");

        // Only acquire once, once we have it just keep using it.
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
    gpio_handle_t *handle = CONTAINER_OF(cb, gpio_handle_t, callback);
    // Read the value and save it in the handle
    gpio_pin_read(port, handle->pin, &handle->value);
    if ((handle->edge_both && handle->value != handle->last) ||
        !handle->edge_both) {
        // Signal the C callback, where we call the JS callback
        zjs_signal_callback(handle->callbackId, &handle->value, 4);
        handle->last = handle->value;
    }
}

static jerry_value_t zjs_gpio_pin_read(const jerry_value_t function_obj,
                                       const jerry_value_t this,
                                       const jerry_value_t argv[],
                                       const jerry_length_t argc)
{
    // requires: this is a GPIOPin object from zjs_gpio_open, takes no args
    //  effects: reads a logical value from the pin and returns it in ret_val_p
    uintptr_t ptr;
    if (jerry_get_object_native_handle(this, &ptr)) {
        gpio_handle_t *handle = (gpio_handle_t *)ptr;
        if (handle->closed) {
            return zjs_error("zjs_gpio_pin_read: pin closed");
        }
    }

    int newpin;
    struct device *gpiodev;
    jerry_value_t status = lookup_pin(this, &gpiodev, &newpin);
    if (!jerry_value_is_undefined(status))
        return status;
    jerry_release_value(status);

    bool activeLow = false;
    zjs_obj_get_boolean(this, "activeLow", &activeLow);

    uint32_t value;
    int rval = gpio_pin_read(gpiodev, newpin, &value);
    if (rval) {
        ERR_PRINT("PIN: #%d\n", newpin);
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
    uintptr_t ptr;
    if (jerry_get_object_native_handle(this, &ptr)) {
        gpio_handle_t *handle = (gpio_handle_t *)ptr;
        if (handle->closed) {
            return zjs_error("zjs_gpio_pin_write: pin closed");
        }
    }

    if (argc < 1 || !jerry_value_is_boolean(argv[0]))
        return zjs_error("zjs_gpio_pin_write: invalid argument");

    bool logical = jerry_get_boolean_value(argv[0]);

    int newpin;
    struct device *gpiodev;
    jerry_value_t status = lookup_pin(this, &gpiodev, &newpin);
    if (!jerry_value_is_undefined(status))
        return status;
    jerry_release_value(status);

    bool activeLow = false;
    zjs_obj_get_boolean(this, "activeLow", &activeLow);

    uint32_t value = 0;
    if ((logical && !activeLow) || (!logical && activeLow))
        value = 1;
    int rval = gpio_pin_write(gpiodev, newpin, value);
    if (rval) {
        ERR_PRINT("GPIO: #%d!n", newpin);
        return zjs_error("zjs_gpio_pin_write: error writing to GPIO");
    }

    return ZJS_UNDEFINED;
}

static void zjs_gpio_close(gpio_handle_t *handle)
{
        zjs_remove_callback(handle->callbackId);
        if (handle->onchange_func) {
            jerry_release_value(handle->onchange_func);
        }
        gpio_remove_callback(handle->port, &handle->callback);
        handle->closed = true;
}

static jerry_value_t zjs_gpio_pin_close(const jerry_value_t function_obj,
                                        const jerry_value_t this,
                                        const jerry_value_t argv[],
                                        const jerry_length_t argc)
{
    uintptr_t ptr;
    if (jerry_get_object_native_handle(this, &ptr)) {
        gpio_handle_t *handle = (gpio_handle_t *)ptr;
        if (handle->closed)
            return zjs_error("zjs_gpio_pin_close: already closed");

        zjs_gpio_close(handle);
        return ZJS_UNDEFINED;
    }

    return zjs_error("zjs_gpio_pin_close: no native handle");
}

static void zjs_gpio_free_cb(const uintptr_t native)
{
    gpio_handle_t *handle = (gpio_handle_t *)native;
    if (!handle->closed)
        zjs_gpio_close(handle);

    zjs_free(handle);
}

// Called after the promise is fulfilled/rejected
static void post_open_promise(void* h)
{
    gpio_handle_t *handle = (gpio_handle_t *)h;
    // Handle is the ret_args array that was malloc'ed in open()
    if (handle) {
        jerry_release_value(handle->pin_obj);
        jerry_release_value(handle->open_rval);
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

    struct device *gpiodev;
    int newpin;

    jerry_value_t status = lookup_pin(data, &gpiodev, &newpin);
    if (!jerry_value_is_undefined(status))
        return status;
    jerry_release_value(status);

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
            DBG_PRINT("warning: invalid edge value provided\n");
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

    int rval = gpio_pin_configure(gpiodev, newpin, flags);
    if (rval) {
        ERR_PRINT("GPIO: #%d (RVAL: %d)\n", newpin, rval);
        return zjs_error("zjs_gpio_open: error opening GPIO pin");
    }

    // create the GPIOPin object
    jerry_value_t pinobj = jerry_create_object();
    jerry_set_prototype(pinobj, zjs_gpio_pin_prototype);

    zjs_obj_add_string(pinobj, dirOut ? ZJS_DIR_OUT : ZJS_DIR_IN, "direction");
    zjs_obj_add_boolean(pinobj, activeLow, "activeLow");
    zjs_obj_add_string(pinobj, edge, "edge");
    zjs_obj_add_string(pinobj, pull, "pull");
    jerry_value_t pin = zjs_get_property(data, "pin");
    zjs_set_property(pinobj, "pin", pin);
    jerry_release_value(pin);

    gpio_handle_t *handle = zjs_malloc(sizeof(gpio_handle_t));
    memset(handle, 0, sizeof(gpio_handle_t));
    handle->pin = newpin;
    handle->pin_obj = async ? jerry_acquire_value(pinobj) : pinobj;
    handle->port = gpiodev;
    handle->callbackId = -1;

    // Set the native handle so we can free it when close() is called
    jerry_set_object_native_handle(pinobj, (uintptr_t)handle, zjs_gpio_free_cb);

    if (!dirOut) {
        // Zephyr ISR callback init
        gpio_init_callback(&handle->callback, gpio_zephyr_callback,
                           BIT(newpin));
        gpio_add_callback(gpiodev, &handle->callback);
        gpio_pin_enable_callback(gpiodev, newpin);

        // Register a C callback (will be called after the ISR is called)
        handle->callbackId = zjs_add_c_callback(handle, gpio_c_callback);

        if (!strcmp(edge, ZJS_EDGE_BOTH)) {
            handle->edge_both = 1;
        } else {
            handle->edge_both = 0;
        }
    }

    if (async) {
        // Promise obj returned by open(), will have then() and catch() funcs
        jerry_value_t promise_ret = jerry_create_object();

        // Turn object into a promise
        zjs_make_promise(promise_ret, post_open_promise, (void*)handle);

        // TODO: Can open promise be rejected? For now, rejection is based on if
        // gpiodev is not NULL
        if (gpiodev) {
            handle->open_rval = jerry_acquire_value(pinobj);
            // Fulfill the promise
            zjs_fulfill_promise(promise_ret, &handle->open_rval, 1);
        } else {
            handle->open_rval = zjs_error("GPIO could not be opened");
            zjs_reject_promise(promise_ret, &handle->open_rval, 1);
        }

        jerry_release_value(pinobj);
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
            ERR_PRINT("cannot find GPIO device '%s'\n", devname);
        }
    }

    // create GPIO pin prototype object
    zjs_native_func_t array[] = {
        { zjs_gpio_pin_read, "read" },
        { zjs_gpio_pin_write, "write" },
        { zjs_gpio_pin_close, "close" },
        { NULL, NULL }
    };
    zjs_gpio_pin_prototype = jerry_create_object();
    zjs_obj_add_functions(zjs_gpio_pin_prototype, array);

    // create GPIO object
    jerry_value_t gpio_obj = jerry_create_object();
    zjs_obj_add_function(gpio_obj, zjs_gpio_open_sync, "open");
    zjs_obj_add_function(gpio_obj, zjs_gpio_open_async, "openAsync");
    return gpio_obj;
}

void zjs_gpio_cleanup()
{
    jerry_release_value(zjs_gpio_pin_prototype);
}

#endif // BUILD_MODULE_GPIO
