#ifndef __zjs_event_h__
#define __zjs_event_h__

#include "zjs_util.h"

/*
 * Callback prototype for after an event is triggered
 *
 * @param handle        Handle given to zjs_trigger_event()
 */
typedef void (*zjs_post_event)(void* handle);

/*
 * Turn an object into an event object. After this call the object will have
 * all the event functions like addListener(), on(), etc. This object can also
 * be used to trigger events in C.
 *
 * @param obj           Object to turn into an event object
 */
void zjs_make_event(jerry_value_t obj);

/*
 * Add a new event listener to an event object.
 *
 * @param obj           Object to add listener to
 * @param event         Name of new/existing event
 * @param listener      Function to be called when the event is triggered
 */
void zjs_add_event_listener(jerry_value_t obj, const char* event, jerry_value_t listener);

/*
 * Trigger an event
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
bool zjs_trigger_event(jerry_value_t obj,
                       const char* event,
                       jerry_value_t args[],
                       uint32_t args_cnt,
                       zjs_post_event post,
                       void* handle);

/*
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
bool zjs_trigger_event_now(jerry_value_t obj,
                           const char* event,
                           jerry_value_t argv[],
                           uint32_t argc,
                           zjs_post_event post,
                           void* h);

/*
 * Initialize the event module
 *
 * @return              Event constructor
 */
jerry_value_t zjs_event_init();


#endif
