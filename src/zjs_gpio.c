// Copyright (c) 2016-2017, Intel Corporation.

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

// C callback from task context in response to GPIO input interrupt
static void zjs_gpio_c_callback(void *h, const void *args)
{
    gpio_handle_t *handle = (gpio_handle_t *)h;
    if (handle->closed) {
        ERR_PRINT("unexpected callback after close");
        return;
    }
    ZVAL onchange_func =
        zjs_get_property(handle->pin_obj, "onchange");

    // If pin.onChange exists, call it
    if (jerry_value_is_function(onchange_func)) {
        ZVAL event = jerry_create_object();
        uint32_t *the_args = (uint32_t *)args;
        // Put the boolean GPIO trigger value in the object
        zjs_obj_add_boolean(event, the_args[0], "value");
        zjs_obj_add_number(event, the_args[1], "pins");
        // TODO: This "pins" value is pretty useless to the JS script as is,
        //   because it is a bitmask of activated zephyr pins; need to map this
        //   back to JS pin values somehow. So leaving undocumented for now.
        //   This is more complex on k64f because of five GPIO ports.

        // Call the JS callback
        jerry_call_function(onchange_func, ZJS_UNDEFINED, &event, 1);
    } else {
        DBG_PRINT(("onChange has not been registered\n"));
    }
}

// Callback when a GPIO input fires
// INTERRUPT SAFE FUNCTION: No JerryScript VM, allocs, or release prints!
static void zjs_gpio_zephyr_callback(struct device *port,
                                     struct gpio_callback *cb,
                                     uint32_t pins)
{
    // Get our handle for this pin
    gpio_handle_t *handle = CONTAINER_OF(cb, gpio_handle_t, callback);
    // Read the value and save it in the handle
    gpio_pin_read(port, handle->pin, &handle->value);
    if ((handle->edge_both && handle->value != handle->last) ||
        !handle->edge_both) {
        uint32_t args[] = {handle->value, pins};
        // Signal the C callback, where we call the JS callback
        zjs_signal_callback(handle->callbackId, args, sizeof(args));
        handle->last = handle->value;
    }
}

static ZJS_DECL_FUNC(zjs_gpio_pin_read)
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
    ZVAL status = lookup_pin(this, &gpiodev, &newpin);
    if (!jerry_value_is_undefined(status))
        return status;

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

static ZJS_DECL_FUNC(zjs_gpio_pin_write)
{
    // requires: this is a GPIOPin object from zjs_gpio_open, takes one arg,
    //             the logical boolean value to set to the pin (true = active)
    //  effects: writes the logical value to the pin

    // args: pin value
    ZJS_VALIDATE_ARGS(Z_BOOL);

    uintptr_t ptr;
    if (jerry_get_object_native_handle(this, &ptr)) {
        gpio_handle_t *handle = (gpio_handle_t *)ptr;
        if (handle->closed) {
            return zjs_error("zjs_gpio_pin_write: pin closed");
        }
    }

    bool logical = jerry_get_boolean_value(argv[0]);

    int newpin;
    struct device *gpiodev;
    ZVAL status = lookup_pin(this, &gpiodev, &newpin);
    if (!jerry_value_is_undefined(status))
        return status;

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
        gpio_remove_callback(handle->port, &handle->callback);
        handle->closed = true;
}

static ZJS_DECL_FUNC(zjs_gpio_pin_close)
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

static ZJS_DECL_FUNC(zjs_gpio_open)
{
    // requires: arg 0 is an object with these members: pin (int), direction
    //             (defaults to "out"), activeLow (defaults to false),
    //             edge (defaults to "any"), pull (default to undefined)

    // args: initialization object
    ZJS_VALIDATE_ARGS(Z_OBJECT);

    // data input object
    jerry_value_t data = argv[0];

    struct device *gpiodev;
    int newpin;

    ZVAL status = lookup_pin(data, &gpiodev, &newpin);
    if (!jerry_value_is_undefined(status))
        return status;

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
            flags |= GPIO_INT | GPIO_INT_EDGE | GPIO_INT_DOUBLE_EDGE;
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
    ZVAL pinobj = jerry_create_object();
    jerry_set_prototype(pinobj, zjs_gpio_pin_prototype);

    zjs_obj_add_string(pinobj, dirOut ? ZJS_DIR_OUT : ZJS_DIR_IN, "direction");
    zjs_obj_add_boolean(pinobj, activeLow, "activeLow");
    zjs_obj_add_string(pinobj, edge, "edge");
    zjs_obj_add_string(pinobj, pull, "pull");
    ZVAL pin = zjs_get_property(data, "pin");
    zjs_set_property(pinobj, "pin", pin);

    gpio_handle_t *handle = zjs_malloc(sizeof(gpio_handle_t));
    memset(handle, 0, sizeof(gpio_handle_t));
    handle->pin = newpin;
    handle->pin_obj = pinobj;
    handle->port = gpiodev;
    handle->callbackId = -1;

    // Set the native handle so we can free it when close() is called
    jerry_set_object_native_handle(pinobj, (uintptr_t)handle, zjs_gpio_free_cb);

    if (!dirOut) {
        // Zephyr ISR callback init
        gpio_init_callback(&handle->callback, zjs_gpio_zephyr_callback,
                           BIT(newpin));
        gpio_add_callback(gpiodev, &handle->callback);
        gpio_pin_enable_callback(gpiodev, newpin);

        // Register a C callback (will be called after the ISR is called)
        handle->callbackId = zjs_add_c_callback(handle, zjs_gpio_c_callback);

        if (!strcmp(edge, ZJS_EDGE_BOTH)) {
            handle->edge_both = 1;
        } else {
            handle->edge_both = 0;
        }
    }

    return jerry_acquire_value(pinobj);
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
    zjs_obj_add_function(gpio_obj, zjs_gpio_open, "open");
    return gpio_obj;
}

void zjs_gpio_cleanup()
{
    jerry_release_value(zjs_gpio_pin_prototype);
}
