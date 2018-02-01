// Copyright (c) 2016-2018, Intel Corporation.

// C includes
#include <string.h>

// ZJS includes
#include "zjs_callbacks.h"
#include "zjs_event.h"
#include "zjs_util.h"

#define ZJS_MAX_EVENT_NAME_SIZE 24
#define DEFAULT_MAX_LISTENERS   10

static jerry_value_t zjs_event_emitter_prototype = 0;
zjs_callback_id emit_id = -1;

typedef struct listener {
    jerry_value_t func;
    struct listener *next;
} listener_t;

// allocate sizeof(event_t) + len(name)
typedef struct event {
    struct event *next;
    listener_t *listeners;
    int namelen;
    char name[1];
} event_t;

typedef struct emitter {
    int max_listeners;
    event_t *events;
    void *user_handle;
    zjs_event_free user_free;
} emitter_t;

static void free_listener(void *ptr)
{
    listener_t *listener = (listener_t *)ptr;
    jerry_release_value(listener->func);
    zjs_free(listener);
}

static void zjs_event_proto_free_cb(void *native)
{
    zjs_event_emitter_prototype = 0;
    zjs_remove_callback(emit_id);
    emit_id = -1;
}

static const jerry_object_native_info_t event_proto_type_info = {
    .free_cb = zjs_event_proto_free_cb
};

static void zjs_emitter_free_cb(void *native)
{
    emitter_t *handle = (emitter_t *)native;
    event_t *event = handle->events;
    while (event) {
        ZJS_LIST_FREE(listener_t, event->listeners, free_listener);
        event = event->next;
    }
    ZJS_LIST_FREE(event_t, handle->events, zjs_free);
    if (handle->user_free) {
        handle->user_free(handle->user_handle);
    }
    zjs_free(handle);
}

static const jerry_object_native_info_t emitter_type_info = {
    .free_cb = zjs_emitter_free_cb
};

static int compare_name(event_t *event, const char *name)
{
    return strncmp(event->name, name, event->namelen + 1);
}

jerry_value_t zjs_add_event_listener(jerry_value_t obj, const char *event_name,
                                     jerry_value_t func)
{
    // requires: event is an event name; func is a valid JS function
    //  returns: an error, or 0 value on success
    ZJS_GET_HANDLE_ALT(obj, emitter_t, handle, emitter_type_info);

    event_t *event = ZJS_LIST_FIND_CMP(event_t, handle->events, compare_name,
                                       event_name);
    if (!event) {
        int len = strlen(event_name);
        event = (event_t *)zjs_malloc(sizeof(event_t) + len);
        if (!event) {
            return zjs_error_context("out of memory", 0, 0);
        }
        event->next = NULL;
        event->listeners = NULL;
        event->namelen = len;
        strcpy(event->name, event_name);
        ZJS_LIST_APPEND(event_t, handle->events, event);
    }

    listener_t *listener = zjs_malloc(sizeof(listener_t));
    if (!listener) {
        zjs_free(event);
        return zjs_error_context("out of memory", 0, 0);
    }

    listener->func = jerry_acquire_value(func);
    listener->next = NULL;

    ZJS_LIST_APPEND(listener_t, event->listeners, listener);

    if (ZJS_LIST_LENGTH(listener_t, event->listeners) > handle->max_listeners) {
        // warn of possible leak as per Node docs
        ZJS_PRINT("possible memory leak on event %s\n", event_name);
    }

#ifdef ZJS_FIND_FUNC_NAME
    char name[strlen(event_name) + sizeof("event: ")];
    sprintf(name, "event: %s", event_name);
    zjs_obj_add_string(func, ZJS_HIDDEN_PROP("function_name"), name);
#endif

    return 0;
}

static ZJS_DECL_FUNC(add_listener)
{
    // args: event name, callback
    ZJS_VALIDATE_ARGS(Z_STRING, Z_FUNCTION);

    char *name = zjs_alloc_from_jstring(argv[0], NULL);
    if (!name) {
        return zjs_error("out of memory");
    }

    jerry_value_t rval = zjs_add_event_listener(this, name, argv[1]);
    zjs_free(name);

    if (jerry_value_has_error_flag(rval)) {
        return rval;
    }

    // return this object to allow chaining
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

    bool rval = zjs_emit_event(this, event, argv + 1, argc - 1);

    // return true if there were listeners called
    return jerry_create_boolean(rval);
}

static ZJS_DECL_FUNC(remove_listener)
{
    // args: event name, callback
    ZJS_VALIDATE_ARGS(Z_STRING, Z_FUNCTION);

    ZJS_GET_HANDLE(this, emitter_t, handle, emitter_type_info);

    char *name = zjs_alloc_from_jstring(argv[0], NULL);
    if (!name) {
        return zjs_error("out of memory");
    }

    event_t *event = ZJS_LIST_FIND_CMP(event_t, handle->events, compare_name,
                                       name);
    listener_t *listener = NULL;
    zjs_free(name);

    if (event) {
        listener = ZJS_LIST_FIND(listener_t, event->listeners, func, argv[1]);
    }

    if (!listener) {
        return zjs_error("no such event listener found");
    }

    ZJS_LIST_REMOVE(listener_t, event->listeners, listener);
    free_listener(listener);

    return jerry_acquire_value(this);
}

