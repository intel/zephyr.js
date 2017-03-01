// Copyright (c) 2016-2017, Intel Corporation.

#ifndef SRC_ZJS_CALLBACKS_H_
#define SRC_ZJS_CALLBACKS_H_

#include "jerry-api.h"

#ifdef ZJS_32_BIT_CALLBACK_ID
typedef int32_t zjs_callback_id;
#else
typedef int16_t zjs_callback_id;
#endif

/*
 * Function that will be called BEFORE the JS function is called.
 * This should return an array of jerry_value_t's that contain
 * the function arguments for the JS function.
 *
 * @param handle        Module specific handle
 * @param argc          Number of arguments in the returned array
 *
 * @return              Pointer to array of jerry_value_t's
 */
typedef jerry_value_t* (*zjs_pre_callback_func)(void* handle, uint32_t* argc);

/*
 * Function that will be called AFTER the JS function is called.
 * This should do any cleanup/release of function arguments. This
 * also gives the module access to the value returned by the JS
 * function.
 *
 * @param handle        Module specific handle
 * @param ret_val[out]  Value returned by the JS function called
 */
typedef void (*zjs_post_callback_func)(void* handle, jerry_value_t* ret_val);

/*
 * Function definition for a C callback
 *
 * @param handle        Handle registered by zjs_add_c_callback()
 */
typedef void (*zjs_c_callback_func)(void* handle, void* args);

/*
 * Initialize the callback module
 */
void zjs_init_callbacks(void);

/*
 * Get the number of callback functions registered to this ID
 *
 * @param id            ID of callback list
 *
 * @return              Number of functions in the callback list
 */
int zjs_get_num_callbacks(zjs_callback_id id);

/*
 * Get the list of callback functions in a callback list
 *
 * @param id            ID of callback list
 * @param count         Number of functions
 *
 * @return              Array of functions
 */
jerry_value_t* zjs_get_callback_func_list(zjs_callback_id id, int* count);

/*
 * Remove a function from a list of callbacks
 *
 * @param id            Callback ID for the list
 * @param js_func       JS function to remove from list
 *
 * @return              True if function was removed, false if it did not exist
 */
bool zjs_remove_callback_list_func(zjs_callback_id id, jerry_value_t js_func);

/*
 * Create/add a function to a callback list. If the 'id' parameter is -1, a new
 * callback list will be created. If the 'id' parameter matches an existing
 * callback list, the JS callback function will be added to the list.
 *
 * @param js_func       JS function to be added to the callback list
 * @param handle        Module specific handle, given to pre/post
 * @param post          Function called after the JS function (explained above)
 * @param id            ID for this callback list (-1 if its a new list)
 *
 * @return              New callback ID for this list (or existing ID)
 */
zjs_callback_id zjs_add_callback_list(jerry_value_t js_func,
                                      jerry_value_t this,
                                      void* handle,
                                      zjs_post_callback_func post,
                                      zjs_callback_id id);

/*
 * Add/register a callback function
 *
 * @param js_func       JS function to be called (this could be native too)
 * @param handle        Module specific handle, given to pre/post
 * @param post          Function called after the JS function (explained above)
 *
 * @return              ID associated with this callback, use this ID to reference this CB
 */
zjs_callback_id zjs_add_callback(jerry_value_t js_func,
                                 jerry_value_t this,
                                 void* handle,
                                 zjs_post_callback_func post);

/*
 * Add a JS callback that will only get called once. After it is called it will
 * be automatically removed.
 *
 * @param js_func       JS function to be called (this could be native too)
 * @param handle        Module specific handle, given to pre/post
 * @param post          Function called after the JS function (explained above)
 *
 * @return              ID associated with this callback, use this ID to reference this CB
 */
zjs_callback_id zjs_add_callback_once(jerry_value_t js_func,
                                      jerry_value_t this,
                                      void* handle,
                                      zjs_post_callback_func post);

/*
 * Change a callbacks JS function
 *
 * @param id            ID of callback
 * @param func          New JS function
 *
 * @return              true if edit was successful
 */
bool zjs_edit_js_func(zjs_callback_id id, jerry_value_t func);

/*
 * Change a callback ID's native handle
 *
 * @param id            ID of callback
 * @param handle        New callback handle
 *
 * @return              true if edit was successful
 */
bool zjs_edit_callback_handle(zjs_callback_id id, void* handle);

/*
 * Remove a function that was registered by zjs_add_callback(). If you remove a
 * callback that has been signaled, but before it has been serviced it will
 * never get called.
 *
 * @param id            ID returned from zjs_add_callback
 */
void zjs_remove_callback(zjs_callback_id id);

/*
 * Remove all callbacks from memory
 */
void zjs_remove_all_callbacks();

/*
 * Signal the system to make a callback. The callback will not be called
 * immediately, but rather once the system has time to service the callback
 * module; this allows the system to fairly share CPU time as well as prevent
 * large recursion loops. Signaling a callback will cause the callback to be
 * called only once, and will NOT remove the callback from the list. You can
 * signal callbacks multiple times, but if the callback has not been serviced
 * between signaling, it will only get called once.
 *
 * For a JS callback, the arguments are of type jerry_value_t and they will be
 * acquired by the callback module and released when the callback fires. So the
 * caller should release its copies as usual.
 *
 * For a C callback, if there is a pointer among the arguments, it is opaque
 * to the callback module and must be managed by the caller and perhaps freed
 * in the post-callback function if appropriate.
 *
 * @param id            ID returned from zjs_add_callback
 * @param args          Arguments given to the JS/C callback
 * @param size          Size of arguments (in bytes)
 */
void zjs_signal_callback(zjs_callback_id id, const void *args, uint32_t size);

/*
 * Add/register a C callback
 *
 * @param handle        Module specific handle
 * @param callback      C callback to be called when signaled
 *
 * @return              ID for the C callback
 */
zjs_callback_id zjs_add_c_callback(void* handle, zjs_c_callback_func callback);

/*
 * Call a callback immediately. This should only be used when absolutely needed
 * and in a task context. If it is possible to use zjs_signal_callback(), use it.
 * Using this function could result in a large recursion stack because it does
 * not wait until the main loop to call the JS function.
 *
 * @param i             ID of callback
 * @param data          Callback arguments
 * @param sz            Size of callback arguments in bytes
 */
void zjs_call_callback(zjs_callback_id id, void* data, uint32_t sz);

/*
 * Service the callback module. Any callback's that have been signaled will
 * be serviced and the signal flag will be unset.
 *
 * @return              1 if any callbacks were processed
 *                      0 if no callbacks were processed
 */
uint8_t zjs_service_callbacks(void);

#endif /* SRC_ZJS_CALLBACKS_H_ */
