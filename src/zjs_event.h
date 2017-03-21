// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __zjs_event_h__
#define __zjs_event_h__

#include "zjs_util.h"

/**
 * Callback prototype for after an event is triggered
 *
 * @param handle        Handle given to zjs_trigger_event()
 */
typedef void (*zjs_post_event)(void *handle);

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
 */
void zjs_make_event(jerry_value_t obj, jerry_value_t prototype);

/**
 * Add a new event listener to an event object.
 *
 * @param obj           Object to add listener to
 * @param event         Name of new/existing event
 * @param listener      Function to be called when the event is triggered
 */
void zjs_add_event_listener(jerry_value_t obj, const char *event,
                            jerry_value_t listener);

/**
 * Trigger an event
 *
 * FIXME: We need to describe how the ownership of args values works; it appears
 *        maybe the caller needs to keep them live (acquired) and then release
 *        them in post; or could we simplify this by acquiring them ourselves
 *        here and releasing our copies later? Then the caller would just
 *        release theirs immediately after the zjs_trigger_event call.
 *
 * @param obj           Object that contains the event to be triggered
 * @param event         Name of event
 * @param args          Arguments to give to the event listener as parameters
 * @param args_cnt      Number of arguments
 * @param post          Function to be called after the event is triggered
 * @param handle        A handle that is accessible in the 'post' call
 *
 * @return              True if there were listeners
 */
bool zjs_trigger_event(jerry_value_t obj, const char *event,
                       const jerry_value_t argv[], uint32_t argc,
                       zjs_post_event post, void *handle);

/**
 * Call any registered event listeners immediately
 *
 * @param obj           Object that contains the event to be triggered
 * @param event         Name of event
 * @param args          Arguments to give to the event listener as parameters
 * @param args_cnt      Number of arguments
 * @param post          Function to be called after the event is triggered
 * @param handle        A handle that is accessable in the 'post' call
 *
 * @return              True if there were listeners
 */
bool zjs_trigger_event_now(jerry_value_t obj, const char *event,
                           const jerry_value_t argv[], uint32_t argc,
                           zjs_post_event post, void *h);

/**
 * Initialize the event module
 *
 * @return              Event constructor
 */
jerry_value_t zjs_event_init();

/** Release resources held by the event module */
void zjs_event_cleanup();

#endif
