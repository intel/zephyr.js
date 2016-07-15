// Copyright (c) 2016, Intel Corporation.

// Zephyr includes
#include <zephyr.h>
#include <gpio.h>
#include <misc/util.h>
#include <string.h>

// ZJS includes
#include "zjs_gpio.h"
#include "zjs_util.h"

static const char *ZJS_DIR_IN = "in";
static const char *ZJS_DIR_OUT = "out";

static const char *ZJS_EDGE_NONE = "none";
static const char *ZJS_EDGE_RISING = "rising";
static const char *ZJS_EDGE_FALLING = "falling";
static const char *ZJS_EDGE_BOTH = "any";

static const char *ZJS_PULL_NONE = "none";
static const char *ZJS_PULL_UP = "up";
static const char *ZJS_PULL_DOWN = "down";

static const char *ZJS_CHANGE = "change";

static struct device *zjs_gpio_dev;

int (*zjs_gpio_convert_pin)(int num) = zjs_identity;

// This is complicated. One thing going on here is that the GPIO functions do
//   not let you set any "user data" to be returned to you later. So if you need
//   to associate data, you have to embed the gpio callback within a bigger
//   struct like this, and use CONTAINER_OF to get back to your struct.
// The reason for the *list* is just so we're able to find the allocated struct
//   again if it needs to be freed, which we're not really using yet, unless the
//   pin object gets GC'd which hasn't been tested.
// It looks like pin_obj isn't really needed at this time.
struct zjs_cb_list_item {
    struct gpio_callback gpio_cb;
    jerry_object_t *pin_obj;
    struct zjs_callback zjs_cb;
    struct zjs_cb_list_item *next;
};

static struct zjs_cb_list_item *zjs_cb_list = NULL;

static struct zjs_cb_list_item *zjs_gpio_callback_alloc()
{
    // effects: allocates a new callback list item and adds it to the list
    struct zjs_cb_list_item *item;
    item = task_malloc(sizeof(struct zjs_cb_list_item));
    if (!item) {
        PRINT("error: out of memory allocating callback struct\n");
        return NULL;
    }

    item->next = zjs_cb_list;
    zjs_cb_list = item;
    return item;
}

static struct zjs_cb_list_item *zjs_gpio_find(jerry_object_t *pin_obj)
{
    // effects: finds an existing list item for this pin
    struct zjs_cb_list_item *pItem = zjs_cb_list;
    while (pItem) {
        if (pItem->pin_obj == pin_obj)
            return pItem;
        pItem = pItem->next;
    }
    return NULL;
}

static void zjs_gpio_callback_free(uintptr_t handle)
{
    // requires: handle is the native pointer we registered with
    //             jerry_set_object_native_handle
    //  effects: frees the callback list item for the given pin object
    struct zjs_cb_list_item **pItem = &zjs_cb_list;
    while (*pItem) {
        struct zjs_cb_list_item *item = *pItem;
        if ((uintptr_t)item == handle) {
            uint32_t pin;
            zjs_obj_get_uint32(item->pin_obj, "pin", &pin);
            gpio_pin_disable_callback(zjs_gpio_dev, pin);
            gpio_remove_callback(zjs_gpio_dev, &item->gpio_cb);
            jerry_release_object(item->zjs_cb.js_callback);

            *pItem = item->next;
            task_free((void *)handle);
        }
        pItem = &(*pItem)->next;
    }
}

static void zjs_gpio_callback_wrapper(struct device *port,
                                      struct gpio_callback *cb,
                                      uint32_t pins)
{
    // effects: handles C callback; queues up the JS callback for execution
    struct zjs_cb_list_item *mycb = CONTAINER_OF(cb, struct zjs_cb_list_item,
                                                 gpio_cb);
    zjs_queue_callback(&mycb->zjs_cb);
}

