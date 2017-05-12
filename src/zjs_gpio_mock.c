// Copyright (c) 2017, Intel Corporation.

#ifndef ZJS_LINUX_BUILD
#include "gpio.h"
#endif

#include "zjs_board.h"
#include "zjs_gpio.h"
#include "zjs_util.h"

// mock headers should be included last
#include "zjs_gpio_mock.h"

static const jerry_object_native_info_t mock_type_info =
{
   .free_cb = free_handle_nop
};


// specially declare this internal gpio function
jerry_value_t gpio_internal_lookup_pin(const jerry_value_t pin_obj,
                                       DEVICE *port, int *pin);

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
static jerry_value_t mock_root_obj = 0;

// mock helpers
static jerry_value_t get_pin(jerry_value_t port, unsigned int pin)
{
    char name[15];
    sprintf(name, "pin%u", pin);
    return zjs_get_property(port, name);
}

static jerry_value_t ensure_pin(jerry_value_t port, unsigned int pin)
{
    char name[15];
    sprintf(name, "pin%u", pin);
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
    gpio_internal_lookup_pin(argv[0], &port1, &pin1);
    gpio_internal_lookup_pin(argv[1], &port2, &pin2);

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
DEVICE mock_gpio_device_get_binding(const char *name)
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

int mock_gpio_pin_read(DEVICE port, uint32_t pin, uint32_t *value)
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

int mock_gpio_pin_write(DEVICE port, uint32_t pin, uint32_t value)
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
                        ZJS_GET_HANDLE(connection, mock_cb_item_t, item, mock_type_info);

                        if (BIT(conn_pin) & item->enabled_mask) {
                            item->handler(port, item->callback,
                                    item->enabled_mask);
                        }
                    }
                }
            }
        }
    }

    return 0;
}

int mock_gpio_pin_configure(DEVICE port, uint8_t pin, int flags)
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

void mock_gpio_init_callback(struct gpio_callback *callback,
                             gpio_callback_handler_t handler,
                             uint32_t pin_mask)
{
    mock_cb_item_t *item = (mock_cb_item_t *)malloc(sizeof(mock_cb_item_t));
    if (item) {
        item->next = mock_cb_list;
        item->callback = callback;
        item->handler = handler;
        item->pin_mask = pin_mask;
        item->enabled_mask = 0;
        item->port = 0;
        mock_cb_list = item;
    }
}

int mock_gpio_add_callback(DEVICE port, struct gpio_callback *callback)
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

int mock_gpio_remove_callback(DEVICE port, struct gpio_callback *callback)
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

int mock_gpio_pin_enable_callback(DEVICE port, uint32_t pin)
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
                jerry_set_object_native_pointer(pin_obj,
                                                item,
                                                &mock_type_info);
                // FIXME: maybe need to clean up on re-open w/o close
                break;
           }
        }
        item = item->next;
    }
    return 0;
}

void zjs_gpio_mock_pre_init()
{
    mock_root_obj = jerry_create_object();
}

void zjs_gpio_mock_post_init(jerry_value_t gpio_obj)
{
    // use the mock property to check for mock APIs
    zjs_obj_add_boolean(gpio_obj, true, "mock");
    zjs_obj_add_function(gpio_obj, zjs_gpio_mock_wire, "wire");
}

void zjs_gpio_mock_cleanup()
{
    jerry_release_value(mock_root_obj);

    mock_cb_item_t *item = mock_cb_list;
    while (item) {
        mock_cb_item_t *next = item->next;
        zjs_free(item);
        item = next;
    }
}
