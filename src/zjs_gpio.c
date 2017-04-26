// Copyright (c) 2016-2017, Intel Corporation.

// Zephyr includes
#ifndef ZJS_LINUX_BUILD
#include <zephyr.h>
#include <gpio.h>
#include <misc/util.h>
#endif
#include <string.h>
#include <stdlib.h>

// ZJS includes
#include "zjs_gpio.h"
#include "zjs_util.h"
#include "zjs_callbacks.h"

#if defined(ZJS_LINUX_BUILD) || defined(QEMU_BUILD)
#ifndef ZJS_GPIO_MOCK
#define ZJS_GPIO_MOCK
#endif
#endif

#ifndef ZJS_GPIO_MOCK
#define DEVICE struct device *
#else
#define DEVICE jerry_value_t

struct mock_gpio_callback;
typedef void (*mock_gpio_callback_handler_t)(DEVICE port,
                                             struct mock_gpio_callback *cb,
                                             uint32_t pins);
struct mock_gpio_callback {
    mock_gpio_callback_handler_t handler;
    uint32_t pin_mask;
};

#define gpio_callback mock_gpio_callback
#define gpio_callback_handler_t mock_gpio_callback_handler_t

#define device_get_binding mock_device_get_binding
#define gpio_pin_read mock_gpio_pin_read
#define gpio_pin_write mock_gpio_pin_write
#define gpio_pin_configure mock_gpio_pin_configure
#define gpio_init_callback mock_gpio_init_callback
#define gpio_add_callback mock_gpio_add_callback
#define gpio_remove_callback mock_gpio_remove_callback
#define gpio_pin_enable_callback mock_gpio_pin_enable_callback

static DEVICE mock_device_get_binding(const char *name);
static int mock_gpio_pin_read(DEVICE port, uint32_t pin, uint32_t *value);
static int mock_gpio_pin_write(DEVICE port, uint32_t pin, uint32_t value);
static int mock_gpio_pin_configure(DEVICE port, uint8_t pin, int flags);
static void mock_gpio_init_callback(struct gpio_callback *callback,
                                    gpio_callback_handler_t handler,
                                    uint32_t pin_mask);
static int mock_gpio_add_callback(DEVICE port, struct gpio_callback *callback);
static int mock_gpio_remove_callback(DEVICE port,
                                     struct gpio_callback *callback);
static int mock_gpio_pin_enable_callback(DEVICE port, uint32_t pin);

static jerry_value_t mock_root_obj = 0;

#ifdef ZJS_LINUX_BUILD
// provide Zephyr dependencies
#define GPIO_DIR_IN           (0 << 0)
#define GPIO_DIR_OUT          (1 << 0)
#define GPIO_INT              (1 << 1)
#define GPIO_INT_ACTIVE_LOW   (0 << 2)
#define GPIO_INT_ACTIVE_HIGH  (1 << 2)
#define GPIO_INT_EDGE         (1 << 5)
#define GPIO_INT_DOUBLE_EDGE  (1 << 6)
#define GPIO_POL_NORMAL       (0 << 7)
#define GPIO_POL_INV          (1 << 7)
#define GPIO_PUD_NORMAL       (0 << 8)
#define GPIO_PUD_PULL_UP      (1 << 8)
#define GPIO_PUD_PULL_DOWN    (2 << 8)

#define BIT(n)  (1UL << (n))
#define CONTAINER_OF(ptr, type, field) \
	((type *)(((char *)(ptr)) - offsetof(type, field)))
#endif  // ZJS_LINUX_BUILD
#endif  // ZJS_GPIO_MOCK

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

static DEVICE zjs_gpio_dev[GPIO_DEV_COUNT];

static jerry_value_t zjs_gpio_pin_prototype;

void (*zjs_gpio_convert_pin)(uint32_t orig, int *dev, int *pin) =
    zjs_default_convert_pin;

// Handle for GPIO input pins, passed around between ISR/C callbacks
typedef struct gpio_handle {
    struct gpio_callback callback;  // Callback structure for zephyr
    uint32_t pin;                   // Pin associated with this handle
    DEVICE port;                    // Pin's port
    uint32_t value;                 // Value of the pin
    zjs_callback_id callbackId;     // ID for the C callback
    jerry_value_t pin_obj;          // Pin object returned from open()
    jerry_value_t open_rval;
    uint32_t last;
    uint8_t edge_both;
    bool closed;
} gpio_handle_t;

