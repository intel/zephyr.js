// Copyright (c) 2016, Intel Corporation.

#include "zjs_event.h"
#include "zjs_callbacks.h"

#define ZJS_MAX_EVENT_NAME_SIZE     24
#define DEFAULT_MAX_LISTENERS       10
#ifdef DEBUG_BUILD
#define HIDDEN_PROP(n) n
#else
#define HIDDEN_PROP(n) "\377" n
#endif

static jerry_value_t zjs_event_emitter_prototype;

typedef struct event {
    int num_events;
    jerry_value_t map;
    int max_listeners;
} event_t;

typedef struct event_trigger {
    void* handle;
    zjs_post_event post;
} event_trigger_t;

typedef struct event_names {
    jerry_value_t name_array;
    int idx;
} event_names_t;

void post_event(void* h, jerry_value_t* ret_val)
{
    event_trigger_t *trigger = (event_trigger_t *)h;
    if (trigger) {
        if (trigger->post) {
            trigger->post(trigger->handle);
        }
        zjs_free(trigger);
    }
}

void zjs_add_event_listener(jerry_value_t obj, const char* event,
                            jerry_value_t listener)
{
    event_t *ev;

    jerry_value_t event_emitter = zjs_get_property(obj, HIDDEN_PROP("event"));
    if (!jerry_get_object_native_handle(event_emitter, (uintptr_t*)&ev)) {
        jerry_release_value(event_emitter);
        ERR_PRINT("native handle not found\n");
        return;
    }
    jerry_release_value(event_emitter);

    if (ev->num_events >= ev->max_listeners) {
        ERR_PRINT("max listeners reached\n");
        return;
    }

    // Event object to hold callback ID and eventually listener arguments
    jerry_value_t event_obj = zjs_get_property(ev->map, event);
    if (!jerry_value_is_object(event_obj)) {
        jerry_release_value(event_obj);
        event_obj = jerry_create_object();
    }

    int32_t callback_id = -1;
    jerry_value_t id_prop = zjs_get_property(event_obj, "callback_id");
    if (jerry_value_is_number(id_prop)) {
        // If there already is an event object, get the callback ID
        zjs_obj_get_int32(event_obj, "callback_id", &callback_id);
    }
    jerry_release_value(id_prop);
    callback_id = zjs_add_callback_list(listener, obj, ev, post_event,
                                        callback_id);

    // Add callback ID to event object
    zjs_obj_add_number(event_obj, callback_id, "callback_id");
    // Add event object to master event listener
    zjs_set_property(ev->map, event, event_obj);
    jerry_release_value(event_obj);

    DBG_PRINT("added listener, callback id = %ld\n", callback_id);

    ev->num_events++;
}

static jerry_value_t add_listener(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    jerry_value_t rval = jerry_acquire_value(this);
    if (!jerry_value_is_string(argv[0]) ||
        !jerry_value_is_function(argv[1])) {
        ERR_PRINT("invalid argument");
        return rval;
    }

    jerry_size_t size = ZJS_MAX_EVENT_NAME_SIZE;
    char name[size];
    zjs_copy_jstring(argv[0], name, &size);
    if (!size) {
        ERR_PRINT("event name is too long\n");
        return rval;
    }

    zjs_add_event_listener(this, name, argv[1]);
    return rval;
}

static jerry_value_t emit_event(const jerry_value_t function_obj,
                                const jerry_value_t this,
                                const jerry_value_t *argv,
                                const jerry_length_t argc)
{
    if (!jerry_value_is_string(argv[0])) {
        ERR_PRINT("parameter is not a string\n");
        return jerry_create_boolean(false);
    }

    jerry_size_t size = ZJS_MAX_EVENT_NAME_SIZE;
    char event[size];
    zjs_copy_jstring(argv[0], event, &size);
    if (!size) {
        ERR_PRINT("event name is too long\n");
        // FIXME: This function is supposed to return a bool; false probably
        //   makes sense here because there are no listeners for that too-long
        //   event name. Otherwise, at least return an error, not this?
        return jerry_acquire_value(this);
    }

    // FIXME: This is supposed to return true if there were listeners; false,
    //   otherwise. Check that it's behaving correctly. Doesn't look like it.
    return jerry_create_boolean(zjs_trigger_event(this, event, argv + 1,
                                                  argc - 1, NULL, NULL));
}

