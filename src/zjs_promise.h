#ifndef __zjs_promises_h__
#define __zjs_promises_h__

#include "zjs_util.h"

/*
 * Function called after a promise has been fulfilled or rejected
 *
 * @param handle        Handle given to zjs_make_promise()
 */
typedef void (*zjs_post_promise_func)(void* handle);

/*
 * Turn an object into a promise
 *
 * @param obj           Object to make a promise
 * @param post          Function to be called when the promise has been fulfilled/rejected
 * @param handle        Handle passed to post function
 *
 * @return              ID to reference this promise
 */
int32_t zjs_make_promise(jerry_value_t obj, zjs_post_promise_func post, void* handle);

/*
 * Fulfill a promise
 *
 * @param id            ID returned from zjs_make_promise()
 * @param args          Array of args that will be given to then()
 * @param args_cnt      Number of arguments in args
 */
void zjs_fulfill_promise(int32_t id, jerry_value_t args[], uint32_t args_cnt);

/*
 * Reject a promise
 *
 * @param id            ID returned from zjs_make_promise()
 * @param args          Array of args that will be given to catch()
 * @param args_cnt      Number of arguments in args
 */
void zjs_reject_promise(int32_t id, jerry_value_t args[], uint32_t args_cnt);

#endif /* __zjs_promises_h__ */