static jerry_value_t lookup_pin(const jerry_value_t pin_obj, DEVICE *port,
                                int *pin)
{
    char pin_id[32];
    int newpin;
    DEVICE gpiodev;

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
static void zjs_gpio_zephyr_callback(DEVICE port, struct gpio_callback *cb,
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
    DEVICE gpiodev;
    ZVAL status = lookup_pin(this, &gpiodev, &newpin);
    if (!jerry_value_is_undefined(status))
        return status;

    bool activeLow = false;
    zjs_obj_get_boolean(this, "activeLow", &activeLow);

    uint32_t value = 0;
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
    DEVICE gpiodev;
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

    DEVICE gpiodev;
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

#ifdef ZJS_GPIO_MOCK
// Sample structure of mock root object:
// var obj = {
//     "GPIO_0": {
//         "pin1": {
//             "state": <boolean>,
//             "flags": <uint32>,
//             "wired": <array>  // pins this pin is wired to
//         },
//         "pin2": <object>
//     },
//     "GPIO_1": <object>,
//     "GPIO_2": <object>,
// }

// mock helpers
static jerry_value_t get_pin(jerry_value_t port, unsigned int pin)
{
    char name[15];
    sprintf(name, "pin%u\n", pin);
    return zjs_get_property(port, name);
}

static jerry_value_t ensure_pin(jerry_value_t port, unsigned int pin)
{
    char name[15];
    sprintf(name, "pin%u\n", pin);
    jerry_value_t pin_obj = zjs_get_property(port, name);
    if (!jerry_value_is_object(pin_obj)) {
        pin_obj = jerry_create_object();
        zjs_set_property(port, name, pin_obj);
    }
    return pin_obj;
}

struct value_match_data {
    jerry_value_t name;
    jerry_value_t value;
};

static bool value_match(const jerry_value_t name, const jerry_value_t value,
                        void *user_data)
{
    struct value_match_data *data = (struct value_match_data *)user_data;
    if (value == data->value) {
        // found match, stop processing and return name
        data->name = name;
        return false;
    }
    return true;
}

static jerry_value_t find_property_with_value(jerry_value_t obj,
                                              jerry_value_t value)
{
    // effects: returns name of first property in obj that is set to value, or
    //            else a 0 value
    struct value_match_data match;
    match.name = 0;
    match.value = value;
    jerry_foreach_object_property(obj, value_match, &match);
    return match.name;
}

// donated by jprestwo from his pending websockets patch, should move to util
static jerry_value_t push_array(jerry_value_t array, jerry_value_t val)
{
    jerry_value_t new;
    if (!jerry_value_is_array(array)) {
        new = jerry_create_array(1);
        jerry_set_property_by_index(new, 0, val);
    } else {
        uint32_t size = jerry_get_array_length(array);
        new = jerry_create_array(size + 1);
        for (int i = 0; i < size; i++) {
            ZVAL v = jerry_get_property_by_index(array, i);
            jerry_set_property_by_index(new, i, v);
        }
        jerry_set_property_by_index(new, size, val);
    }
    return new;
}

// mock control functions

/**
 * @name mock
 */

/**
 * Simulate a wire connecting the two given pins
 *
 * @name wire
 * @param {GPIOPin} pin1
 * @param {GPIOPin} pin2
 */
static ZJS_DECL_FUNC(zjs_gpio_mock_wire)
{
    ZJS_VALIDATE_ARGS(Z_OBJECT, Z_OBJECT);

    DEVICE port1;
    DEVICE port2;
    int pin1, pin2;
    lookup_pin(argv[0], &port1, &pin1);
    lookup_pin(argv[1], &port2, &pin2);

    jerry_value_t name1 = find_property_with_value(mock_root_obj, port1);
    jerry_value_t name2 = find_property_with_value(mock_root_obj, port2);
    if (!name1 || !name2) {
        return zjs_error("invalid port object");
    }

    ZVAL pin1_obj = get_pin(port1, pin1);
    ZVAL pin2_obj = get_pin(port2, pin2);
    if (!jerry_value_is_object(pin1_obj) ||
        !jerry_value_is_object(pin2_obj)) {
        return zjs_error("invalid pin object");
    }

    ZVAL pin1_wired = zjs_get_property(pin1_obj, "wired");
    ZVAL new1_wired = push_array(pin1_wired, pin2_obj);
    zjs_set_property(pin1_obj, "wired", new1_wired);

    ZVAL pin2_wired = zjs_get_property(pin2_obj, "wired");
    ZVAL new2_wired = push_array(pin2_wired, pin1_obj);
    zjs_set_property(pin2_obj, "wired", new2_wired);

    return ZJS_UNDEFINED;
}

// mocked apis
static DEVICE mock_device_get_binding(const char *name)
{
    ZVAL obj = zjs_get_property(mock_root_obj, name);
    jerry_value_t rval = obj;
    if (!jerry_value_is_object(obj)) {
        ZVAL new_obj = jerry_create_object();
        zjs_set_property(mock_root_obj, name, new_obj);
        rval = new_obj;
    }

    // DEVICE is jerry_value_t in mock mode
    return rval;
}

static int mock_gpio_pin_read(DEVICE port, uint32_t pin, uint32_t *value)
{
    if (!find_property_with_value(mock_root_obj, port)) {
        ERR_PRINT("invalid port object\n");
        return -1;
    }

    ZVAL pin_obj = get_pin(port, pin);
    if (!jerry_value_is_object(pin_obj)) {
        ERR_PRINT("invalid pin object\n");
        return -1;
    }

    // reading from a GPIO output seems to be allowed, so don't check direction

    // check for a 'state'
    bool flag;
    if (zjs_obj_get_boolean(pin_obj, "state", &flag)) {
        *value = flag ? 1 : 0;
        return 0;
    }

    // check for a pin this is wired to
    ZVAL wired = zjs_get_property(pin_obj, "wired");
    if (jerry_value_is_array(wired)) {
        uint32_t len = jerry_get_array_length(wired);
        for (uint32_t i = 0; i < len; ++i) {
            ZVAL pin = jerry_get_property_by_index(wired, i);
            if (zjs_obj_get_boolean(pin, "state", &flag)) {
                *value = flag ? 1 : 0;
                return 0;
            }
        }
    }

    // TODO: for advanced tests, might check against a timing pattern

    ERR_PRINT("no mock value\n");
    return -1;
}

// TODO: write generic list code
typedef struct mock_cb_item {
    struct mock_cb_item *next;
    struct gpio_callback *callback;
    gpio_callback_handler_t handler;
    uint32_t pin_mask;
    uint32_t enabled_mask;
    jerry_value_t port;
} mock_cb_item_t;

mock_cb_item_t *mock_cb_list = NULL;

static int mock_gpio_pin_write(DEVICE port, uint32_t pin, uint32_t value)
{
    if (!find_property_with_value(mock_root_obj, port)) {
        ERR_PRINT("invalid port object\n");
        return -1;
    }

    ZVAL pin_obj = get_pin(port, pin);
    if (!jerry_value_is_object(pin_obj)) {
        ERR_PRINT("invalid pin object\n");
        return -1;
    }

    uint32_t flags;
    if (!zjs_obj_get_uint32(pin_obj, "flags", &flags) ||
        !(flags & GPIO_DIR_OUT)) {
        // attempted write to an input; just do nothing
        return 0;
    }

    // set 'state'
    bool level = false;
    bool found = zjs_obj_get_boolean(pin_obj, "state", &level);
    bool new_level = value ? true : false;
    zjs_obj_add_boolean(pin_obj, new_level, "state");

    if (found && level != new_level) {
        // check for pins this is wired to
        ZVAL wired = zjs_get_property(pin_obj, "wired");
        if (jerry_value_is_array(wired)) {
            // trigger edges on change
            uint32_t len = jerry_get_array_length(wired);
            for (uint32_t i = 0; i < len; ++i) {
                ZVAL connection = jerry_get_property_by_index(wired, i);
                uint32_t conn_pin = 0;
                zjs_obj_get_uint32(connection, "pin", &conn_pin);

                if (zjs_obj_get_uint32(connection, "flags", &flags)) {
                    // make sure it has interrupt and edge-triggering enabled
                    uint32_t expect = GPIO_INT | GPIO_INT_EDGE;
                    if ((flags & expect) != expect) {
                        continue;
                    }

                    if ((flags & GPIO_INT_DOUBLE_EDGE) ||
                        ((flags & GPIO_INT_ACTIVE_HIGH) == GPIO_INT_ACTIVE_HIGH
                         && new_level) ||
                        ((flags & GPIO_INT_ACTIVE_LOW) == GPIO_INT_ACTIVE_LOW
                         && !new_level)) {

                        // simulate onchange interrupt
                        uintptr_t ptr;
                        if (jerry_get_object_native_handle(connection, &ptr)) {
                            mock_cb_item_t *item = (mock_cb_item_t *)ptr;
                            if (BIT(conn_pin) & item->enabled_mask) {
                                item->handler(port, item->callback,
                                              item->enabled_mask);
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}

static int mock_gpio_pin_configure(DEVICE port, uint8_t pin, int flags)
{
    if (!find_property_with_value(mock_root_obj, port)) {
        ERR_PRINT("invalid port object\n");
        return -1;
    }

    // create or update pin object
    ZVAL pin_obj = ensure_pin(port, pin);
    zjs_set_property(pin_obj, "flags", jerry_create_number(flags));
    zjs_set_property(pin_obj, "pin", jerry_create_number(pin));

    return 0;
}

static void mock_gpio_init_callback(struct gpio_callback *callback,
                                    gpio_callback_handler_t handler,
                                    uint32_t pin_mask)
{
    mock_cb_item_t *item = (mock_cb_item_t *)malloc(sizeof(mock_cb_item_t));
    item->next = mock_cb_list;
    item->callback = callback;
    item->handler = handler;
    item->pin_mask = pin_mask;
    item->enabled_mask = 0;
    item->port = 0;
    mock_cb_list = item;
}

static int mock_gpio_add_callback(DEVICE port, struct gpio_callback *callback)
{
    if (!find_property_with_value(mock_root_obj, port)) {
        ERR_PRINT("invalid port object\n");
        return -1;
    }

    mock_cb_item_t *item = mock_cb_list;
    while (item) {
        if (item->callback == callback) {
            item->port = port;
            break;
        }
        item = item->next;
    }
    return 0;
}

static int mock_gpio_remove_callback(DEVICE port,
                                     struct gpio_callback *callback)
{
    // disable the callback for this port
    if (!find_property_with_value(mock_root_obj, port)) {
        ERR_PRINT("invalid port object\n");
        return -1;
    }

    mock_cb_item_t *item = mock_cb_list;
    while (item) {
        if (item->callback == callback && item->port == port) {
            item->enabled_mask = 0;
            item = item->next;
            break;
        }
        item = item->next;
    }

    return 0;
}

static int mock_gpio_pin_enable_callback(DEVICE port, uint32_t pin)
{
    if (!find_property_with_value(mock_root_obj, port)) {
        ERR_PRINT("invalid port object\n");
        return -1;
    }

    ZVAL pin_obj = get_pin(port, pin);
    if (!jerry_value_is_object(pin_obj)) {
        ERR_PRINT("invalid pin object\n");
        return -1;
    }

    mock_cb_item_t *item = mock_cb_list;
    while (item) {
        if (item->port == port) {
            uint32_t bit = BIT(pin);
            if (bit & item->pin_mask) {
                item->enabled_mask |= bit;
                jerry_set_object_native_handle(pin_obj, (uintptr_t)item, NULL);
                // FIXME: maybe need to clean up on re-open w/o close
                break;
           }
        }
        item = item->next;
    }
    return 0;
}
#endif  // ZJS_GPIO_MOCK

jerry_value_t zjs_gpio_init()
{
    // effects: finds the GPIO driver and returns the GPIO JS object

#ifdef ZJS_GPIO_MOCK
    mock_root_obj = jerry_create_object();
#endif

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

#ifdef ZJS_GPIO_MOCK
    // use the mock property to check for mock APIs
    zjs_obj_add_boolean(gpio_obj, true, "mock");
    zjs_obj_add_function(gpio_obj, zjs_gpio_mock_wire, "wire");
#endif

    return gpio_obj;
}

void zjs_gpio_cleanup()
{
    jerry_release_value(zjs_gpio_pin_prototype);

#ifdef ZJS_GPIO_MOCK
    jerry_release_value(mock_root_obj);

    mock_cb_item_t *item = mock_cb_list;
    while (item) {
        mock_cb_item_t *next = item->next;
        zjs_free(item);
        item = next;
    }
#endif
}
