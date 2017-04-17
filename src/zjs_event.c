// Copyright (c) 2016-2017, Intel Corporation.

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
    void *handle;
    zjs_post_event post;
} event_trigger_t;

typedef struct event_names {
    jerry_value_t name_array;
    int idx;
} event_names_t;

void post_event(void *h, jerry_value_t *ret_val)
{
    event_trigger_t *trigger = (event_trigger_t *)h;
    if (trigger) {
        if (trigger->post) {
            trigger->post(trigger->handle);
        }
        zjs_free(trigger);
    }
}

static uint32_t get_num_events(jerry_value_t emitter)
{
    ZVAL val = zjs_get_property(emitter, "numEvents");
    if (!jerry_value_is_number(val)) {
        ERR_PRINT("emitter had no numEvents property\n");
        return 0;
    }
    uint32_t num = jerry_get_number_value(val);
    return num;
}

static uint32_t get_max_event_listeners(jerry_value_t emitter)
{
    ZVAL val = zjs_get_property(emitter, "maxListeners");
    if (!jerry_value_is_number(val)) {
        ERR_PRINT("emitter had no maxListeners property\n");
        return 0;
    }
    uint32_t num = jerry_get_number_value(val);
    return num;
}

static int32_t get_callback_id(jerry_value_t event_obj)
{
    int32_t callback_id = -1;
    ZVAL id_prop = zjs_get_property(event_obj, "callback_id");
    if (jerry_value_is_number(id_prop)) {
        // If there already is an event object, get the callback ID
        zjs_obj_get_int32(event_obj, "callback_id", &callback_id);
    }
    return callback_id;
}

void zjs_add_event_listener(jerry_value_t obj, const char *event,
                            jerry_value_t listener)
{
    ZVAL event_emitter = zjs_get_property(obj, HIDDEN_PROP("event"));
    if (!jerry_value_is_object(event_emitter)) {
        ERR_PRINT("no event '%s' found\n", event);
        return;
    }
    uint32_t num_events = get_num_events(event_emitter);
    uint32_t max_listeners = get_max_event_listeners(event_emitter);

    if (num_events >= max_listeners) {
        ERR_PRINT("max listeners reached\n");
        return;
    }

    ZVAL map = zjs_get_property(event_emitter, "map");
    jerry_value_t event_prop = zjs_get_property(map, event);

    // Event object to hold callback ID and eventually listener arguments
    ZVAL event_obj = jerry_value_is_object(event_prop) ? event_prop :
        jerry_create_object();

    int32_t callback_id = get_callback_id(event_obj);
    callback_id = zjs_add_callback_list(listener, obj, NULL, post_event,
                                        callback_id);
    // Add callback ID to event object
    zjs_obj_add_number(event_obj, callback_id, "callback_id");
    // Add event object to master event listener
    zjs_set_property(map, event, event_obj);

    DBG_PRINT("added listener, callback id = %ld\n", callback_id);

    zjs_obj_add_number(event_emitter, ++num_events, "numEvents");
}

static ZJS_DECL_FUNC(add_listener)
{
    // args: event name, callback
    ZJS_VALIDATE_ARGS(Z_STRING, Z_FUNCTION);

    jerry_size_t size = ZJS_MAX_EVENT_NAME_SIZE;
    char name[size];
    zjs_copy_jstring(argv[0], name, &size);
    if (!size) {
        return zjs_error("event name is too long");
    }
    zjs_add_event_listener(this, name, argv[1]);
    return jerry_acquire_value(this);
}

static ZJS_DECL_FUNC(emit_event)
{
    // args: event name[, additional pass-through args]
    ZJS_VALIDATE_ARGS(Z_STRING);

    jerry_size_t size = ZJS_MAX_EVENT_NAME_SIZE;
    char event[size];
    zjs_copy_jstring(argv[0], event, &size);
    if (!size) {
        return zjs_error("event name is too long");
    }

    // FIXME: This is supposed to return true if there were listeners; false,
    //   otherwise. Check that it's behaving correctly. Doesn't look like it.
    return jerry_create_boolean(zjs_trigger_event(this, event, argv + 1,
                                                  argc - 1, NULL, NULL));
}