static jerry_value_t remove_listener(const jerry_value_t function_obj,
                                     const jerry_value_t this,
                                     const jerry_value_t argv[],
                                     const jerry_length_t argc)
{
    event_t *ev;

    jerry_value_t event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));
    if (!jerry_get_object_native_handle(event_emitter, (uintptr_t*)&ev)) {
        jerry_release_value(event_emitter);
        ERR_PRINT("native handle not found\n");
        return jerry_acquire_value(this);
    }
    jerry_release_value(event_emitter);

    if (!jerry_value_is_string(argv[0])) {
        ERR_PRINT("event name must be first parameter\n");
        return jerry_acquire_value(this);
    }
    if (!jerry_value_is_function(argv[1])) {
        ERR_PRINT("event listener must be second parameter\n");
        return jerry_acquire_value(this);
    }

    jerry_size_t size = ZJS_MAX_EVENT_NAME_SIZE;
    char event[size];
    zjs_copy_jstring(argv[0], event, &size);
    if (!size) {
        ERR_PRINT("event name is too long\n");
        return jerry_acquire_value(this);
    }

    // Event object to hold callback ID and eventually listener arguments
    jerry_value_t event_obj = zjs_get_property(ev->map, event);
    if (!jerry_value_is_object(event_obj)) {
        jerry_release_value(event_obj);
        DBG_PRINT("event object not found for '%s'\n", event);
        return jerry_acquire_value(this);
    }

    int32_t callback_id = -1;
    jerry_value_t id_prop = zjs_get_property(event_obj, "callback_id");
    if (jerry_value_is_number(id_prop)) {
        // If there already is an event object, get the callback ID
        zjs_obj_get_int32(event_obj, "callback_id", &callback_id);

        zjs_remove_callback_list_func(callback_id, argv[1]);
    } else {
        ERR_PRINT("callback_id not found for '%s'\n", event);
    }
    jerry_release_value(id_prop);
    jerry_release_value(event_obj);

    return jerry_acquire_value(this);
}

static jerry_value_t remove_all_listeners(const jerry_value_t function_obj,
                                          const jerry_value_t this,
                                          const jerry_value_t argv[],
                                          const jerry_length_t argc)
{
    event_t *ev;

    jerry_value_t event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));
    if (!jerry_get_object_native_handle(event_emitter, (uintptr_t*)&ev)) {
        jerry_release_value(event_emitter);
        ERR_PRINT("native handle not found\n");
        return jerry_acquire_value(this);
    }
    jerry_release_value(event_emitter);

    if (!jerry_value_is_string(argv[0])) {
        ERR_PRINT("event name must be first parameter\n");
        return jerry_acquire_value(this);
    }

    jerry_size_t size = ZJS_MAX_EVENT_NAME_SIZE;
    char event[size];
    zjs_copy_jstring(argv[0], event, &size);
    if (!size) {
        ERR_PRINT("event name is too long\n");
        return jerry_acquire_value(this);
    }

    // Event object to hold callback ID and eventually listener arguments
    jerry_value_t event_obj = zjs_get_property(ev->map, event);
    if (!jerry_value_is_object(event_obj)) {
        jerry_release_value(event_obj);
        DBG_PRINT("event object not found for '%s'\n", event);
        return jerry_acquire_value(this);
    }

    int32_t callback_id = -1;
    jerry_value_t id_prop = zjs_get_property(event_obj, "callback_id");
    if (jerry_value_is_number(id_prop)) {
        // If there already is an event object, get the callback ID
        zjs_obj_get_int32(event_obj, "callback_id", &callback_id);

        zjs_remove_callback(callback_id);

        jerry_value_t name = jerry_create_string((const jerry_char_t*)event);
        jerry_delete_property(ev->map, (const jerry_value_t)name);
        jerry_release_value(name);
    } else {
        ERR_PRINT("callback_id not found for '%s'\n", event);
    }

    jerry_release_value(id_prop);
    jerry_release_value(event_obj);
    return jerry_acquire_value(this);
}

