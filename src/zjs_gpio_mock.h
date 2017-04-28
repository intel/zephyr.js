// Copyright (c) 2017, Intel Corporation.

#ifndef __zjs_gpio_mock_h__
#define __zjs_gpio_mock_h__

#if defined(ZJS_GPIO_MOCK) && !defined(__zjs_gpio_h__)
#error zjs_gpio.h must be included before zjs_gpio_mock.h!
#endif

#include "jerryscript.h"

#define DEVICE jerry_value_t

struct mock_gpio_callback;
typedef void (*mock_gpio_callback_handler_t)(DEVICE port,
                                             struct mock_gpio_callback *cb,
                                             uint32_t pins);
struct mock_gpio_callback {
    mock_gpio_callback_handler_t handler;
    uint32_t pin_mask;
};

// redefine calls to Zephyr APIs we're mocking
#define gpio_callback mock_gpio_callback
#define gpio_callback_handler_t mock_gpio_callback_handler_t

#define device_get_binding mock_gpio_device_get_binding
#define gpio_pin_read mock_gpio_pin_read
#define gpio_pin_write mock_gpio_pin_write
#define gpio_pin_configure mock_gpio_pin_configure
#define gpio_init_callback mock_gpio_init_callback
#define gpio_add_callback mock_gpio_add_callback
#define gpio_remove_callback mock_gpio_remove_callback
#define gpio_pin_enable_callback mock_gpio_pin_enable_callback

DEVICE mock_gpio_device_get_binding(const char *name);
int mock_gpio_pin_read(DEVICE port, uint32_t pin, uint32_t *value);
int mock_gpio_pin_write(DEVICE port, uint32_t pin, uint32_t value);
int mock_gpio_pin_configure(DEVICE port, uint8_t pin, int flags);
void mock_gpio_init_callback(struct gpio_callback *callback,
                             gpio_callback_handler_t handler,
                             uint32_t pin_mask);
int mock_gpio_add_callback(DEVICE port, struct gpio_callback *callback);
int mock_gpio_remove_callback(DEVICE port, struct gpio_callback *callback);
int mock_gpio_pin_enable_callback(DEVICE port, uint32_t pin);

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

// hook to initialize mock before the rest of gpio init
void zjs_gpio_mock_pre_init();

// hook to add mock functions into gpio object before it's returned to JS
void zjs_gpio_mock_post_init(jerry_value_t gpio_obj);

// hook to clean up mock after gpio cleanup
void zjs_gpio_mock_cleanup();

#endif  // __zjs_gpio_mock_h__