static ZJS_DECL_FUNC(remove_listener)
{
    // args: event name, callback
    ZJS_VALIDATE_ARGS(Z_STRING, Z_FUNCTION);

    ZVAL event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));

    jerry_size_t size = ZJS_MAX_EVENT_NAME_SIZE;
    char event[size];
    zjs_copy_jstring(argv[0], event, &size);
    if (!size) {
        return zjs_error("event name is too long");
    }

    ZVAL map = zjs_get_property(event_emitter, "map");
    ZVAL event_obj = zjs_get_property(map, event);

    // Event object to hold callback ID and eventually listener arguments
    if (!jerry_value_is_object(event_obj)) {
        ERR_PRINT("event object not found\n");
        return ZJS_UNDEFINED;
    }

    int32_t callback_id = get_callback_id(event_obj);
    if (callback_id != -1) {
        zjs_remove_callback_list_func(callback_id, argv[1]);
    } else {
        ERR_PRINT("callback_id not found for '%s'\n", event);
    }

    return jerry_acquire_value(this);
}

static ZJS_DECL_FUNC(remove_all_listeners)
{
    // args: event name
    ZJS_VALIDATE_ARGS(Z_STRING);

    ZVAL event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));

    jerry_size_t size = ZJS_MAX_EVENT_NAME_SIZE;
    char event[size];
    zjs_copy_jstring(argv[0], event, &size);
    if (!size) {
        return zjs_error("event name is too long");
    }

    ZVAL map = zjs_get_property(event_emitter, "map");
    ZVAL event_obj = zjs_get_property(map, event);

    // Event object to hold callback ID and eventually listener arguments
    if (!jerry_value_is_object(event_obj)) {
        ERR_PRINT("event object not found\n");
        return ZJS_UNDEFINED;
    }

    int32_t callback_id = get_callback_id(event_obj);
    if (callback_id != -1) {
        zjs_remove_callback(callback_id);

        ZVAL name = jerry_create_string((const jerry_char_t *)event);
        jerry_delete_property(map, name);
    } else {
        ERR_PRINT("callback_id not found for '%s'\n", event);
    }

    zjs_obj_add_number(event_emitter, 0, "numEvents");

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

static ZJS_DECL_FUNC(get_event_names)
{
    event_names_t names;

    ZVAL event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));
    uint32_t num_events = get_num_events(event_emitter);
    ZVAL map = zjs_get_property(event_emitter, "map");

    names.idx = 0;
    names.name_array = jerry_create_array(num_events);

    jerry_foreach_object_property(map, foreach_event_name, &names);

    return names.name_array;
}

static ZJS_DECL_FUNC(get_max_listeners)
{
    ZVAL event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));
    uint32_t max_listeners = get_max_event_listeners(event_emitter);
    return jerry_create_number(max_listeners);
}

static ZJS_DECL_FUNC(set_max_listeners)
{
    // args: max count
    ZJS_VALIDATE_ARGS(Z_NUMBER);

    ZVAL event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));

    double num = jerry_get_number_value(argv[0]);
    if (num < 0) {
        return zjs_error("max listener value must be a positive integer");
    }
    zjs_obj_add_number(event_emitter, num, "maxListeners");

    return jerry_acquire_value(this);
}

static ZJS_DECL_FUNC(get_listener_count)
{
    // args: event name
    ZJS_VALIDATE_ARGS(Z_STRING);

    ZVAL event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));

    jerry_size_t size = ZJS_MAX_EVENT_NAME_SIZE;
    char event[size];
    zjs_copy_jstring(argv[0], event, &size);
    if (!size) {
        return zjs_error("event name is too long");
    }

    ZVAL map = zjs_get_property(event_emitter, "map");
    ZVAL event_obj = zjs_get_property(map, event);

    if (!jerry_value_is_object(event_obj)) {
        return jerry_create_number(0);
    }

    int32_t callback_id = get_callback_id(event_obj);
    int count = 0;
    if (callback_id != -1) {
        count = zjs_get_num_callbacks(callback_id);
    } else {
        ERR_PRINT("callback_id not found for '%s'\n", event);
    }

    return jerry_create_number(count);
}

