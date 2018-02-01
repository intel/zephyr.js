// Copyright (c) 2017-2018, Intel Corporation.

// enable to use function tracing for debug purposes
#if 0
#define USE_FTRACE
static char FTRACE_PREFIX[] = "aio";
#endif

#ifdef BUILD_MODULE_AIO
#ifndef QEMU_BUILD
// C includes
#include <string.h>

// Zephyr includes
#include <adc.h>
#include <misc/util.h>

// ZJS includes
#include "zjs_board.h"
#include "zjs_callbacks.h"
#include "zjs_common.h"
#include "zjs_event.h"
#include "zjs_modules.h"
#include "zjs_util.h"

#define AIO_POLL_FREQUENCY 10  // default AIO poll frequency is 10Hz

const int MAX_TYPE_LEN = 20;

static jerry_value_t zjs_aio_prototype = 0;

typedef struct aio_handle {
    struct device *dev;
    jerry_value_t pin_obj;
    u16_t pin;
    u16_t last_value;
    struct aio_handle *next;
} aio_handle_t;

static aio_handle_t *opened_handles = NULL;

// get the aio handle or return a JS error
#define GET_AIO_HANDLE(obj, var)                                     \
aio_handle_t *var = (aio_handle_t *)zjs_event_get_user_handle(obj);  \
if (!var) { return zjs_error("no aio handle"); }

static void aio_free_cb(void *ptr)
{
    FTRACE("ptr = %p\n", ptr);

    // remove from the list of opened handles
    aio_handle_t *handle = (aio_handle_t *)ptr;
    ZJS_LIST_REMOVE(aio_handle_t, opened_handles, handle);
    zjs_free(handle);
}

static u16_t pin_read(struct device *dev, u8_t pin)
{
    FTRACE("dev = %p, pin = %d\n", dev, (s32_t)pin);
    u8_t seq_buffer[ADC_BUFFER_SIZE];

    struct adc_seq_entry entry = {
        .sampling_delay = 30,
        .channel_id = pin,
        .buffer = seq_buffer,
        .buffer_length = ADC_BUFFER_SIZE,
    };

    struct adc_seq_table entry_table = {
        .entries = &entry,
        .num_entries = 1,
    };

    if (adc_read(dev, &entry_table) != 0) {
        ERR_PRINT("couldn't read from pin %d\n", pin);
        return -1;
    }

    // read from buffer, not sure if byte order is important
    return seq_buffer[0] | seq_buffer[1] << 8;
}

static ZJS_DECL_FUNC_ARGS(aio_pin_read, bool async)
{
    FTRACE_JSAPI;
    GET_AIO_HANDLE(this, handle);

    u16_t value = pin_read(handle->dev, handle->pin);
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
    FTRACE_JSAPI;
    return aio_pin_read(function_obj, this, argv, argc, false);
}

// Asynchronous Operations
static ZJS_DECL_FUNC(zjs_aio_pin_read_async)
{
    FTRACE_JSAPI;
    // args: callback
    ZJS_VALIDATE_ARGS(Z_FUNCTION);

    return aio_pin_read(function_obj, this, argv, argc, true);
}

static ZJS_DECL_FUNC(zjs_aio_pin_close)
{
    FTRACE_JSAPI;
    GET_AIO_HANDLE(this, handle);
    aio_free_cb(handle);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_aio_open)
{
    FTRACE_JSAPI;
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
    int pin = zjs_board_find_aio(pin_val, devname, 20);
    if (pin == FIND_PIN_INVALID) {
        return TYPE_ERROR("bad pin argument");
    } else if (pin == FIND_DEVICE_FAILURE) {
        return zjs_error("device not found");
    } else if (pin < 0) {
        return zjs_error("pin not found");
    }
    struct device *aiodev = device_get_binding(devname);
    if (!aiodev) {
        return zjs_error("device not found");
    }

    // create the AIOPin object
    ZVAL pin_obj = zjs_create_object();
    jerry_set_prototype(pin_obj, zjs_aio_prototype);

    aio_handle_t *handle = zjs_malloc(sizeof(aio_handle_t));
    if (!handle) {
        return zjs_error("out of memory");
    }
    memset(handle, 0, sizeof(aio_handle_t));
    handle->dev = aiodev;
    handle->pin = pin;
    handle->pin_obj = pin_obj;  // weak reference

    // make it an emitter object
    zjs_make_emitter(pin_obj, zjs_aio_prototype, handle, aio_free_cb);

    // add to the list of opened handles
    ZJS_LIST_APPEND(aio_handle_t, opened_handles, handle);
    return jerry_acquire_value(pin_obj);
}

static s32_t aio_poll_routine(void *h)
{
    FTRACE("h = %p\n", h);
    static u32_t last_read_time = 0;
    u32_t uptime = k_uptime_get_32();
    if ((uptime - last_read_time) > ((u32_t)(CONFIG_SYS_CLOCK_TICKS_PER_SEC /
                                             AIO_POLL_FREQUENCY * 10))) {
        aio_handle_t *handle = opened_handles;
        while (handle) {
            // FIXME: We dont know when a object subscribes to onchange
            // events, need a way to intercept the .on()
            // currently, if the pin is opened, then it do a read
            // and signal the event, this could slow down the system
            // when there are multiple AIO pins opened.
            u16_t value = pin_read(handle->dev, handle->pin);
            if (value != handle->last_value) {
                handle->last_value = value;
                ZVAL val = jerry_create_number(value);
                zjs_defer_emit_event(handle->pin_obj, "change", &val,
                                     sizeof(val), zjs_copy_arg,
                                     zjs_release_args);
            }
            handle = handle->next;
        }
        last_read_time = uptime;
    }

    zjs_loop_unblock();
    return K_FOREVER;
}

static void zjs_aio_cleanup(void *native)
{
    FTRACE("native = %p\n", native);
    while (opened_handles) {
        zjs_free(opened_handles);
        opened_handles = opened_handles->next;
    }
    jerry_release_value(zjs_aio_prototype);
    zjs_unregister_service_routine(aio_poll_routine);
}

static const jerry_object_native_info_t aio_module_type_info = {
    .free_cb = zjs_aio_cleanup
};

jerry_value_t zjs_aio_init()
{
    FTRACE("");
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
