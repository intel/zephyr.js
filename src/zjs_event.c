#include "zjs_event.h"
#include "zjs_callbacks.h"

#define ZJS_MAX_EVENT_NAME_SIZE     24
#define DEFAULT_MAX_LISTENERS       10
#define HIDDEN_PROP(n) "\377" n

struct event {
    int num_events;
    jerry_value_t map;
    int max_listeners;
};

struct event_trigger {
    jerry_value_t* argv;
    uint32_t argc;
    void* handle;
    zjs_post_event post;
};

struct event_names {
    jerry_value_t name_array;
    int idx;
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
        zjs_free(trigger);
    }
}

void zjs_add_event_listener(jerry_value_t obj, const char* event, jerry_value_t listener)
{
    struct event* ev;

    jerry_value_t event_emitter = zjs_get_property(obj, HIDDEN_PROP("event"));
    if (!jerry_get_object_native_handle(event_emitter, (uintptr_t*)&ev)) {
        DBG_PRINT("native handle not found\n");
        return;
    }
    if (ev->num_events >= ev->max_listeners) {
        DBG_PRINT("max listeners reached\n");
        return;
    }

    // Event object to hold callback ID and eventually listener arguments
    jerry_value_t event_obj = zjs_get_property(ev->map, event);
    if (!jerry_value_is_object(event_obj)) {
        event_obj = jerry_create_object();
    }

    int32_t callback_id = -1;
    jerry_value_t id_prop = zjs_get_property(event_obj, "callback_id");
    if (jerry_value_is_number(id_prop)) {
        // If there already is an event object, get the callback ID
        zjs_obj_get_int32(event_obj, "callback_id", &callback_id);
    }
    callback_id = zjs_add_callback_list(listener, obj, ev, pre_event, post_event, callback_id);

    // Add callback ID to event object
    zjs_obj_add_number(event_obj, callback_id, "callback_id");
    // Add event object to master event listener
    zjs_set_property(ev->map, event, event_obj);

    DBG_PRINT("added listener, callback id = %ld\n", callback_id);

    ev->num_events++;
}

static jerry_value_t add_listener(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    if (!jerry_value_is_string(argv[0])) {
        DBG_PRINT("first parameter must be event string\n");
        return ZJS_UNDEFINED;
    }
    if (!jerry_value_is_function(argv[1])) {
        DBG_PRINT("second parameter must be a listener function\n");
        return ZJS_UNDEFINED;
    }
    int sz = jerry_get_string_size(argv[0]);
    if (sz > ZJS_MAX_EVENT_NAME_SIZE) {
        DBG_PRINT("event name is too long\n");
        return ZJS_UNDEFINED;
    }
    char name[sz];
    int len = jerry_string_to_char_buffer(argv[0], (jerry_char_t *)name, sz);
    if (len != sz) {
        DBG_PRINT("size mismatch\n");
        return ZJS_UNDEFINED;
    }
    name[len] = '\0';

    zjs_add_event_listener(this, name, argv[1]);

    return ZJS_UNDEFINED;
}