static ZJS_DECL_FUNC(remove_all_listeners)
{
    // args: event name
    ZJS_VALIDATE_ARGS(Z_STRING);

    ZJS_GET_HANDLE(this, emitter_t, handle, emitter_type_info);

    char *name = zjs_alloc_from_jstring(argv[0], NULL);
    if (!name) {
        return zjs_error("out of memory");
    }

    event_t *event = ZJS_LIST_FIND_CMP(event_t, handle->events, compare_name,
                                       name);
    zjs_free(name);
    if (!event) {
        return zjs_error("no event listeners found");
    }

    ZJS_LIST_FREE(listener_t, event->listeners, free_listener);

    return jerry_acquire_value(this);
}

static ZJS_DECL_FUNC(get_event_names)
{
    ZJS_GET_HANDLE(this, emitter_t, handle, emitter_type_info);

    // FIXME: pre-register events
    int len = ZJS_LIST_LENGTH(event_t, handle->events);
    jerry_value_t rval = jerry_create_array(len);

    event_t *event = handle->events;
    int i = 0;
    while (event) {
        jerry_set_property_by_index(rval, i++,
            jerry_create_string((jerry_char_t *)event->name));
        event = event->next;
    }

    return rval;
}

static ZJS_DECL_FUNC(get_max_listeners)
{
    ZJS_GET_HANDLE(this, emitter_t, handle, emitter_type_info);

    return jerry_create_number(handle->max_listeners);
}

static ZJS_DECL_FUNC(set_max_listeners)
{
    // args: max count
    ZJS_VALIDATE_ARGS(Z_NUMBER);

    ZJS_GET_HANDLE(this, emitter_t, handle, emitter_type_info);

    // FIXME: Support Infinity/0 for listeners (which means no limit)
    handle->max_listeners = (u32_t)jerry_get_number_value(argv[0]);

    return jerry_acquire_value(this);
}

static ZJS_DECL_FUNC(get_listener_count)
{
    // args: event name
    ZJS_VALIDATE_ARGS(Z_STRING);

    ZJS_GET_HANDLE(this, emitter_t, handle, emitter_type_info);

    char *name = zjs_alloc_from_jstring(argv[0], NULL);
    if (!name) {
        return zjs_error("out of memory");
    }

    event_t *event = ZJS_LIST_FIND_CMP(event_t, handle->events, compare_name,
                                       name);
    zjs_free(name);
    if (!event) {
        return jerry_create_number(0);
    }

    int len = ZJS_LIST_LENGTH(listener_t, event->listeners);
    return jerry_create_number(len);
}

static ZJS_DECL_FUNC(get_listeners)
{
    // args: event name
    ZJS_VALIDATE_ARGS(Z_STRING);

    ZJS_GET_HANDLE(this, emitter_t, handle, emitter_type_info);

    char *name = zjs_alloc_from_jstring(argv[0], NULL);
    if (!name) {
        return zjs_error("out of memory");
    }

    event_t *event = ZJS_LIST_FIND_CMP(event_t, handle->events, compare_name,
                                       name);
    zjs_free(name);
    if (!event) {
        return jerry_create_array(0);
    }

    int len = ZJS_LIST_LENGTH(event_t, handle->events);
    jerry_value_t rval = jerry_create_array(len);

    listener_t *listener = event->listeners;
    int i = 0;
    while (listener) {
        jerry_set_property_by_index(rval, i++, listener->func);
        listener = listener->next;
    }

    return rval;
}

typedef struct emit_event {
    jerry_value_t obj;
    zjs_pre_emit pre;
    zjs_post_emit post;
    u32_t length;  // length of user data
    char data[0];  // data is user data followed by null-terminated event name
} emit_event_t;

static void emit_event_callback(void *handle, const void *args)
{
    const emit_event_t *emit = (const emit_event_t *)args;

    void *user_handle = zjs_event_get_user_handle(emit->obj);

    // prepare arguments for the event
    jerry_value_t argv[MAX_EVENT_ARGS];
    jerry_value_t *argp = argv;
    u32_t argc = 0;
    if (emit->pre) {
        if (!emit->pre(user_handle, argv, &argc, emit->data, emit->length)) {
            // event cancelled
            DBG_PRINT("event cancelled\n");
            return;
        }
        if (argc > MAX_EVENT_ARGS) {
            ERR_PRINT("Must increase MAX_EVENT_ARGS!");
        }
    }
    if (argc == 0) {
        argp = NULL;
    }

    // emit the event
    zjs_emit_event(emit->obj, emit->data + emit->length, argp, argc);
    // TODO: possibly do something different depending on success/failure?

    // free args
    if (emit->post) {
        // TODO: figure out what is needed for args here
        emit->post(user_handle, argv, argc);
    }
}