bool foreach_event_name(const jerry_value_t prop_name,
                        const jerry_value_t prop_value,
                        void *data)
{
    event_names_t *names = (event_names_t *)data;

    jerry_set_property_by_index(names->name_array,
                                names->idx++,
                                prop_name);
    return true;
}

static jerry_value_t get_event_names(const jerry_value_t function_obj,
                                     const jerry_value_t this,
                                     const jerry_value_t argv[],
                                     const jerry_length_t argc)
{
    event_t *ev;
    event_names_t names;

    jerry_value_t event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));
    if (!jerry_get_object_native_handle(event_emitter, (uintptr_t*)&ev)) {
        jerry_release_value(event_emitter);
        ERR_PRINT("native handle not found\n");
        return ZJS_UNDEFINED;
    }
    jerry_release_value(event_emitter);

    names.idx = 0;
    names.name_array = jerry_create_array(ev->num_events);

    jerry_foreach_object_property(ev->map, foreach_event_name, &names);

    return names.name_array;
}

static jerry_value_t get_max_listeners(const jerry_value_t function_obj,
                                       const jerry_value_t this,
                                       const jerry_value_t argv[],
                                       const jerry_length_t argc)
{
    event_t *ev;

    jerry_value_t event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));
    if (!jerry_get_object_native_handle(event_emitter, (uintptr_t*)&ev)) {
        jerry_release_value(event_emitter);
        ERR_PRINT("native handle not found\n");
        return jerry_create_number(DEFAULT_MAX_LISTENERS);
    }
    jerry_release_value(event_emitter);
    return jerry_create_number(ev->max_listeners);
}

static jerry_value_t set_max_listeners(const jerry_value_t function_obj,
                                       const jerry_value_t this,
                                       const jerry_value_t argv[],
                                       const jerry_length_t argc)
{
    event_t *ev;

    jerry_value_t event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));
    if (!jerry_get_object_native_handle(event_emitter, (uintptr_t*)&ev)) {
        jerry_release_value(event_emitter);
        ERR_PRINT("native handle not found\n");
        return jerry_acquire_value(this);
    }
    jerry_release_value(event_emitter);

    if (!jerry_value_is_number(argv[0])) {
        ERR_PRINT("max listeners count must be first parameter\n");
        return jerry_acquire_value(this);
    }
    ev->max_listeners = (uint32_t)jerry_get_number_value(argv[0]);

    return jerry_acquire_value(this);
}

static jerry_value_t get_listener_count(const jerry_value_t function_obj,
                                        const jerry_value_t this,
                                        const jerry_value_t argv[],
                                        const jerry_length_t argc)
{
    event_t *ev;

    jerry_value_t event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));
    if (!jerry_get_object_native_handle(event_emitter, (uintptr_t*)&ev)) {
        jerry_release_value(event_emitter);
        ERR_PRINT("native handle not found\n");
        return jerry_create_number(0);
    }
    jerry_release_value(event_emitter);

    if (!jerry_value_is_string(argv[0])) {
        ERR_PRINT("event name must be first parameter\n");
        return jerry_create_number(0);
    }

    jerry_size_t size = ZJS_MAX_EVENT_NAME_SIZE;
    char event[size];
    zjs_copy_jstring(argv[0], event, &size);
    if (!size) {
        ERR_PRINT("event name is too long\n");
        return jerry_acquire_value(this);
    }

    // Event object to hold callback ID and eventually listener arguments
    jerry_value_t event_obj = zjs_get_property(ev->map, event);
    if (!jerry_value_is_object(event_obj)) {
        DBG_PRINT("event object not found for '%s'\n", event);
        return jerry_create_number(0);
    }

    int32_t callback_id = -1;
    int count = 0;
    jerry_value_t id_prop = zjs_get_property(event_obj, "callback_id");
    if (jerry_value_is_number(id_prop)) {
        // If there already is an event object, get the callback ID
        zjs_obj_get_int32(event_obj, "callback_id", &callback_id);
        count = zjs_get_num_callbacks(callback_id);
    } else {
        ERR_PRINT("callback_id not found for '%s'\n", event);
    }

    jerry_release_value(id_prop);
    jerry_release_value(event_obj);
    return jerry_create_number(count);
}

