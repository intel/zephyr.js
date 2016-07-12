#ifndef SRC_ZJS_CALLBACKS_H_
#define SRC_ZJS_CALLBACKS_H_

#include "jerry-api.h"

/*
 * Function that will be called BEFORE the JS function is called.
 * This should return an array of jerry_value_t's that contain
 * the function arguments for the JS function.
 *
 * @param handle        Module specific handle
 * @param args_cnt      Number of arguments in the returned array
 *
 * @return              Pointer to array of jerry_value_t's
 */
typedef jerry_value_t* (*zjs_pre_callback_func)(void* handle, uint32_t* args_cnt);

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
typedef void (*zjs_c_callback_func)(void* handle);

/*
 * Initialize the callback module
 */
void zjs_init_callbacks(void);

/*
 * Add/register a callback function
 *
 * @param js_func       JS function to be called (this could be native too)
 * @param handle        Module specific handle, given to pre/post
 * @param pre           Function called before the JS function (explained above)
 * @param post          Function called after the JS function (explained above)
 *
 * @return              ID associated with this callback, use this ID to reference this CB
 */
int32_t zjs_add_callback(jerry_object_t *js_func, void* handle, zjs_pre_callback_func pre, zjs_post_callback_func post);

/*
 * Remove a function that was registered by zjs_add_callback(). If you remove a
 * callback that has been signaled, but before it has been serviced it will
 * never get called.
 *
 * @param id            ID returned from zjs_add_callback
 */
void zjs_remove_callback(int32_t id);

/*
 * Signal the system to make a callback. The callback will not be called
 * immediately, but rather once the system has time to service the callback
 * module; this allows the system to fairly share CPU time as well as prevent
 * large recursion loops. Signaling a callback will cause the callback to be
 * called only once, and will NOT remove the callback from the list. You can
 * signal callbacks multiple times, but if the callback has not been serviced
 * between signaling, it will only get called once.
 *
 * @param id            ID returned from zjs_add_callback
 */
void zjs_signal_callback(int32_t id);

/*
 * Add/register a C callback
 *
 * @param handle        Module specific handle
 * @param callback      C callback to be called when signaled
 *
 * @return              ID for the C callback
 */
int32_t zjs_add_c_callback(void* handle, zjs_c_callback_func callback);

/*
 * Service the callback module. Any callback's that have been signaled will
 * be serviced and the signal flag will be unset.
 */
void zjs_service_callbacks(void);

#endif /* SRC_ZJS_CALLBACKS_H_ */