static void zjs_gpio_call_function(struct zjs_callback *cb)
{
    // requires: called only from task context
    //  effects: handles execution of the JS callback when ready
    jerry_value_t rval = jerry_call_function(cb->js_callback, NULL, NULL, 0);
    if (jerry_value_is_error(rval)) {
        PRINT("error: calling gpio callback\n");
    }
    jerry_release_value(rval);
    // NOTE: this function is actually generic and could serve to call any
    //   JS callback function with no args, so we may move it to a generic
    //   name as we discover how often this comes up
}

static bool zjs_gpio_pin_read(const jerry_object_t *function_obj_p,
                              const jerry_value_t this_val,
                              const jerry_value_t args_p[],
                              const jerry_length_t args_cnt,
                              jerry_value_t *ret_val_p)
{
    // requires: this_val is a GPIOPin object from zjs_gpio_open, takes no args
    //  effects: reads a logical value from the pin and returns it in ret_val_p
    jerry_object_t *obj = jerry_get_object_value(this_val);

    uint32_t pin;
    zjs_obj_get_uint32(obj, "pin", &pin);
    int newpin = zjs_gpio_convert_pin(pin);

    bool activeLow = false;
    zjs_obj_get_boolean(obj, "activeLow", &activeLow);

    uint32_t value;
    int rval = gpio_pin_read(zjs_gpio_dev, newpin, &value);
    if (rval) {
        PRINT("error: reading from GPIO pin #%d!\n", newpin);
        return false;
    }

    bool logical = false;
    if ((value && !activeLow) || (!value && activeLow))
        logical = true;

    *ret_val_p = jerry_create_boolean_value(logical);

    return true;
}

static bool zjs_gpio_pin_write(const jerry_object_t *function_obj_p,
                               const jerry_value_t this_val,
                               const jerry_value_t args_p[],
                               const jerry_length_t args_cnt,
                               jerry_value_t *ret_val_p)
{
    // requires: this_val is a GPIOPin object from zjs_gpio_open, takes one arg,
    //             the logical boolean value to set to the pin (true = active)
    //  effects: writes the logical value to the pin
    if (args_cnt < 1 || !jerry_value_is_boolean(args_p[0])) {
        PRINT("zjs_gpio_pin_write: invalid argument\n");
        return false;
    }

    bool logical = jerry_get_boolean_value(args_p[0]);
    jerry_object_t *obj = jerry_get_object_value(this_val);

    uint32_t pin;
    zjs_obj_get_uint32(obj, "pin", &pin);
    int newpin = zjs_gpio_convert_pin(pin);

    bool activeLow = false;
    zjs_obj_get_boolean(obj, "activeLow", &activeLow);

    uint32_t value = 0;
    if ((logical && !activeLow) || (!logical && activeLow))
        value = 1;
    int rval = gpio_pin_write(zjs_gpio_dev, newpin, value);
    if (rval) {
        PRINT("error: writing to GPIO #%d!\n", newpin);
        return false;
    }

    return true;
}

