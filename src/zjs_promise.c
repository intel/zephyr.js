#include <zephyr.h>

#include <string.h>
#include "zjs_util.h"
#include "zjs_promise.h"
#include "zjs_callbacks.h"

struct promise {
    int32_t id;
    jerry_value_t then;         // Function registered from then()
    int32_t then_id;            // Callback ID for then JS callback
    jerry_value_t* then_args;   // Arguments for fulfilling promise
    uint32_t then_args_cnt;     // Number of arguments for then callback
    jerry_value_t catch;        // Function registered from catch()
    int32_t catch_id;           // Callback ID for catch JS callback
    jerry_value_t* catch_args;  // Arguments for rejecting promise
    uint32_t catch_args_cnt;    // Number of arguments for catch callback
    void* user_handle;
    zjs_post_promise_func post;
};

// Bits track promises in use
static uint64_t use = 0;

#define BIT_IS_SET(i, b)  ((i) & (1 << (b)))
#define BIT_SET(i, b)     ((i) |= (1 << (b)))
#define BIT_CLR(i, b)     ((i) &= ~(1 << (b)))

static int find_id(uint64_t* use)
{
    int id = 0;
    while (BIT_IS_SET(*use, id)) {
        ++id;
    }
    BIT_SET(*use, id);
    return id;
}

char* make_name(int32_t id, char* module)
{
    // It is assumed that the string being created will be less than 20 characters
    // e.g. promise-1, promise-10, promise-300, not promise-1234567891011121314
    //      Regardless, there is only the possibility of having 64 promises due
    //      to them being tracked with bits in a uint64_t.
    static char name[20];
    memcpy(name, module, strlen(module));
    name[strlen(module)] = '-';
    sprintf(name + strlen(module) + 1, "%ld", id);
    return name;
}

struct promise* new_promise(void)
{
    struct promise* new = task_malloc(sizeof(struct promise));
    memset(new, 0, sizeof(struct promise));
    new->catch_id = -1;
    new->then_id = -1;
    new->id = -1;
    return new;
}

// Dummy function for then/catch
static jerry_value_t null_function(const jerry_value_t function_obj_val,
                                   const jerry_value_t this_val,
                                   const jerry_value_t args_p[],
                                   const jerry_length_t args_cnt)
{
    return ZJS_UNDEFINED;
}

static void post_promise(void* h, jerry_value_t* ret_val)
{
    struct promise* handle = (struct promise*)h;

    jerry_release_value(handle->then);
    jerry_release_value(handle->catch);
}

static void promise_free(const uintptr_t native_p)
{
    struct promise* handle = (struct promise*)native_p;
    if (handle) {
        task_free(handle);
    }
}

static jerry_value_t* pre_then(void* h, uint32_t* args_cnt)
{
    struct promise* handle = (struct promise*)h;

    *args_cnt = handle->then_args_cnt;
    return handle->then_args;
}

static jerry_value_t promise_then(const jerry_value_t function_obj_val,
                                   const jerry_value_t this_val,
                                   const jerry_value_t args_p[],
                                   const jerry_length_t args_cnt)
{
    struct promise* handle;

    jerry_value_t promise_obj = zjs_get_property(this_val, "promise");
    jerry_get_object_native_handle(promise_obj, (uintptr_t*)&handle);

    if (jerry_value_is_function(args_p[0])) {
        jerry_release_value(handle->then);
        handle->then = jerry_acquire_value(args_p[0]);
        zjs_edit_js_func(handle->then_id, handle->then);
        // Return the promise so it can be used by catch()
        return this_val;
    } else {
        return ZJS_UNDEFINED;
    }
}

static jerry_value_t* pre_catch(void* h, uint32_t* args_cnt)
{
    struct promise* handle = (struct promise*)h;

    *args_cnt = handle->catch_args_cnt;
    return handle->catch_args;
}

static jerry_value_t promise_catch(const jerry_value_t function_obj_val,
                                   const jerry_value_t this_val,
                                   const jerry_value_t args_p[],
                                   const jerry_length_t args_cnt)
{
    struct promise* handle;

    jerry_value_t promise_obj = zjs_get_property(this_val, "promise");
    jerry_get_object_native_handle(promise_obj, (uintptr_t*)&handle);

    if (jerry_value_is_function(args_p[0])) {
        jerry_release_value(handle->catch);
        handle->catch = jerry_acquire_value(args_p[0]);
        zjs_edit_js_func(handle->catch_id, handle->catch);
    }
    return ZJS_UNDEFINED;
}

int32_t zjs_make_promise(jerry_value_t obj, zjs_post_promise_func post, void* handle)
{
    struct promise* new = new_promise();
    jerry_value_t promise_obj = jerry_create_object();
    jerry_value_t global_obj = jerry_get_global_object();

    zjs_obj_add_function(obj, promise_then, "then");
    // TODO: Jerryscript does not like a function to be called "catch"
    zjs_obj_add_function(obj, promise_catch, "docatch");
    jerry_set_object_native_handle(promise_obj, (uintptr_t)new, promise_free);

    new->id = find_id(&use);
    new->user_handle = handle;
    new->post = post;

    // Add the "promise" object to the object passed as a property, because the
    // object being made to a promise may already have a native handle.
    zjs_obj_add_object(obj, promise_obj, "promise");
    zjs_obj_add_object(global_obj, obj, make_name(new->id, "promise"));

    jerry_release_value(global_obj);

    return new->id;
}

void zjs_fulfill_promise(int32_t id, jerry_value_t args[], uint32_t args_cnt)
{
    struct promise* handle;
    jerry_value_t global_obj = jerry_get_global_object();
    jerry_value_t val = zjs_get_property(global_obj, make_name(id, "promise"));
    jerry_value_t promise_obj = zjs_get_property(val, "promise");

    jerry_get_object_native_handle(promise_obj, (uintptr_t*)&handle);

    // Put *something* here in case it never gets registered
    handle->then = jerry_create_external_function(null_function);
    handle->then_id = zjs_add_callback(handle->then, handle, pre_then, post_promise);
    handle->then_args = args;
    handle->then_args_cnt = args_cnt;

    zjs_signal_callback(handle->then_id);

    jerry_release_value(global_obj);
}

void zjs_reject_promise(int32_t id, jerry_value_t args[], uint32_t args_cnt)
{
    struct promise* handle;
    jerry_value_t global_obj = jerry_get_global_object();
    jerry_value_t val = zjs_get_property(global_obj, make_name(id, "promise"));
    jerry_value_t promise_obj = zjs_get_property(val, "promise");

    jerry_get_object_native_handle(promise_obj, (uintptr_t*)&handle);

    // Put *something* here in case it never gets registered
    handle->catch = jerry_create_external_function(null_function);
    handle->catch_id = zjs_add_callback(handle->catch, handle, pre_catch, post_promise);
    handle->catch_args = args;
    handle->catch_args_cnt = args_cnt;

    zjs_signal_callback(handle->catch_id);

    jerry_release_value(global_obj);
}