static jerry_value_t get_listeners(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc)
{
    event_t *ev;

    jerry_value_t event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));
    if (!jerry_get_object_native_handle(event_emitter, (uintptr_t*)&ev)) {
        jerry_release_value(event_emitter);
        ERR_PRINT("native handle not found\n");
        return ZJS_UNDEFINED;
    }
    jerry_release_value(event_emitter);

    if (!jerry_value_is_string(argv[0])) {
        ERR_PRINT("event name must be first parameter\n");
        return ZJS_UNDEFINED;
    }

    jerry_size_t size = ZJS_MAX_EVENT_NAME_SIZE;
    char event[size];
    zjs_copy_jstring(argv[0], event, &size);
    if (!size) {
        ERR_PRINT("event name is too long\n");
        return jerry_acquire_value(this);
    }

    // Event object to hold callback ID and eventually listener arguments
    jerry_value_t event_obj = zjs_get_property(ev->map, event);
    if (!jerry_value_is_object(event_obj)) {
        DBG_PRINT("event object not found for '%s'\n", event);
        return ZJS_UNDEFINED;
    }

    int32_t callback_id = -1;
    jerry_value_t id_prop = zjs_get_property(event_obj, "callback_id");
    
    if (jerry_value_is_number(id_prop)) {
        // If there already is an event object, get the callback ID
        zjs_obj_get_int32(event_obj, "callback_id", &callback_id);
    } else {
        jerry_release_value(id_prop);
        jerry_release_value(event_obj);
        ERR_PRINT("callback_id not found for '%s'\n", event);
        return ZJS_UNDEFINED;
    }

    int count;
    int i;
    jerry_value_t* func_array = zjs_get_callback_func_list(callback_id, &count);
    jerry_value_t ret_array = jerry_create_array(count);
    for (i = 0; i < count; ++i) {
        jerry_set_property_by_index(ret_array, i, func_array[i]);
    }

    jerry_release_value(id_prop);
    jerry_release_value(event_obj);
    return ret_array;
}

bool zjs_trigger_event(jerry_value_t obj,
                       const char* event,
                       const jerry_value_t *argv,
                       uint32_t argc,
                       zjs_post_event post,
                       void* h)
{
    event_t *ev;
    event_trigger_t *trigger = zjs_malloc(sizeof(event_trigger_t));
    if (!trigger) {
        ERR_PRINT("could not allocate trigger, out of memory\n");
        return false;
    }

    int32_t callback_id = -1;

    jerry_value_t event_emitter = zjs_get_property(obj, HIDDEN_PROP("event"));
    if (!jerry_get_object_native_handle(event_emitter, (uintptr_t*)&ev)) {
        jerry_release_value(event_emitter);
        zjs_free(trigger);
        ERR_PRINT("native handle not found\n");
        return false;
    }
    jerry_release_value(event_emitter);

    jerry_value_t event_obj = zjs_get_property(ev->map, event);
    if (!jerry_value_is_object(event_obj)) {
        jerry_release_value(event_obj);
        zjs_free(trigger);
        ERR_PRINT("event object not found\n");
        return false;
    }

    zjs_obj_get_int32(event_obj, "callback_id", &callback_id);
    jerry_release_value(event_obj);
    if (callback_id == -1) {
        zjs_free(trigger);
        ERR_PRINT("callback_id not found\n");
        return false;
    }

    trigger->handle = h;
    trigger->post = post;

    zjs_edit_callback_handle(callback_id, trigger);

    zjs_signal_callback(callback_id, argv, argc * sizeof(jerry_value_t));

    DBG_PRINT("triggering event '%s', args_cnt=%lu, callback_id=%ld\n",
              event, argc, callback_id);

    return true;
}

