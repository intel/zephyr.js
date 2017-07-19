// Copyright (c) 2016-2017, Intel Corporation.

#ifndef SRC_ZJS_CALLBACKS_H_
#define SRC_ZJS_CALLBACKS_H_

#include "jerryscript.h"

// ZJS includes
#include "zjs_common.h"

typedef s16_t zjs_callback_id;

/*
 * Function that will be called AFTER the JS function is called.
 * This should do any cleanup/release of function arguments. This
 * also gives the module access to the value returned by the JS
 * function.
 *
 * @param handle        Module specific handle
 * @param ret_val[out]  Value returned by the JS function called
 */
typedef void (*zjs_post_callback_func)(void *handle, jerry_value_t ret_val);

/*
 * Function definition for a C callback
 *
 * @param handle        Handle registered by zjs_add_c_callback()
 */
typedef void (*zjs_c_callback_func)(void *handle, const void *args);

/*
 * Initialize the callback module
 */
void zjs_init_callbacks(void);

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
void zjs_remove_all_callbacks(void);

void signal_callback_priv(zjs_callback_id id,
                          const void *args,
                          u32_t size
#ifdef DEBUG_BUILD
                          ,
                          const char *file,
                          const char *func);
#else
                          );
#endif

#ifndef DEBUG_BUILD
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
#define zjs_signal_callback(id, args, size) signal_callback_priv(id, args, size)
#else
#define zjs_signal_callback(id, args, size) \
    signal_callback_priv(id, args, size, __FILE__, __func__)
#endif

zjs_callback_id add_callback_priv(jerry_value_t js_func,
                                  jerry_value_t this,
                                  void *handle,
                                  zjs_post_callback_func post,
                                  u8_t once
#ifdef DEBUG_BUILD
                                  ,
                                  const char *file,
                                  const char *func);
#else
                                  );
#endif

#ifndef DEBUG_BUILD
/*
* Add/register a callback function
*
* @param js_func       JS function to be called (this could be native too)
* @param handle        Module specific handle, given to pre/post
* @param post          Function called after the JS function (explained above)
*
* @return              ID associated with this callback, use this ID to
*                        reference this CB
*/
#define zjs_add_callback(func, this, handle, post) \
    add_callback_priv(func, this, handle, post, 0)

/*
* Add a JS callback that will only get called once. After it is called it will
* be automatically removed.
*
* @param js_func       JS function to be called (this could be native too)
* @param handle        Module specific handle, given to pre/post
* @param post          Function called after the JS function (explained above)
*
* @return              ID associated with this callback, use this ID to
*                        reference this CB
*/
#define zjs_add_callback_once(func, this, handle, post) \
    add_callback_priv(func, this, handle, post, 1);
#else
#define zjs_add_callback(func, this, handle, post) \
    add_callback_priv(func, this, handle, post, 0, __FILE__, __func__)
#define zjs_add_callback_once(func, this, handle, post) \
    add_callback_priv(func, this, handle, post, 1, __FILE__, __func__);
#endif

/*
 * Add/register a C callback
 *
 * @param handle        Module specific handle
 * @param callback      C callback to be called when signaled
 *
 * @return              ID for the C callback
 */
zjs_callback_id zjs_add_c_callback(void *handle, zjs_c_callback_func callback);

/*
 * Call a callback immediately. This should only be used when absolutely needed
 * and in a task context. If it is possible to use zjs_signal_callback(), use
 * it.
 * Using this function could result in a large recursion stack because it does
 * not wait until the main loop to call the JS function.
 *
 * @param i             ID of callback
 * @param data          Callback arguments
 * @param sz            Size of callback arguments in bytes
 */
void zjs_call_callback(zjs_callback_id id, const void *data, u32_t sz);

/*
 * Service the callback module. Any callback's that have been signaled will
 * be serviced and the signal flag will be unset.
 *
 * @return              1 if any callbacks were processed
 *                      0 if no callbacks were processed
 */
u8_t zjs_service_callbacks(void);

#endif /* SRC_ZJS_CALLBACKS_H_ */
