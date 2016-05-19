// Copyright (c) 2016, Intel Corporation.

// Zephyr includes
#include <zephyr.h>
#include <gpio.h>
#include <misc/util.h>
#include <string.h>

#if defined(CONFIG_STDOUT_CONSOLE)
#include <stdio.h>
#define PRINT           printf
#else
#include <misc/printk.h>
#define PRINT           printk
#endif

// ZJS includes
#include "zjs_gpio.h"
#include "zjs_util.h"

#if defined(CONFIG_GPIO_DW_0)
#define GPIO_DRV_NAME CONFIG_GPIO_DW_0_NAME
#elif defined(CONFIG_GPIO_QMSI_0)
#define GPIO_DRV_NAME CONFIG_GPIO_QMSI_0_NAME
#elif defined(CONFIG_GPIO_ATMEL_SAM3)
#define GPIO_DRV_NAME CONFIG_GPIO_ATMEL_SAM3_PORTB_DEV_NAME
#else
#error "Unsupported GPIO driver"
#endif

static const char *ZJS_DIR_IN = "in";
static const char *ZJS_DIR_OUT = "out";

static const char *ZJS_EDGE_NONE = "none";
static const char *ZJS_EDGE_RISING = "rising";
static const char *ZJS_EDGE_FALLING = "falling";
static const char *ZJS_EDGE_BOTH = "any";

static const char *ZJS_PULL_NONE = "none";
static const char *ZJS_PULL_UP = "up";
static const char *ZJS_PULL_DOWN = "down";

static struct device *zjs_gpio_dev;