bool zjs_trigger_event_now(jerry_value_t obj,
                           const char* event,
                           jerry_value_t argv[],
                           uint32_t argc,
                           zjs_post_event post,
                           void* h)
{
    event_t *ev;
    event_trigger_t *trigger = zjs_malloc(sizeof(event_trigger_t));
    if (!trigger) {
        ERR_PRINT("could not allocate trigger, out of memory\n");
        return false;
    }

    int32_t callback_id = -1;

    jerry_value_t event_emitter = zjs_get_property(obj, HIDDEN_PROP("event"));
    if (!jerry_get_object_native_handle(event_emitter, (uintptr_t*)&ev)) {
        jerry_release_value(event_emitter);
        zjs_free(trigger);
        ERR_PRINT("native handle not found\n");
        return false;
    }
    jerry_release_value(event_emitter);

    jerry_value_t event_obj = zjs_get_property(ev->map, event);
    if (!jerry_value_is_object(event_obj)) {
        jerry_release_value(event_obj);
        zjs_free(trigger);
        ERR_PRINT("event object not found\n");
        return false;
    }

    zjs_obj_get_int32(event_obj, "callback_id", &callback_id);
    jerry_release_value(event_obj);
    if (callback_id == -1) {
        zjs_free(trigger);
        ERR_PRINT("callback_id not found\n");
        return false;
    }

    trigger->handle = h;
    trigger->post = post;

    DBG_PRINT("triggering event %s now\n", event);

    zjs_edit_callback_handle(callback_id, trigger);

    zjs_call_callback(callback_id, argv, argc);

    zjs_free(trigger);

    return true;
}

static void destroy_event(const uintptr_t pointer)
{
    event_t *ev = (event_t *)pointer;
    if (ev) {
        jerry_release_value(ev->map);
        zjs_free(ev);
    }
}

void zjs_make_event(jerry_value_t obj, jerry_value_t prototype)
{
    jerry_value_t event_obj = jerry_create_object();
    event_t *ev = zjs_malloc(sizeof(event_t));
    if (!ev) {
        jerry_release_value(event_obj);
        DBG_PRINT("could not allocate event handle, out of memory\n");
        return;
    }

    ev->max_listeners = DEFAULT_MAX_LISTENERS;
    ev->num_events = 0;
    ev->map = jerry_create_object();

    jerry_value_t proto = zjs_event_emitter_prototype;
    if (jerry_value_is_object(prototype)) {
        jerry_set_prototype(prototype, proto);
        proto = prototype;
    }
    jerry_set_prototype(obj, proto);

    jerry_set_object_native_handle(event_obj, (uintptr_t)ev, destroy_event);

    zjs_obj_add_object(obj, event_obj, HIDDEN_PROP("event"));
    jerry_release_value(event_obj);
}

static jerry_value_t event_constructor(const jerry_value_t function_obj,
                                       const jerry_value_t this,
                                       const jerry_value_t argv[],
                                       const jerry_length_t argc)
{
    jerry_value_t new_emitter = jerry_create_object();
    zjs_make_event(new_emitter, ZJS_UNDEFINED);
    return new_emitter;
}

jerry_value_t zjs_event_init()
{
    zjs_native_func_t array[] = {
        { add_listener, "on" },
        { add_listener, "addListener" },
        { emit_event, "emit" },
        { remove_listener, "removeListener" },
        { remove_all_listeners, "removeAllListeners" },
        { get_event_names, "eventNames" },
        { get_max_listeners, "getMaxListeners" },
        { get_listener_count, "listenerCount" },
        { get_listeners, "listeners" },
        { set_max_listeners, "setMaxListeners" },
        { NULL, NULL }
    };
    zjs_event_emitter_prototype = jerry_create_object();
    zjs_obj_add_functions(zjs_event_emitter_prototype, array);

    return jerry_create_external_function(event_constructor);
}

void zjs_event_cleanup()
{
    jerry_release_value(zjs_event_emitter_prototype);
}