static jerry_value_t emit_event(const jerry_value_t function_obj,
                                const jerry_value_t this,
                                const jerry_value_t argv[],
                                const jerry_length_t argc)
{
    if (!jerry_value_is_string(argv[0])) {
        DBG_PRINT("parameter is not a string\n");
        return ZJS_UNDEFINED;
    }
    int sz = jerry_get_string_size(argv[0]);
    char event[sz];
    int len = jerry_string_to_char_buffer(argv[0], (jerry_char_t *)event, sz);
    if (len != sz) {
        DBG_PRINT("size mismatch\n");
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

static jerry_value_t remove_listener(const jerry_value_t function_obj,
                                     const jerry_value_t this,
                                     const jerry_value_t argv[],
                                     const jerry_length_t argc)
{
    struct event* ev;

    jerry_value_t event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));
    if (!jerry_get_object_native_handle(event_emitter, (uintptr_t*)&ev)) {
        DBG_PRINT("native handle not found\n");
        return ZJS_UNDEFINED;
    }
    if (!jerry_value_is_string(argv[0])) {
        DBG_PRINT("event name must be first parameter\n");
        return ZJS_UNDEFINED;
    }
    if (!jerry_value_is_function(argv[1])) {
        DBG_PRINT("event listener must be second parameter\n");
        return ZJS_UNDEFINED;
    }
    int sz = jerry_get_string_size(argv[0]);
    char event[sz];
    int len = jerry_string_to_char_buffer(argv[0], (jerry_char_t *)event, sz);
    if (len != sz) {
        DBG_PRINT("size mismatch\n");
        return ZJS_UNDEFINED;
    }
    event[len] = '\0';

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
        DBG_PRINT("callback_id not found for '%s'\n", event);
        return ZJS_UNDEFINED;
    }

    bool removed = zjs_remove_callback_list_func(callback_id, argv[1]);

    return jerry_create_boolean(removed);
}

static jerry_value_t remove_all_listeners(const jerry_value_t function_obj,
                                          const jerry_value_t this,
                                          const jerry_value_t argv[],
                                          const jerry_length_t argc)
{
    struct event* ev;

    jerry_value_t event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));
    if (!jerry_get_object_native_handle(event_emitter, (uintptr_t*)&ev)) {
        DBG_PRINT("native handle not found\n");
        return ZJS_UNDEFINED;
    }
    if (!jerry_value_is_string(argv[0])) {
        DBG_PRINT("event name must be first parameter\n");
        return ZJS_UNDEFINED;
    }
    int sz = jerry_get_string_size(argv[0]);
    char event[sz];
    int len = jerry_string_to_char_buffer(argv[0], (jerry_char_t *)event, sz);
    if (len != sz) {
        DBG_PRINT("size mismatch\n");
        return ZJS_UNDEFINED;
    }
    event[len] = '\0';

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
        DBG_PRINT("callback_id not found for '%s'\n", event);
        return ZJS_UNDEFINED;
    }

    zjs_remove_callback(callback_id);

    jerry_value_t event_name_val = jerry_create_string((const jerry_char_t*)event);
    jerry_delete_property(ev->map, (const jerry_value_t)event_name_val);
    jerry_release_value(event_name_val);

    return ZJS_UNDEFINED;
}

bool foreach_event_name(const jerry_value_t prop_name,
                        const jerry_value_t prop_value,
                        void *data)
{
    struct event_names* names = (struct event_names*)data;

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
    struct event* ev;
    struct event_names names;

    jerry_value_t event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));
    if (!jerry_get_object_native_handle(event_emitter, (uintptr_t*)&ev)) {
        DBG_PRINT("native handle not found\n");
        return ZJS_UNDEFINED;
    }
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
    struct event* ev;

    jerry_value_t event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));
    if (!jerry_get_object_native_handle(event_emitter, (uintptr_t*)&ev)) {
        DBG_PRINT("native handle not found\n");
        return ZJS_UNDEFINED;
    }
    return jerry_create_number(ev->max_listeners);
}

static jerry_value_t set_max_listeners(const jerry_value_t function_obj,
                                       const jerry_value_t this,
                                       const jerry_value_t argv[],
                                       const jerry_length_t argc)
{
    struct event* ev;

    jerry_value_t event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));
    if (!jerry_get_object_native_handle(event_emitter, (uintptr_t*)&ev)) {
        DBG_PRINT("native handle not found\n");
        return ZJS_UNDEFINED;
    }
    if (!jerry_value_is_number(argv[0])) {
        DBG_PRINT("max listeners count must be first parameter\n");
        return ZJS_UNDEFINED;
    }
    ev->max_listeners = (uint32_t)jerry_get_number_value(argv[0]);

    return ZJS_UNDEFINED;
}

bool foreach_listener(const jerry_value_t prop_name,
                      const jerry_value_t prop_value,
                      void *data)
{
    struct event_names* names = (struct event_names*)data;

    jerry_set_property_by_index(names->name_array,
                                names->idx++,
                                prop_name);
    return true;
}