static bool zjs_gpio_pin_on(const jerry_object_t *function_obj_p,
                            const jerry_value_t this_val,
                            const jerry_value_t args_p[],
                            const jerry_length_t args_cnt,
                            jerry_value_t *ret_val_p)
{
    // requires: this_val is a GPIOPin object, the one arg is a JS callback
    //             function
    //  effects: registers this callback to be called when the GPIO changes
    if (args_cnt < 2 || !jerry_value_is_string(args_p[0])) {
        PRINT("zjs_gpio_pin_on: invalid arguments\n");
        return false;
    }

    if (!zjs_strequal(jerry_get_string_value(args_p[0]), ZJS_CHANGE)) {
        PRINT("zjs_gpio_pin_on: unknown event\n");
        return false;
    }

    jerry_object_t *pinobj = jerry_get_object_value(this_val);
    uint32_t pin;
    zjs_obj_get_uint32(pinobj, "pin", &pin);

    jerry_object_t *func = NULL;
    if (jerry_value_is_object(args_p[1])) {
        func = jerry_get_object_value(args_p[1]);
        if (!jerry_is_function(func))
            func = NULL;
    }

    // first free existing callback: updating more efficient but more code
    struct zjs_cb_list_item *item = zjs_gpio_find(pinobj);
    if (!func) {
        // no callback now, so return
        if (item)
            // first, free item if present
            zjs_gpio_callback_free((uintptr_t)item);
        return true;
    }

    if (!item) {
        item = zjs_gpio_callback_alloc();
        gpio_init_callback(&item->gpio_cb, zjs_gpio_callback_wrapper, BIT(pin));
        item->pin_obj = pinobj;
        item->zjs_cb.call_function = zjs_gpio_call_function;

        // watch for the object getting garbage collected, and clean up
        jerry_set_object_native_handle(pinobj, (uintptr_t)item,
                                       zjs_gpio_callback_free);

        int rval = gpio_add_callback(zjs_gpio_dev, &item->gpio_cb);
        if (rval) {
            PRINT("error: cannot setup callback!\n");
            return false;
        }

        rval = gpio_pin_enable_callback(zjs_gpio_dev, pin);
        if (rval) {
            PRINT("error: cannot enable callback!\n");
            return false;
        }
    }

    if (!item)
        return false;

    item->zjs_cb.js_callback = jerry_get_object_value(args_p[1]);
    jerry_acquire_object(item->zjs_cb.js_callback);

    return true;
}

static bool zjs_gpio_open(const jerry_object_t *function_obj_p,
                          const jerry_value_t this_val,
                          const jerry_value_t args_p[],
                          const jerry_length_t args_cnt,
                          jerry_value_t *ret_val_p)
{
    // requires: arg 0 is an object with these members: pin (int), direction
    //             (defaults to "out"), activeLow (defaults to false),
    //             edge (defaults to "any"), pull (default to undefined)
    if (args_cnt < 1 || !jerry_value_is_object(args_p[0])) {
        PRINT("zjs_gpio_open: invalid argument\n");
        return false;
    }

    // data input object
    jerry_object_t *data = jerry_get_object_value(args_p[0]);

    uint32_t pin;
    if (!zjs_obj_get_uint32(data, "pin", &pin)) {
        PRINT("zjs_gpio_open: missing required field\n");
        return false;
    }

    int newpin = zjs_gpio_convert_pin(pin);
    if (newpin == -1) {
        PRINT("invalid pin\n");
        return false;
    }

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
        else if (strcmp(buffer, ZJS_EDGE_BOTH)) {
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

    int rval = gpio_pin_configure(zjs_gpio_dev, newpin, flags);
    if (rval) {
        PRINT("error: opening GPIO pin #%d! (%d)\n", newpin, rval);
        //        return false;
    }

    // create the GPIOPin object
    jerry_object_t *pinobj = jerry_create_object();
    zjs_obj_add_function(pinobj, zjs_gpio_pin_read, "read");
    zjs_obj_add_function(pinobj, zjs_gpio_pin_write, "write");
    zjs_obj_add_function(pinobj, zjs_gpio_pin_on, "on");
    zjs_obj_add_number(pinobj, pin, "pin");
    zjs_obj_add_string(pinobj, dirOut ? ZJS_DIR_OUT : ZJS_DIR_IN, "direction");
    zjs_obj_add_boolean(pinobj, activeLow, "activeLow");
    zjs_obj_add_string(pinobj, edge, "edge");
    zjs_obj_add_string(pinobj, pull, "pull");
    // TODO: When we implement close, we should release the reference on this

    *ret_val_p = jerry_create_object_value(pinobj);
    return true;
}

jerry_object_t *zjs_gpio_init()
{
    // effects: finds the GPIO driver and returns the GPIO JS object
    zjs_gpio_dev = device_get_binding("GPIO_0");
    if (!zjs_gpio_dev) {
        PRINT("Cannot find GPIO_0 device\n");
    }

    // create GPIO object
    jerry_object_t *gpio_obj = jerry_create_object();
    zjs_obj_add_function(gpio_obj, zjs_gpio_open, "open");
    return gpio_obj;
}