static ZJS_DECL_FUNC(get_listeners)
{
    // args: event name
    ZJS_VALIDATE_ARGS(Z_STRING);

    ZVAL event_emitter = zjs_get_property(this, HIDDEN_PROP("event"));

    jerry_size_t size = ZJS_MAX_EVENT_NAME_SIZE;
    char event[size];
    zjs_copy_jstring(argv[0], event, &size);
    if (!size) {
        return zjs_error("event name is too long");
    }

    ZVAL map = zjs_get_property(event_emitter, "map");
    ZVAL event_obj = zjs_get_property(map, event);

    if (!jerry_value_is_object(event_obj)) {
        return zjs_error("event object not found");
    }

    int32_t callback_id = get_callback_id(event_obj);
    if (callback_id == -1) {
        ERR_PRINT("callback_id not found for '%s'\n", event);
        return ZJS_UNDEFINED;
    }

    int count;
    int i;
    // not using ZVAL because this is a pointer to values and one we will return
    jerry_value_t *func_array = zjs_get_callback_func_list(callback_id, &count);
    jerry_value_t ret_array = jerry_create_array(count);
    for (i = 0; i < count; ++i) {
        jerry_set_property_by_index(ret_array, i, func_array[i]);
    }

    return ret_array;
}

bool zjs_trigger_event(jerry_value_t obj,
                       const char *event,
                       const jerry_value_t argv[],
                       uint32_t argc,
                       zjs_post_event post,
                       void *h)
{
    ZVAL event_emitter = zjs_get_property(obj, HIDDEN_PROP("event"));
    ZVAL map = zjs_get_property(event_emitter, "map");
    ZVAL event_obj = zjs_get_property(map, event);

    if (!jerry_value_is_object(event_obj)) {
        DBG_PRINT("event object not found\n");
        return false;
    }

    int32_t callback_id = get_callback_id(event_obj);
    if (callback_id == -1) {
        ERR_PRINT("callback_id not found\n");
        return false;
    }

    event_trigger_t *trigger = zjs_malloc(sizeof(event_trigger_t));
    if (!trigger) {
        ERR_PRINT("could not allocate trigger, out of memory\n");
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
                           const char *event,
                           const jerry_value_t argv[],
                           uint32_t argc,
                           zjs_post_event post,
                           void *h)
{
    ZVAL event_emitter = zjs_get_property(obj, HIDDEN_PROP("event"));
    ZVAL map = zjs_get_property(event_emitter, "map");
    ZVAL event_obj = zjs_get_property(map, event);

    if (!jerry_value_is_object(event_obj)) {
        ERR_PRINT("event object not found\n");
        return false;
    }

    int32_t callback_id = get_callback_id(event_obj);
    if (callback_id == -1) {
        ERR_PRINT("callback_id not found\n");
        return false;
    }

    event_trigger_t *trigger = zjs_malloc(sizeof(event_trigger_t));
    if (!trigger) {
        ERR_PRINT("could not allocate trigger, out of memory\n");
        return false;
    }
    trigger->handle = h;
    trigger->post = post;

    zjs_edit_callback_handle(callback_id, trigger);

    zjs_call_callback(callback_id, argv, argc);

    return true;
}

void zjs_make_event(jerry_value_t obj, jerry_value_t prototype)
{
    ZVAL event_obj = jerry_create_object();

    zjs_obj_add_number(event_obj, DEFAULT_MAX_LISTENERS, "maxListeners");
    zjs_obj_add_number(event_obj, 0, "numEvents");

    ZVAL map = jerry_create_object();
    zjs_set_property(event_obj, "map", map);

    jerry_value_t proto = zjs_event_emitter_prototype;
    if (jerry_value_is_object(prototype)) {
        jerry_set_prototype(prototype, proto);
        proto = prototype;
    }
    jerry_set_prototype(obj, proto);

    zjs_obj_add_object(obj, event_obj, HIDDEN_PROP("event"));
}

static ZJS_DECL_FUNC(event_constructor)
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
    zjs_obj_add_number(zjs_event_emitter_prototype,
                       (double)DEFAULT_MAX_LISTENERS,
                       "defaultMaxListeners");

    return jerry_create_external_function(event_constructor);
}

void zjs_event_cleanup()
{
    jerry_release_value(zjs_event_emitter_prototype);
}