static jerry_value_t get_listener_count(const jerry_value_t function_obj,
                                        const jerry_value_t this,
                                        const jerry_value_t argv[],
                                        const jerry_length_t argc)
{
    struct event* ev;

    jerry_value_t event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));
    if (!jerry_get_object_native_handle(event_emitter, (uintptr_t*)&ev)) {
        DBG_PRINT("native handle not found\n");
        return zjs_error("native handle not found");
    }
    if (!jerry_value_is_string(argv[0])) {
        DBG_PRINT("event name must be first parameter\n");
        return zjs_error("event name must be first parameter");
    }
    int sz = jerry_get_string_size(argv[0]);
    char event[sz];
    int len = jerry_string_to_char_buffer(argv[0], (jerry_char_t *)event, sz);
    if (len != sz) {
        DBG_PRINT("size mismatch\n");
        return zjs_error("string size mismatch");
    }
    event[len] = '\0';

    // Event object to hold callback ID and eventually listener arguments
    jerry_value_t event_obj = zjs_get_property(ev->map, event);
    if (!jerry_value_is_object(event_obj)) {
        DBG_PRINT("event object not found for '%s'\n", event);
        return jerry_create_number(0);
    }

    int32_t callback_id = -1;
    jerry_value_t id_prop = zjs_get_property(event_obj, "callback_id");
    if (jerry_value_is_number(id_prop)) {
        // If there already is an event object, get the callback ID
        zjs_obj_get_int32(event_obj, "callback_id", &callback_id);
    } else {
        DBG_PRINT("callback_id not found for '%s'\n", event);
        return jerry_create_number(0);
    }

    return jerry_create_number(zjs_get_num_callbacks(callback_id));
}

static jerry_value_t get_listeners(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc)
{
    struct event* ev;

    jerry_value_t event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));
    if (!jerry_get_object_native_handle(event_emitter, (uintptr_t*)&ev)) {
        DBG_PRINT("native handle not found\n");
        return ZJS_UNDEFINED;
    }
    if (!jerry_value_is_string(argv[0])) {
        DBG_PRINT("event name must be first parameter\n");
        return ZJS_UNDEFINED;
    }
    int sz = jerry_get_string_size(argv[0]);
    char event[sz];
    int len = jerry_string_to_char_buffer(argv[0], (jerry_char_t *)event, sz);
    if (len != sz) {
        DBG_PRINT("size mismatch\n");
        return ZJS_UNDEFINED;
    }
    event[len] = '\0';

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
        DBG_PRINT("callback_id not found for '%s'\n", event);
        return ZJS_UNDEFINED;
    }
    int count;
    int i;
    jerry_value_t* func_array = zjs_get_callback_func_list(callback_id, &count);
    jerry_value_t ret_array = jerry_create_array(count);
    for (i = 0; i < count; ++i) {
        jerry_set_property_by_index(ret_array, i, func_array[i]);
    }
    return ret_array;
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
        DBG_PRINT("could not allocate trigger, out of memory\n");
        return false;
    }

    int32_t callback_id = -1;
    jerry_value_t event_obj;

    jerry_value_t event_emitter = zjs_get_property(obj, HIDDEN_PROP("event"));
    if (!jerry_get_object_native_handle(event_emitter, (uintptr_t*)&ev)) {
        zjs_free(trigger);
        DBG_PRINT("native handle not found\n");
        return jerry_create_boolean(false);
    }

    int i;
    trigger->argv = zjs_malloc(sizeof(jerry_value_t) * argc);
    if (!trigger->argv) {
        zjs_free(trigger);
        DBG_PRINT("could not allocate trigger args, out of memory\n");
        return false;
    }
    for (i = 0; i < argc; ++i) {
        trigger->argv[i] = argv[i];
    }
    trigger->argc = argc;

    event_obj = zjs_get_property(ev->map, event);
    if (!jerry_value_is_object(event_obj)) {
        zjs_free(trigger);
        DBG_PRINT("event object not found\n");
        return false;
    }

    if (!zjs_obj_get_int32(event_obj, "callback_id", &callback_id)) {
        zjs_free(trigger);
        DBG_PRINT("[event] zjs_trigger_event(): Error, callback_id not found\n");
        return false;
    }

    trigger->handle = h;
    trigger->post = post;

    zjs_edit_callback_handle(callback_id, trigger);

    zjs_signal_callback(callback_id);

    DBG_PRINT("triggering event '%s', args_cnt=%lu, callback_id=%ld\n",
              event, trigger->argc, callback_id);

    return jerry_create_boolean(true);
}

