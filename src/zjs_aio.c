// Copyright (c) 2017, Intel Corporation.

#ifdef BUILD_MODULE_AIO
#ifndef QEMU_BUILD
// C includes
#include <string.h>

// Zephyr includes
#include <adc.h>
#include <misc/util.h>

// ZJS includes
#include "zjs_callbacks.h"
#include "zjs_event.h"
#include "zjs_common.h"
#include "zjs_modules.h"
#include "zjs_util.h"

#define AIO_POLL_FREQUENCY 10  // default AIO poll frequency is 10Hz

const int MAX_TYPE_LEN = 20;

static struct device *adc_device[2] = { NULL, NULL }; // ADC_0 and ADC_1
static u8_t pin_enabled[AIO_LEN] = {};
static u32_t pin_values[AIO_LEN] = {};
static u32_t pin_last_values[AIO_LEN] = {};
static u8_t seq_buffer[ADC_BUFFER_SIZE];
static u32_t last_read_time = 0;

static jerry_value_t zjs_aio_prototype;

typedef struct aio_handle {
    jerry_value_t pin_obj;
    u32_t pin;
    struct aio_handle *next;
} aio_handle_t;

static aio_handle_t *opened_handles = NULL;

// get the aio handle or return a JS error
#define GET_AIO_HANDLE(obj, var)                                     \
aio_handle_t *var = (aio_handle_t *)zjs_event_get_user_handle(obj);  \
if (!var) { return zjs_error("no aio handle"); }

static u8_t map_device(u8_t pin)
{
    if (pin == 12 || pin == 13) {
        return 0;
    } else if (pin == 14 || pin == 15) {
        return 1;
    }

    ERR_PRINT("invalid pin %d\n", pin);
    return -1;
}

static const char* map_device_name(u8_t pin)
{
    if (pin == 12 || pin == 13) {
        return CONFIG_ADC_0_NAME;
    } else if (pin == 14 || pin == 15) {
        return CONFIG_ADC_1_NAME;
    }

    ERR_PRINT("invalid pin %d\n", pin);
    return NULL;
}

static aio_handle_t *aio_alloc_handle()
{
    size_t size = sizeof(aio_handle_t);
    aio_handle_t *handle = zjs_malloc(size);
    if (handle) {
        memset(handle, 0, size);
    }
    return handle;
}

static void aio_free_cb(void *ptr)
{
    aio_handle_t *handle = (aio_handle_t *)ptr;
    u32_t pin = handle->pin;
    pin_enabled[pin - AIO_MIN]--;
    DBG_PRINT("AIO pin %d refcount %d\n", pin, pin_enabled[pin - AIO_MIN]);

    struct device *dev = adc_device[map_device(handle->pin)];
    if (dev) {
        if (((pin == 12 || pin == 13) &&
             (pin_enabled[0] == 0 && pin_enabled[1] == 0)) ||
            ((pin == 14 || pin == 15) &&
             (pin_enabled[2] == 0 && pin_enabled[3] == 0))) {
            // if no more A0 and A1 pins opened, disable ADC_0
            // if no more A2 and A3 pins opened, disable ADC_1
            DBG_PRINT("disabling ADC device\n");
            adc_disable(dev);
            adc_device[map_device(handle->pin)] = NULL;
        }
    }

    jerry_release_value(handle->pin_obj);

    // remove from the list of opened handles
    ZJS_LIST_REMOVE(aio_handle_t, opened_handles, handle);
    zjs_free(handle);
}

static s32_t pin_read(u8_t pin)
{
    struct adc_seq_entry entry = {
        .sampling_delay = 30 ,
        .channel_id = pin,
        .buffer = seq_buffer,
        .buffer_length = ADC_BUFFER_SIZE,
    };

    struct adc_seq_table entry_table = {
        .entries = &entry,
        .num_entries = 1,
    };

    struct device *dev = adc_device[map_device(pin)];
    if (!dev) {
        ERR_PRINT("ADC device not found with pin %d\n", pin);
        return -1;
    }

    if (adc_read(dev, &entry_table) != 0) {
        ERR_PRINT("couldn't read from pin %d\n", pin);
        return -1;
    }

    // read from buffer, not sure if byte order is important
    u32_t raw_value = (u32_t)seq_buffer[0] | (u32_t)seq_buffer[1] << 8;

    return raw_value;
}

static jerry_value_t aio_pin_read(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc,
                                  bool async)
{
    u32_t pin;
    zjs_obj_get_uint32(this, "pin", &pin);

    if (pin < AIO_MIN || pin > AIO_MAX) {
        DBG_PRINT("PIN: #%u\n", pin);
        return zjs_error("pin out of range");
    }

    s32_t value = pin_read(pin);
    if (value < 0) {
        return ZJS_ERROR("AIO read failed");
    }

    jerry_value_t result = jerry_create_number(value);

    if (async) {
#ifdef ZJS_FIND_FUNC_NAME
        zjs_obj_add_string(argv[0], ZJS_HIDDEN_PROP("function_name"),
                           "readAsync");
#endif
        zjs_callback_id id = zjs_add_callback_once(argv[0], this, NULL, NULL);
        zjs_signal_callback(id, &result, sizeof(result));
        return ZJS_UNDEFINED;
    } else {
        return result;
    }
}

