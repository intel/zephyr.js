// Copyright (c) 2016-2018, Intel Corporation.

#ifndef __zjs_event_h__
#define __zjs_event_h__

// ZJS includes
#include "zjs_util.h"

// enable to trace all callers of zjs_emit_event / zjs_defer_emit_event
#define DEBUG_TRACE_EMIT 0

// This needs to be big enough to fit all the args that any zjs_pre_emit
// function wants to use, so may need to be increased in the future. An error
// message in emit_event_callback should let you know that you've exceeded the
// limit.
#define MAX_EVENT_ARGS 2

/**
 * Callback prototype for before an event is emitted
 *
 * The callback is responsible for setting up the arguments; if it returns
 * false, the event will not fire and the post callback will not be called, so
 * it must clean up after itself.
 *
 * @param handle  Event user handle provided to zjs_make_emitter
 * @param argv    Arg array w/ max of four args to be set up
 * @param argc    Pointer to arg count to be set (default 0)
 * @param buffer  Data provided to zjs_defer_emit_event from which to
 *                  construct arguments
 * @param length  Length of buffer
 *
 * @return        true to confirm event, false to cancel event
 */
typedef bool (*zjs_pre_emit)(void *handle, jerry_value_t argv[], u32_t *argc,
                             const char *buffer, u32_t length);

/**
 * Callback prototype for after an event is emitted
 *
 * @param handle  Event user handle provided to zjs_make_emitter
 * @param argv    Arg array w/ max of four args to be cleaned up
 * @param argc    Arg count
 */
typedef void (*zjs_post_emit)(void *handle, jerry_value_t argv[], u32_t argc);

/**
 * Callback prototype for after an event is triggered
 *
 * @param handle        Handle given to zjs_trigger_event()
 */
typedef void (*zjs_post_event)(void *handle);

/**
 * Callback prototype for when an event object is freed
 *
 * @param handle        Handle given to zjs_make_emitter()
 */
typedef void (*zjs_event_free)(void *handle);

/**
 * Turns an object into an event emitter object
 *
 * After this call the object will have all the event functions like
 * addListener(), on(), etc. This object can also be used to trigger events in
 * C. If the object needs no other prototype, pass undefined and the event
 * emitter prototype will be used. If a prototype is given, it will be used as
 * the object's prototype but its prototype in turn will be set to the event
 * emitter prototype.
 *
 * @param obj           Object to turn into an event object
 * @param prototype     Object to decorate and use as prototype, or undefined
 * @param user_handle   A handle the caller can get back later
 * @param free_cb       A callback for cleanup related to user_handle
 */
void zjs_make_emitter(jerry_value_t obj, jerry_value_t prototype,
                      void *user_handle, zjs_event_free free_cb);

/**
 * Remove all events and listeners from the event emitter
 *
 * This should be called for C-based event emitters when we know that the
 * emitter has become inactive; it allows the listener functions to be freed
 * so they won't hold circular references to the emitter object.
 *
 * TODO: Later we may want to add finer-granularity support for unregistering
 * one event at a time, but that doesn't seem to be needed now.
 *
 * FIXME: There is still a problem in JS-based event emitters that we will
 * not know when to "destroy" them like this. A simple but Node-incompatible
 * solution would be to expose this as a JS API. Another is to revert to
 * storing the listeners in a hidden property array; properties are held with
 * a looser reference than a refcount and allowed by JrS to be freed along
 * with an object.
 */
void zjs_destroy_emitter(jerry_value_t obj);

/**
 * Get back the user handle for this event supplied to zjs_make_emitter
 *
 * @param obj           Object to turn into an event object
 */
void *zjs_event_get_user_handle(jerry_value_t obj);

/**
 * Add a new event listener to an event object.
 *
 * @param obj         Object to add listener to
 * @param event_name  Name of new/existing event
 * @param func        Function to be called when the event is triggered
 *
 * @return            Error or 0 value on success
 */
jerry_value_t zjs_add_event_listener(jerry_value_t obj, const char *event_name,
                                     jerry_value_t func);

/**
 * zjs_defer_emit_event: Emit an event from a callback on the main thread
 *
 * This can be called from interrupt context or another thread, so to avoid any
 * JerryScript calls, it takes data in a simple buffer format. This could be a
 * struct that the caller understands. When we switch over to the main thread
 * and prepare to emit the event, we will call the supplied 'pre' function that
 * will turn that data into a JerryScript argument list. The 'post' function
 * gives a chance to release references to them.
 *
 * @param obj     Object that contains the event to be triggered
 * @param name    Name of event
 * @param buffer  Data needed to call event listeners
 * @param bytes   Size of buffer
 * @param pre     Arg setup function called before event emitted
 * @param post    Arg teardown function called after event emitted
 *
 * @return        True if there were listeners
 */
#if DEBUG_TRACE_EMIT
#define zjs_defer_emit_event(obj, name, buffer, bytes, pre, post)              \
    {                                                                          \
        ZJS_PRINT("[EVENT] %s:%d Deferring '%s'\n", __FILE__, __LINE__, name); \
        zjs_defer_emit_event_priv(obj, name, buffer, bytes, pre, post);        \
    }
#else
#define zjs_defer_emit_event zjs_defer_emit_event_priv
#endif

// NOTE: don't call the priv version directly
void zjs_defer_emit_event_priv(jerry_value_t obj, const char *name,
                               const void *buffer, int bytes,
                               zjs_pre_emit pre, zjs_post_emit post);

/**
 * zjs_emit_event: Call any registered event listeners immediately
 *
 * @param obj   Object that contains the event to be triggered
 * @param name  Name of event
 * @param argv  Arguments to give to the event listeners as parameters
 * @param argc  Number of arguments
 *
 * @return      True if there were listeners called
 */
#if DEBUG_TRACE_EMIT
#define zjs_emit_event(obj, name, argv, argc)                                 \
    ({                                                                        \
        ZJS_PRINT("[EVENT] %s:%d Emitting '%s'\n", __FILE__, __LINE__, name); \
        zjs_emit_event_priv(obj, name, argv, argc);                           \
    })
#else
#define zjs_emit_event zjs_emit_event_priv
#endif

// NOTE: don't call the priv version directly
bool zjs_emit_event_priv(jerry_value_t obj, const char *name,
                         const jerry_value_t argv[], u32_t argc);

// emit helpers

/**
 * Copies one jerry_value_t from buffer to argv[0]
 *
 * A zjs_pre_emit callback.
 */
bool zjs_copy_arg(void *unused, jerry_value_t argv[], u32_t *argc,
                  const char *buffer, u32_t bytes);

/**
 * Releases the jerry_value_t's in argv
 *
 * A zjs_post_emit callback.
 */
void zjs_release_args(void *unused, jerry_value_t argv[], u32_t argc);

#endif