bool zjs_trigger_event_now(jerry_value_t obj,
                           const char* event,
                           jerry_value_t argv[],
                           uint32_t argc,
                           zjs_post_event post,
                           void* h)
{
    struct event* ev;
    struct event_trigger* trigger = zjs_malloc(sizeof(struct event_trigger));
    if (!trigger) {
        DBG_PRINT("could not allocate trigger, out of memory\n");
        return false;
    }

    int32_t callback_id = -1;
    jerry_value_t event_obj;

    if (!jerry_get_object_native_handle(obj, (uintptr_t*)&ev)) {
        zjs_free(trigger);
        DBG_PRINT("native handle not found\n");
        return jerry_create_boolean(false);
    }

    int i;
    trigger->argv = zjs_malloc(sizeof(jerry_value_t) * argc);
    if (!trigger->argv) {
        zjs_free(trigger);
        DBG_PRINT("could not allocate trigger args, out of memory\n");
        return false;
    }
    for (i = 0; i < argc; ++i) {
        trigger->argv[i] = argv[i];
    }
    trigger->argc = argc;

    event_obj = zjs_get_property(ev->map, event);
    if (!jerry_value_is_object(event_obj)) {
        zjs_free(trigger);
        DBG_PRINT("event object not found\n");
        return false;
    }

    zjs_obj_get_uint32(event_obj, "callback_id", &callback_id);
    if (callback_id == -1) {
        zjs_free(trigger);
        DBG_PRINT("callback_id not found\n");
        return false;
    }

    trigger->handle = h;
    trigger->post = post;

    zjs_edit_callback_handle(callback_id, trigger);

    zjs_call_callback(callback_id);

    return jerry_create_boolean(true);
}

static void destroy_event(const uintptr_t pointer)
{
    struct event* ev = (struct event*)pointer;
    if (ev) {
        jerry_release_value(ev->map);
        zjs_free(ev);
    }
}

void zjs_make_event(jerry_value_t obj)
{
    jerry_value_t event_obj = jerry_create_object();
    struct event* ev = zjs_malloc(sizeof(struct event));
    if (!ev) {
        DBG_PRINT("could not allocate event handle, out of memory\n");
        return;
    }

    ev->max_listeners = DEFAULT_MAX_LISTENERS;
    ev->num_events = 0;
    ev->map = jerry_create_object();

    zjs_obj_add_function(obj, add_listener, "on");
    zjs_obj_add_function(obj, add_listener, "addListener");
    zjs_obj_add_function(obj, emit_event, "emit");
    zjs_obj_add_function(obj, remove_listener, "removeListener");
    zjs_obj_add_function(obj, remove_all_listeners, "removeAllListeners");
    zjs_obj_add_function(obj, get_event_names, "eventNames");
    zjs_obj_add_function(obj, get_max_listeners, "getMaxListeners");
    zjs_obj_add_function(obj, get_listener_count, "listenerCount");
    zjs_obj_add_function(obj, get_listeners, "listeners");
    zjs_obj_add_function(obj, set_max_listeners, "setMaxListeners");

    jerry_set_object_native_handle(event_obj, (uintptr_t)ev, destroy_event);

    zjs_obj_add_object(obj, event_obj, HIDDEN_PROP("event"));
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