struct zjs_cb_list_item {
    struct gpio_callback orig;
    jerry_api_object_t *pin_obj;
    jerry_api_object_t *js_callback;
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

static void zjs_gpio_callback_free(uintptr_t handle)
{
    // requires: handle is the native pointer we registered with
    //             jerry_api_set_object_native_handle
    //  effects: frees the callback list item for the given pin object
    struct zjs_cb_list_item **pItem = &zjs_cb_list;
    while (*pItem) {
        if ((uintptr_t)*pItem == handle) {
            *pItem = (*pItem)->next;
            task_free((void *)handle);
        }
        pItem = &(*pItem)->next;
    }
}

static void zjs_gpio_callback_wrapper(struct device *port,
                                      struct gpio_callback *cb,
                                      uint32_t pins)
{
    // effects: calls the JS callback registered in the struct
    struct zjs_cb_list_item *mycb = CONTAINER_OF(cb, struct zjs_cb_list_item,
                                                 orig);
    if (!mycb->js_callback) {
        PRINT("zjs_gpio_callback_wrapper: No callback registered!\n");
        return;
    }

    jerry_api_value_t rval;
    jerry_api_call_function(mycb->js_callback, NULL, &rval, NULL, 0);
}

jerry_api_object_t *zjs_gpio_init()
{
    // effects: finds the GPIO driver and registers the GPIO JS object
    zjs_gpio_dev = device_get_binding(GPIO_DRV_NAME);
    if (!zjs_gpio_dev) {
        PRINT("Cannot find %s!\n", GPIO_DRV_NAME);
    }

    // create global GPIO object
    jerry_api_object_t *gpio_obj = jerry_api_create_object();
    zjs_obj_add_function(gpio_obj, zjs_gpio_open, "open");
    zjs_obj_add_function(gpio_obj, zjs_gpio_set_callback, "set_callback");
    return gpio_obj;
}

bool zjs_gpio_open(const jerry_api_object_t *function_obj_p,
                   const jerry_api_value_t *this_p,
                   jerry_api_value_t *ret_val_p,
                   const jerry_api_value_t args_p[],
                   const jerry_api_length_t args_cnt)
{
    // requires: arg 0 is an object with these members: pin (int), direction
    //             (defaults to "out"), activeLow (defaults to false),
    //             edge (defaults to "any"), poll (default to 0, no polling),
    //             pull (default to undefined), raw (defaults to false)
    if (args_cnt < 1 ||
        args_p[0].type != JERRY_API_DATA_TYPE_OBJECT)
    {
        PRINT("zjs_gpio_open: invalid arguments\n");
        return false;
    }

    // data input object
    jerry_api_object_t *data = args_p[0].u.v_object;

    uint32_t pin;
    if (!zjs_obj_get_uint32(data, "pin", &pin)) {
        PRINT("zjs_gpio_open: missing required field\n");
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

    PRINT("DBG: Configuring pin #%lu with flags %x\n", pin, flags);
    int rval = gpio_pin_configure(zjs_gpio_dev, pin, flags);
    if (rval) {
        PRINT("error: opening GPIO pin #%lu! (%d)\n", pin, rval);
        //        return false;
    }

    // create the GPIOPin object
    jerry_api_object_t *pinobj = jerry_api_create_object();
    zjs_obj_add_function(pinobj, zjs_gpio_pin_read, "read");
    zjs_obj_add_function(pinobj, zjs_gpio_pin_write, "write");
    zjs_obj_add_function(pinobj, zjs_gpio_set_callback, "set_callback");
    zjs_obj_add_uint32(pinobj, pin, "pin");
    zjs_obj_add_string(pinobj, dirOut ? ZJS_DIR_OUT : ZJS_DIR_IN, "direction");
    zjs_obj_add_boolean(pinobj, activeLow, "activeLow");
    zjs_obj_add_string(pinobj, edge, "edge");
    zjs_obj_add_string(pinobj, pull, "pull");

    *ret_val_p = jerry_api_create_object_value(pinobj);
    return true;
}

bool zjs_gpio_pin_read(const jerry_api_object_t *function_obj_p,
                       const jerry_api_value_t *this_p,
                       jerry_api_value_t *ret_val_p,
                       const jerry_api_value_t args_p[],
                       const jerry_api_length_t args_cnt)
{
    // requires: this_p is a GPIOPin object from zjs_gpio_open, takes no args
    //  effects: reads a logical value from the pin and returns it in ret_val_p
    jerry_api_object_t *obj = jerry_api_get_object_value(this_p);

    uint32_t pin;
    zjs_obj_get_uint32(obj, "pin", &pin);
    bool activeLow = false;
    zjs_obj_get_boolean(obj, "activeLow", &activeLow);

    uint32_t value;
    int rval = gpio_pin_read(zjs_gpio_dev, pin, &value);
    if (rval) {
        PRINT("error: reading from GPIO pin #%lu!\n", pin);
        return false;
    }

    bool logical = false;
    if ((value && !activeLow) || (!value && activeLow))
        logical = true;
    ret_val_p->type = JERRY_API_DATA_TYPE_BOOLEAN;
    ret_val_p->u.v_bool = logical;
    PRINT("DBG: Reading pin #%lu: %s (%lu)\n", pin,
          logical ? "ACTIVE":"INACTIVE", value);

    return true;
}

bool zjs_gpio_pin_write(const jerry_api_object_t *function_obj_p,
                        const jerry_api_value_t *this_p,
                        jerry_api_value_t *ret_val_p,
                        const jerry_api_value_t args_p[],
                        const jerry_api_length_t args_cnt)
{
    // requires: this_p is a GPIOPin object from zjs_gpio_open, takes one arg,
    //             the logical boolean value to set to the pin (true = active)
    //  effects: writes the logical value to the pin
    if (args_cnt < 1 ||
        args_p[0].type != JERRY_API_DATA_TYPE_BOOLEAN)
    {
        PRINT("zjs_gpio_pin_write: invalid arguments\n");
        return false;
    }

    bool logical = args_p[0].u.v_bool;

    jerry_api_object_t *obj = jerry_api_get_object_value(this_p);

    uint32_t pin;
    zjs_obj_get_uint32(obj, "pin", &pin);
    bool activeLow = false;
    zjs_obj_get_boolean(obj, "activeLow", &activeLow);

    uint32_t value = 0;
    if ((logical && !activeLow) || (!logical && activeLow))
        value = 1;
    PRINT("DBG: Setting pin #%lu to %s (%lu)\n", pin,
          logical ? "ACTIVE":"INACTIVE", value);
    int rval = gpio_pin_write(zjs_gpio_dev, pin, value);
    if (rval) {
        PRINT("error: writing to GPIO #%lu!\n", pin);
        return false;
    }

    return true;
}

bool zjs_gpio_set_callback(const jerry_api_object_t *function_obj_p,
                           const jerry_api_value_t *this_p,
                           jerry_api_value_t *ret_val_p,
                           const jerry_api_value_t args_p[],
                           const jerry_api_length_t args_cnt)
{
    // requires: first arg is pin number, second is a JS callback function
    //  effects: registers this callback to be called when the GPIO changes
    if (args_cnt < 2 ||
        args_p[0].type != JERRY_API_DATA_TYPE_FLOAT32 ||
        args_p[1].type != JERRY_API_DATA_TYPE_OBJECT)
    {
        PRINT("zjs_gpio_set_callback: invalid arguments\n");
        return false;
    }

    jerry_api_object_t *pinobj = jerry_api_get_object_value(this_p);

    int pin = (int)args_p[0].u.v_float32;
    struct zjs_cb_list_item *item = zjs_gpio_callback_alloc();

    if (!item)
        return false;
    gpio_init_callback(&item->orig, zjs_gpio_callback_wrapper, BIT(pin));
    item->pin_obj = pinobj;
    item->js_callback = args_p[1].u.v_object;

    // watch for the object getting garbage collected, and clean up
    jerry_api_set_object_native_handle(pinobj, (uintptr_t)item,
                                       zjs_gpio_callback_free);

	int rval = gpio_add_callback(zjs_gpio_dev, &item->orig);
	if (rval) {
		PRINT("error: cannot setup callback!\n");
        return false;
	}

	rval = gpio_pin_enable_callback(zjs_gpio_dev, pin);
	if (rval) {
		PRINT("error: cannot enable callback!\n");
        return false;
	}

    return true;
}
