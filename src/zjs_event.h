// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __zjs_event_h__
#define __zjs_event_h__

// ZJS includes
#include "zjs_util.h"

// This needs to be big enough to fit all the args that any zjs_pre_emit
// function wants to use, so may need to be increased in the future. An error
// message in emit_event_callback should let you know that you've exceeded the
// limit.
#define MAX_EVENT_ARGS 2

/**
 * Callback prototype for before an event is emitted
 *
 * The callback is responsible for setting up the arguments
 *
 * @param handle        Event user handle provided to zjs_make_event
 * @param argv          Arg array w/ max of four args to be set up
 * @param argc          Pointer to arg count to be set (default 0)
 * @param buffer        Data provided to zjs_defer_emit_event from which to
 *                        construct arguments
 * @param length        Length of buffer
 */
typedef void (*zjs_pre_emit)(void *handle, jerry_value_t argv[], u32_t *argc,
                             const char *buffer, u32_t length);

/**
 * Callback prototype for after an event is emitted
 *
 * @param handle        Event user handle provided to zjs_make_event
 * @param argv          Arg array w/ max of four args to be cleaned up
 * @param argc          Arg count
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
 * @param handle        Handle given to zjs_make_event()
 */
typedef void (*zjs_event_free)(void *handle);

/**
 * Turn an object into an event object. After this call the object will have
 * all the event functions like addListener(), on(), etc. This object can also
 * be used to trigger events in C. If the object needs no other prototype, pass
 * undefined and the event emitter prototype will be used. If a prototype is
 * given, it will be used as the object's prototype but its prototype in turn
 * will be set to the event emitter prototype.
 *
 * @param obj           Object to turn into an event object
 * @param prototype     Object to decorate and use as prototype, or undefined
 * @param user_handle   A handle the caller can get back later
 * @param free_cb       A callback for cleanup related to user_handle
 */
void zjs_make_event(jerry_value_t obj, jerry_value_t prototype,
                    void *user_handle, zjs_event_free free_cb);

/**
 * Get back the user handle for this event supplied to zjs_make_event
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
 * Emit an event from a callback on the main thread
 *
 * This can be called from interrupt context or another thread, so to avoid any
 * JerryScript calls, it takes data in a simple buffer format. This could be a
 * struct that the caller understands. When we switch over to the main thread
 * and prepare to emit the event, we will call the supplied 'pre' function that
 * will turn that data into a JerryScript argument list. The 'post' function
 * gives a chance to release references to them.
 *
 * @param obj           Object that contains the event to be triggered
 * @param event         Name of event
 * @param buffer        Data needed to call event listeners
 * @param bytes         Size of buffer
 * @param pre           Arg setup function called before event emitted
 * @param post          Arg teardown function called after event emitted
 *
 * @return              True if there were listeners
 */
void zjs_defer_emit_event(jerry_value_t obj, const char *event,
                          const void *buffer, int bytes,
                          zjs_pre_emit pre, zjs_post_emit post);

/**
 * Call any registered event listeners immediately
 *
 * @param obj           Object that contains the event to be triggered
 * @param event         Name of event
 * @param argv          Arguments to give to the event listeners as parameters
 * @param argc          Number of arguments
 *
 * @return              True if there were listeners called
 */
bool zjs_emit_event(jerry_value_t obj, const char *event_name,
                    const jerry_value_t argv[], u32_t argc);

// emit helpers

/**
 * Copies one jerry_value_t from buffer to argv[0]
 *
 * A zjs_pre_emit callback.
 */
void zjs_copy_arg(void *unused, jerry_value_t argv[], u32_t *argc,
                  const char *buffer, u32_t bytes);

/**
 * Releases the jerry_value_t's in argv
 *
 * A zjs_post_emit callback.
 */
void zjs_release_args(void *unused, jerry_value_t argv[], u32_t argc);

/**
 * Initialize the event module
 *
 * @return              Event constructor
 */
jerry_value_t zjs_event_init();

/** Release resources held by the event module */
void zjs_event_cleanup();

#endif
