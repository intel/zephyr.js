#include "zjs_event.h"
#include "zjs_callbacks.h"

#define ZJS_MAX_EVENT_NAME_SIZE     24

struct event {
    int32_t event_id;
    jerry_value_t map;
};

struct event_trigger {
    jerry_value_t* argv;
    uint32_t argc;
    void* handle;
    zjs_post_event post;
};

jerry_value_t* pre_event(void* h, uint32_t* args_cnt)
{
    struct event_trigger* trigger = (struct event_trigger*)h;
    if (trigger) {
        *args_cnt = trigger->argc;
        return trigger->argv;
    }
    return NULL;
}

void post_event(void* h, jerry_value_t* ret_val)
{
    struct event_trigger* trigger = (struct event_trigger*)h;
    if (trigger) {
        if (trigger->post) {
            trigger->post(trigger->handle);
        }
        if (trigger->argv) {
            zjs_free(trigger->argv);
        }
    }
}

static jerry_value_t add_listener(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    struct event* ev;

    if (!jerry_get_object_native_handle(this, (uintptr_t*)&ev)) {
        DBG_PRINT("[event] add_listener(): Error, native handle not found\n");
        return ZJS_UNDEFINED;
    }
    if (!jerry_value_is_string(argv[0])) {
        DBG_PRINT("[event] add_listener(): Error, first parameter must be event string\n");
        return ZJS_UNDEFINED;
    }
    if (!jerry_value_is_function(argv[1])) {
        DBG_PRINT("[event] add_listener(): Error, second parameter must be a listener function\n");
        return ZJS_UNDEFINED;
    }

    int sz = jerry_get_string_size(argv[0]);
    if (sz > ZJS_MAX_EVENT_NAME_SIZE) {
        PRINT("[event] add_listener(): Error, event name is too long\n");
        return ZJS_UNDEFINED;
    }
    char name[sz];
    int len = jerry_string_to_char_buffer(argv[0], (jerry_char_t *)name, sz);
    if (len != sz) {
        DBG_PRINT("[event] add_listener(): Error, size mismatch\n");
        return ZJS_UNDEFINED;
    }
    name[len] = '\0';

    // Event object to hold callback ID and eventually listener arguments
    jerry_value_t event_obj = zjs_get_property(ev->map, name);
    if (!jerry_value_is_object(event_obj)) {
        event_obj = jerry_create_object();
    }

    int32_t callback_id = zjs_add_callback(argv[1], ev, pre_event, post_event);

    // Add callback ID to event object
    zjs_obj_add_number(event_obj, callback_id, "callback_id");

    // Add event object to master event listener
    zjs_set_property(ev->map, name, event_obj);

    DBG_PRINT("[event] add_listener(): Added event listener: %s, callback ID = %u\n",
            name, callback_id);

    return ZJS_UNDEFINED;
}

static jerry_value_t emit_event(const jerry_value_t function_obj,
                                const jerry_value_t this,
                                const jerry_value_t argv[],
                                const jerry_length_t argc)
{
    if (!jerry_value_is_string(argv[0])) {
        DBG_PRINT("[event] emit_event(): Error, parameter is not a string\n");
        return ZJS_UNDEFINED;
    }
    int sz = jerry_get_string_size(argv[0]);
    char event[sz];
    int len = jerry_string_to_char_buffer(argv[0], (jerry_char_t *)event, sz);
    if (len != sz) {
        DBG_PRINT("[event] emit_event(): Error, size mismatch\n");
        return ZJS_UNDEFINED;
    }
    event[len] = '\0';

    return jerry_create_boolean(zjs_trigger_event(this,
                                                  event,
                                                  (jerry_value_t*)argv + 1,
                                                  argc - 1,
                                                  NULL,
                                                  NULL));
}

bool zjs_trigger_event(jerry_value_t obj,
                       const char* event,
                       jerry_value_t argv[],
                       uint32_t argc,
                       zjs_post_event post,
                       void* h)
{
    struct event* ev;
    struct event_trigger* trigger = zjs_malloc(sizeof(struct event_trigger));
    if (!trigger) {
        DBG_PRINT("[event] zjs_trigger_event(): Could not allocate trigger, out of memory\n");
        return false;
    }
    int32_t callback_id = -1;
    jerry_value_t event_obj;

    if (!jerry_get_object_native_handle(obj, (uintptr_t*)&ev)) {
        DBG_PRINT("[event] zjs_add_event_listener(): Error, native handle not found\n");
        return jerry_create_boolean(false);
    }

    int i;
    trigger->argv = zjs_malloc(sizeof(jerry_value_t) * argc);
    if (!trigger->argv) {
        DBG_PRINT("[event] zjs_trigger_event(): Could not allocate trigger args, out of memory\n");
        return false;
    }
    for (i = 0; i < argc; ++i) {
        trigger->argv[i] = argv[i];
    }
    trigger->argc = argc;

    event_obj = zjs_get_property(ev->map, event);
    if (!jerry_value_is_object(event_obj)) {
        DBG_PRINT("[event] zjs_trigger_event(): Event object not found\n");
        return false;
    }

    zjs_obj_get_uint32(event_obj, "callback_id", &callback_id);
    if (callback_id == -1) {
        DBG_PRINT("[event] zjs_trigger_event(): Error, callback_id not found\n");
        return false;
    }

    trigger->handle = h;
    trigger->post = post;

    zjs_edit_callback_handle(callback_id, trigger);

    zjs_signal_callback(callback_id);

    DBG_PRINT("[event] zjs_trigger_event(): Triggering event '%s', args_cnt=%u, callback_id=%u\n",
            event, trigger->argc, callback_id);

    return jerry_create_boolean(true);
}

void zjs_make_event(jerry_value_t obj)
{
    struct event* ev = zjs_malloc(sizeof(struct event));
    if (!ev) {
        DBG_PRINT("[event] zjs_make_event(): Could not allocate event handle, out of memory\n");
        return;
    }

    zjs_obj_add_function(obj, add_listener, "on");
    zjs_obj_add_function(obj, add_listener, "addListener");
    zjs_obj_add_function(obj, emit_event, "emit");

    jerry_set_object_native_handle(obj, (uintptr_t)ev, NULL);

    ev->map = jerry_create_object();
}

void zjs_add_event_listener(jerry_value_t obj, const char* event, jerry_value_t listener)
{
    struct event* ev;

    if (!jerry_get_object_native_handle(obj, (uintptr_t*)&ev)) {
        DBG_PRINT("[event] zjs_add_event_listener(): Error, native handle not found\n");
        return;
    }

    // Event object to hold callback ID and eventually listener arguments
    jerry_value_t event_obj = zjs_get_property(ev->map, event);
    if (!jerry_value_is_object(event_obj)) {
        event_obj = jerry_create_object();
    }

    int32_t callback_id = zjs_add_callback(listener, ev, pre_event, post_event);

    // Add callback ID to event object
    zjs_obj_add_number(event_obj, callback_id, "callback_id");
    // Add event object to master event listener
    zjs_set_property(ev->map, event, event_obj);

    DBG_PRINT("[event] zjs_add_event_listener(): Added listener, callback id = %u\n",
            callback_id);
}


static jerry_value_t event_constructor(const jerry_value_t function_obj,
                                       const jerry_value_t this,
                                       const jerry_value_t argv[],
                                       const jerry_length_t argc)
{
    jerry_value_t new_emitter = jerry_create_object();
    zjs_make_event(new_emitter);
    return new_emitter;
}

jerry_value_t zjs_event_init()
{
    return jerry_create_external_function(event_constructor);
}