static ZJS_DECL_FUNC(zjs_aio_pin_read)
{
    return aio_pin_read(function_obj,
                        this,
                        argv,
                        argc,
                        false);
}

// Asynchronous Operations
static ZJS_DECL_FUNC(zjs_aio_pin_read_async)
{
    // args: callback
    ZJS_VALIDATE_ARGS(Z_FUNCTION);

    return aio_pin_read(function_obj,
                        this,
                        argv,
                        argc,
                        true);
}

static ZJS_DECL_FUNC(zjs_aio_pin_close)
{
    u32_t pin;
    zjs_obj_get_uint32(this, "pin", &pin);
    GET_AIO_HANDLE(this, handle);

    aio_free_cb(handle);

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_aio_open)
{
    // args: initialization object
    ZJS_VALIDATE_ARGS(Z_OBJECT);

    jerry_value_t data = argv[0];

    u32_t pin;
    if (!zjs_obj_get_uint32(data, "pin", &pin))
        return zjs_error("missing required field (pin)");

    if (pin < AIO_MIN || pin > AIO_MAX) {
        DBG_PRINT("PIN: #%u\n", pin);
        return zjs_error("pin out of range");
    }

    // create the AIOPin object
    jerry_value_t pinobj = zjs_create_object();
    jerry_set_prototype(pinobj, zjs_aio_prototype);
    zjs_obj_add_number(pinobj, "pin", pin);

    aio_handle_t *handle = aio_alloc_handle();
    if (!handle) {
        return zjs_error("could not allocate handle");
    }

    // lookup what device the pins are connected to
    // A0 and A1 are connected to ADC_0, A2 and A3 are connected to ADC_1
    struct device *dev = adc_device[map_device(pin)];
    if (!dev) {
        const char *dev_name = map_device_name(pin);
        if (dev_name) {
            dev = device_get_binding(dev_name);
            if (!dev) {
                zjs_free(handle);
                return zjs_error("failed to initialize AIO device");
            }
            adc_enable(dev);
            adc_device[map_device(pin)] = dev;
        } else {
            zjs_free(handle);
            return zjs_error("failed to find ADC device");
        }
    }

    pin_enabled[pin - AIO_MIN]++;
    DBG_PRINT("AIO pin %d refcount %d\n", pin, pin_enabled[pin-12]);

    handle->pin_obj = jerry_acquire_value(pinobj);
    handle->pin = pin;

    // make it an emitter object
    zjs_make_emitter(pinobj, zjs_aio_prototype, handle, aio_free_cb);

    // add to the list of opened handles
    ZJS_LIST_APPEND(aio_handle_t, opened_handles, handle);
    return pinobj;
}

static s32_t aio_poll_routine(void *h)
{
    u32_t uptime = k_uptime_get_32();
    if ((uptime - last_read_time) > ((u32_t)(CONFIG_SYS_CLOCK_TICKS_PER_SEC /
                                             AIO_POLL_FREQUENCY * 10))) {
        for (int i = 0; i < AIO_LEN; i++) {
            if (pin_enabled[i]) {
                // FIXME: We dont know when a object subscribes to onchange
                // events, need a way to intercept the .on()
                // currently, if the pin is opened, then it do a read
                // and signal the event, this could slow down the system
                // when there are multiple AIO pins opened.
                pin_values[i] = pin_read(i + AIO_MIN);
                last_read_time = uptime;
                if (pin_values[i] != pin_last_values[i]) {
                    // send updates only if value has changed
                    aio_handle_t *h = ZJS_LIST_FIND(aio_handle_t,
                                                    opened_handles, pin,
                                                    i + AIO_MIN);
                    if (h) {
                        ZVAL val = jerry_create_number(pin_values[i]);
                        zjs_defer_emit_event(h->pin_obj, "change",
                                             &val,
                                             sizeof(val),
                                             zjs_copy_arg,
                                             zjs_release_args);
                    }
                    pin_last_values[i] = pin_values[i];
                }
            }
        }
    }

    zjs_loop_unblock();
    return K_FOREVER;
}

static void zjs_aio_cleanup(void *native)
{
    jerry_release_value(zjs_aio_prototype);
    zjs_unregister_service_routine(aio_poll_routine);
}

static const jerry_object_native_info_t aio_module_type_info = {
   .free_cb = zjs_aio_cleanup
};

jerry_value_t zjs_aio_init()
{
    // FIXME: need a way to unregister service routines
    zjs_register_service_routine(NULL, aio_poll_routine);

    zjs_native_func_t array[] = {
        { zjs_aio_pin_read, "read" },
        { zjs_aio_pin_read_async, "readAsync" },
        { zjs_aio_pin_close, "close" },
        { NULL, NULL }
    };

    zjs_aio_prototype = zjs_create_object();
    zjs_obj_add_functions(zjs_aio_prototype, array);

    // create global AIO object
    jerry_value_t aio_obj = zjs_create_object();
    zjs_obj_add_function(aio_obj, "open", zjs_aio_open);
    // Set up cleanup function for when the object gets freed
    jerry_set_object_native_pointer(aio_obj, NULL, &aio_module_type_info);
    return aio_obj;
}

JERRYX_NATIVE_MODULE(aio, zjs_aio_init)
#endif  // QEMU_BUILD
#endif  // BUILD_MODULE_AIO