// a zjs_pre_emit callback
bool zjs_copy_arg(void *unused, jerry_value_t argv[], u32_t *argc,
                  const char *buffer, u32_t bytes)
{
    // requires: buffer contains one jerry_value_t
    ZJS_ASSERT(bytes == sizeof(jerry_value_t), "invalid data received");
    argv[0] = *(jerry_value_t *)buffer;
    *argc = 1;
    return true;
}

// a zjs_post_emit callback
void zjs_release_args(void *unused, jerry_value_t argv[], u32_t argc)
{
    // effects: releases all jerry values in argv baesd on argc count
    for (int i = 0; i < argc; i++) {
        jerry_release_value(argv[i]);
    }
}

void zjs_defer_emit_event_priv(jerry_value_t obj, const char *event,
                               const void *buffer, int bytes,
                               zjs_pre_emit pre, zjs_post_emit post)
{
    // requires: don't exceed MAX_EVENT_ARGS in pre function
    //  effects: threadsafe way to schedule an event to be triggered from the
    //             main thread in the next event loop pass
    DBG_PRINT("queuing event '%s'\n", event);

    int namelen = strlen(event);
    int len = sizeof(emit_event_t) + namelen + 1 + bytes;
    char buf[len];
    emit_event_t *emit = (emit_event_t *)buf;
    emit->obj = obj;
    emit->pre = pre;
    emit->post = post;
    emit->length = bytes;
    if (buffer && bytes) {
        memcpy(emit->data, buffer, bytes);
    }
    // assert: if buffer is null, bytes should be 0, and vice versa
    strcpy(emit->data + bytes, event);
    zjs_signal_callback(emit_id, buf, len);
}

bool zjs_emit_event_priv(jerry_value_t obj, const char *event_name,
                         const jerry_value_t argv[], u32_t argc)
{
    // effects: emits event now, should only be called from main thread
    DBG_PRINT("emitting event '%s'\n", event_name);

    ZJS_GET_HANDLE_OR_NULL(obj, emitter_t, handle, emitter_type_info);
    if (!handle) {
        ERR_PRINT("no handle found\n");
        return false;
    }

    event_t *event = ZJS_LIST_FIND_CMP(event_t, handle->events, compare_name,
                                       event_name);
    if (!event || !event->listeners) {
        DBG_PRINT("Event '%s' not found or no listeners\n", event_name);
        return false;
    }

    // call the listeners in order
    listener_t *listener = event->listeners;
    while (listener) {
        ZVAL rval = jerry_call_function(listener->func, obj, argv, argc);
        if (jerry_value_has_error_flag(rval)) {
            ERR_PRINT("error calling listener\n");
        }
        listener = listener->next;
    }

    return true;
}

void zjs_destroy_emitter(jerry_value_t obj)
{
    // FIXME: probably better to more fully free in case JS keeps obj around
    ZJS_GET_HANDLE_OR_NULL(obj, emitter_t, handle, emitter_type_info);
    if (handle) {
        event_t *event = handle->events;
        while (event) {
            ZJS_LIST_FREE(listener_t, event->listeners, free_listener);
            event = event->next;
        }
    }
}

void *zjs_event_get_user_handle(jerry_value_t obj)
{
    ZJS_GET_HANDLE_OR_NULL(obj, emitter_t, handle, emitter_type_info);
    if (handle) {
        return handle->user_handle;
    }
    return NULL;
}

static void zjs_event_init_prototype()
{
    if (!zjs_event_emitter_prototype) {
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
        zjs_event_emitter_prototype = zjs_create_object();
        zjs_obj_add_functions(zjs_event_emitter_prototype, array);
        jerry_set_object_native_pointer(zjs_event_emitter_prototype, NULL,
                                        &event_proto_type_info);
        jerry_release_value(zjs_event_emitter_prototype);
        emit_id = zjs_add_c_callback(NULL, emit_event_callback);
    }
}

void zjs_make_emitter(jerry_value_t obj, jerry_value_t prototype,
                      void *user_data, zjs_event_free free_cb)
{
    zjs_event_init_prototype();
    jerry_value_t proto = zjs_event_emitter_prototype;
    if (jerry_value_is_object(prototype)) {
        jerry_set_prototype(prototype, proto);
        proto = prototype;
    }
    jerry_set_prototype(obj, proto);

    emitter_t *emitter = zjs_malloc(sizeof(emitter_t));
    emitter->max_listeners = DEFAULT_MAX_LISTENERS;
    emitter->events = NULL;
    emitter->user_free = free_cb;
    emitter->user_handle = user_data;
    jerry_set_object_native_pointer(obj, emitter, &emitter_type_info);
}

static ZJS_DECL_FUNC(event_constructor)
{
    jerry_value_t new_emitter = zjs_create_object();
    zjs_make_emitter(new_emitter, ZJS_UNDEFINED, NULL, NULL);
    return new_emitter;
}

static jerry_value_t zjs_event_init()
{
    // NOTE: dropped defaultMaxListeners as this didn't seem important for us
    return jerry_create_external_function(event_constructor);
}

JERRYX_NATIVE_MODULE(events, zjs_event_init)
