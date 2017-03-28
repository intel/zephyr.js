// Copyright (c) 2016-2017, Intel Corporation.

#include <string.h>
#include "zjs_util.h"
#include "zjs_common.h"
#include "zjs_promise.h"
#include "zjs_callbacks.h"
#include "zjs_modules.h"

static jerry_value_t promise_prototype;

// 25.4.1.6
static bool is_promise(jerry_value_t val)
{
    // 1. if Type(val) != Object
    if (!jerry_value_is_object(val)) {
        return false;
    }
    ZVAL name = jerry_create_string("PromiseState");
    // 2. if val.PromiseState does not exist
    if (!jerry_has_property(val, name)) {
        return false;
    }
    // 3.
    return true;
}

static jerry_value_t create_resolving_functions(jerry_value_t promise);

static void post_thenable_job(void *handle, jerry_value_t *ret)
{
    jerry_value_t resolving_functions = (jerry_value_t)((intptr_t)handle);
    ZVAL reject = zjs_get_property(resolving_functions, "Reject");

    // 3.
    if (jerry_value_has_error_flag(*ret)) {
        zjs_callback_id cb = zjs_add_callback_once(reject, ZJS_UNDEFINED, NULL, NULL);
        zjs_signal_callback(cb, &reject, sizeof(jerry_value_t));
    }
    jerry_release_value(resolving_functions);
}

// 25.4.2.2
static void promise_thenable_job(jerry_value_t promise, jerry_value_t resolution, jerry_value_t then_action)
{
    // 1. let resolving_functions == create_resolving_functions(promise)
    jerry_value_t resolving_functions = create_resolving_functions(promise);

    ZVAL resolve = zjs_get_property(resolving_functions, "Resolve");
    ZVAL reject = zjs_get_property(resolving_functions, "Reject");
    jerry_value_t argv[] = {resolve, reject};

    // 2.
    zjs_callback_id cb = zjs_add_callback_once(then_action, resolution, (void*)((intptr_t)resolving_functions), post_thenable_job);
    zjs_signal_callback(cb,argv, sizeof(jerry_value_t) * 2);
}

static void post_reaction_job(void *handle, jerry_value_t *ret)
{
    jerry_value_t promise_capability = (jerry_value_t)((intptr_t)handle);
    // 7.
    if (jerry_value_has_error_flag(*ret)) {
        // a.
        ZVAL reject = zjs_get_property(promise_capability, "Reject");
        zjs_callback_id cb = zjs_add_callback_once(reject, ZJS_UNDEFINED, NULL, NULL);
        zjs_signal_callback(cb, ret, sizeof(jerry_value_t));
    } else {
        // 8.
        ZVAL resolve = zjs_get_property(promise_capability, "Resolve");
        zjs_callback_id cb = zjs_add_callback_once(resolve, ZJS_UNDEFINED, NULL, NULL);
        zjs_signal_callback(cb, ret, sizeof(jerry_value_t));
    }
    jerry_release_value(promise_capability);
}

// 25.4.2.1
static void promise_reaction_job(jerry_value_t reaction, const jerry_value_t value)
{
    // 1. Assert reaction is PromiseReaction record
    // 2.
    ZVAL promise_capability = zjs_get_property(reaction, "Capabilities");
    // 3.
    ZVAL handler = zjs_get_property(reaction, "Handler");
    if (jerry_value_is_string(handler)) {
        uint32_t size = 16;
        char str[size];
        zjs_copy_jstring(handler, str, &size);
        if (strcmp(str, "Identity") == 0) {
            // 4. If handler == "Identity"
            // 8.
            ZVAL resolve = zjs_get_property(promise_capability, "Resolve");
            zjs_callback_id cb = zjs_add_callback_once(resolve, ZJS_UNDEFINED, (void*)NULL, NULL);
            zjs_signal_callback(cb, &value, sizeof(jerry_value_t));
        } else {
            // 5. If handler == "Thrower"
            ZVAL reject = zjs_get_property(promise_capability, "Reject");
            zjs_callback_id cb = zjs_add_callback_once(reject, ZJS_UNDEFINED, (void*)NULL, NULL);
            zjs_signal_callback(cb, &value, sizeof(jerry_value_t));
        }
    } else {
        // 6.
        zjs_callback_id cb = zjs_add_callback_once(handler, ZJS_UNDEFINED, (void*)((intptr_t)jerry_acquire_value(promise_capability)), post_reaction_job);
        zjs_signal_callback(cb, &value, sizeof(jerry_value_t));
    }
}

// 25.4.1.8
static jerry_value_t trigger_promise_reactions(jerry_value_t reactions, const jerry_value_t value)
{
    // 1. Enqueue jobs for each reaction
    if (!jerry_value_is_array(reactions)) {
        //ERR_PRINT("reactions is not array");
        return ZJS_UNDEFINED;
    }
    int i;
    uint32_t length = jerry_get_array_length(reactions);
    for (i = 0; i < length; ++i) {
        ZVAL r = jerry_get_property_by_index(reactions, i);
        promise_reaction_job(r, value);
    }
    // 2.
    return ZJS_UNDEFINED;
}

// 25.4.1.7
static jerry_value_t do_reject_promise(jerry_value_t promise, const jerry_value_t reason)
{
    // 1. Assert PromiseState == "pending"
    // 2.
    ZVAL reactions = zjs_get_property(promise, "PromiseRejectReactions");
    // 3.
    zjs_set_property(promise, "PromiseResult", reactions);
    // 4.
    zjs_set_property(promise, "PromiseFulfillReactions", ZJS_UNDEFINED);
    // 5.
    zjs_set_property(promise, "PromiseRejectReactions", ZJS_UNDEFINED);
    // 6.
    zjs_obj_add_string(promise, "rejected", "PromiseState");
    // 7.
    return trigger_promise_reactions(reactions, reason);
}

// 25.4.1.4
static jerry_value_t do_fulfill_promise(jerry_value_t promise, jerry_value_t value)
{
    // 1. Assert PromiseState == "pending"
    // 2.
    ZVAL reactions = zjs_get_property(promise, "PromiseFulfillReactions");
    // 3.
    zjs_set_property(promise, "PromiseResult", value);
    // 4.
    zjs_set_property(promise, "PromiseFulfillReactions", ZJS_UNDEFINED);
    // 5.
    zjs_set_property(promise, "PromiseRejectReactions", ZJS_UNDEFINED);
    // 6.
    zjs_obj_add_string(promise, "fulfilled", "PromiseState");
    // 7.
    return trigger_promise_reactions(reactions, value);
}

// 25.4.1.3.1
static jerry_value_t reject_function(const jerry_value_t function_obj,
                                     const jerry_value_t this,
                                     const jerry_value_t argv[],
                                     const jerry_length_t argc)
{
    ZVAL promise = zjs_get_property(function_obj, "Promise");
    bool already_resolved;
    zjs_obj_get_boolean(function_obj, "AlreadyResolved", &already_resolved);
    if (already_resolved == true) {
        return ZJS_UNDEFINED;
    }
    zjs_obj_add_boolean(function_obj, true, "AlreadyResolved");

    return do_reject_promise(promise, argv[0]);
}

// 25.4.1.3.2
static jerry_value_t resolve_function(const jerry_value_t function_obj,
                                      const jerry_value_t this,
                                      const jerry_value_t argv[],
                                      const jerry_length_t argc)
{
    // 2.
    ZVAL promise = zjs_get_property(function_obj, "Promise");
    if (!jerry_value_is_object(promise)) {
        ERR_PRINT("promise is not object\n");
    }

    // 3.
    bool already_resolved;
    zjs_obj_get_boolean(function_obj, "AlreadyResolved", &already_resolved);
    // 4.
    if (already_resolved == true) {
        return ZJS_UNDEFINED;
    }
    // 5.
    zjs_obj_add_boolean(function_obj, true, "AlreadyResolved");
    // 6.
    if (promise == argv[0]) {
        return do_reject_promise(promise, TYPE_ERROR("resolution == promise"));
    }
    // 7.
    if (argc < 1 ) {
        return do_fulfill_promise(promise, ZJS_UNDEFINED);
    } else if (!jerry_value_is_object(argv[0])) {
        return do_fulfill_promise(promise, argv[0]);
    }
    // 8.
    ZVAL then = zjs_get_property(argv[0], "then");
    // 9. // abrupt completion???
    // 10. thenAction = then
    // 11.
    if (!jerry_value_is_function(then)) {
        return do_fulfill_promise(promise, argv[0]);
    }
    // 12. // EnqueueJob
    promise_thenable_job(promise, argv[0], then);
    // 13.
    return ZJS_UNDEFINED;
}

// 25.4.1.3
static jerry_value_t create_resolving_functions(jerry_value_t promise)
{
    ZVAL already_resolved = jerry_create_boolean(false);
    ZVAL resolve = jerry_create_external_function(resolve_function);
    zjs_set_property(resolve, "Promise", promise);
    zjs_set_property(resolve, "AlreadyResolved", already_resolved);

    ZVAL reject = jerry_create_external_function(reject_function);
    zjs_set_property(reject, "Promise", promise);
    zjs_set_property(reject, "AlreadyResolved", already_resolved);

    jerry_value_t ret = jerry_create_object();
    zjs_set_property(ret, "Resolve", resolve);
    zjs_set_property(ret, "Reject", reject);

    return ret;
}

// 25.4.3.1
static jerry_value_t create_new_promise(jerry_value_t executor)
{
    // 1. if NewTarget == undefined throw TypeError
    // 2. if isCallable(executor) == false, throw TypeError
    if (!jerry_value_is_function(executor)) {
        return TYPE_ERROR("executor is not function");
    }
    // 3.
    jerry_value_t promise = jerry_create_object();
    jerry_set_prototype(promise, promise_prototype);

    // 4. ReturnIfAbrupt(promise)
    // 5.
    zjs_obj_add_string(promise, "pending", "PromiseState");
    // 6.
    //jerry_value_t fulfills = jerry_create_array(10);
    ZVAL fulfills = ZJS_UNDEFINED;
    zjs_set_property(promise, "PromiseFulfillReactions", fulfills);
    // 7.
    ZVAL rejects = ZJS_UNDEFINED;
    zjs_set_property(promise, "PromiseRejectReactions", rejects);
    // 8.
    ZVAL resolving_functions = create_resolving_functions(promise);
    zjs_set_property(promise, "resolvingFunctions", resolving_functions);

    ZVAL res = zjs_get_property(resolving_functions, "Resolve");
    ZVAL rej = zjs_get_property(resolving_functions, "Reject");

    jerry_value_t argv[] = {res, rej};

    ZVAL ret_val = jerry_call_function(executor, ZJS_UNDEFINED, argv, 2);

    if (jerry_value_has_error_flag(ret_val)) {
        ZVAL reject = zjs_get_property(resolving_functions, "Reject");
        zjs_callback_id cb = zjs_add_callback_once(reject, ZJS_UNDEFINED, NULL, NULL);
        zjs_signal_callback(cb, &ret_val, sizeof(jerry_value_t));
    }

    return promise;
}

static jerry_value_t promise_constructor(const jerry_value_t function_obj,
                                         const jerry_value_t this,
                                         const jerry_value_t argv[],
                                         const jerry_length_t argc)
{
    // args: promise function
    ZJS_VALIDATE_ARGS(Z_FUNCTION);

    return create_new_promise(argv[0]);
}

// 25.4.1.5.1
static jerry_value_t promise_executor(const jerry_value_t function_obj,
                                      const jerry_value_t this,
                                      const jerry_value_t argv[],
                                      const jerry_length_t argc)
{
    // 1. Assert F has Capability slot
    ZVAL name = jerry_create_string("Capability");
    if (!jerry_has_property(function_obj, name)) {
        return TYPE_ERROR("no Capability property");
    }
    // 2.
    ZVAL promise_capability = zjs_get_property(function_obj, "Capability");
    // 3.
    ZVAL resolve = zjs_get_property(promise_capability, "Resolve");
    if (!jerry_value_is_undefined(resolve)) {
        return TYPE_ERROR("no Resolve property");
    }
    // 4.
    ZVAL reject = zjs_get_property(promise_capability, "Reject");
    if (!jerry_value_is_undefined(reject)) {
        return TYPE_ERROR("no Reject property");
    }
    // 5.
    zjs_set_property(promise_capability, "Resolve", argv[0]);
    // 6.
    zjs_set_property(promise_capability, "Reject", argv[1]);

    // 7.
    return ZJS_UNDEFINED;
}

// 25.4.1.5
static jerry_value_t new_promise_capability(jerry_value_t c)
{
    // 1.
    if (!jerry_value_is_constructor(c)) {
        return TYPE_ERROR("value not constructor");
    }
    // 2.
    // 3.
    ZVAL promise_capability = jerry_create_object();
    zjs_set_property(promise_capability, "Promise", ZJS_UNDEFINED);
    zjs_set_property(promise_capability, "Resolve", ZJS_UNDEFINED);
    zjs_set_property(promise_capability, "Reject", ZJS_UNDEFINED);

    // 4.
    ZVAL executor = jerry_create_external_function(promise_executor);
    // 5.
    zjs_set_property(executor, "Capability", promise_capability);
    // 6.
    ZVAL promise = jerry_construct_object(c, &executor, 1);
    // 7. ReturnIfAbrupt(promise);
    if (!is_promise(promise)) {
        ZJS_PRINT("\n\nNOT PROMISE\n\n");
        return jerry_acquire_value(promise);
    }
    // 8.
    ZVAL resolve = zjs_get_property(promise_capability, "Resolve");
    if (!jerry_value_is_function(resolve)) {
        return TYPE_ERROR("Resolve is not a function");
    }
    // 9.
    ZVAL reject = zjs_get_property(promise_capability, "Reject");
    if (!jerry_value_is_function(reject)) {
        return TYPE_ERROR("Reject is not a function");
    }
    // 10.
    zjs_set_property(promise_capability, "Promise", promise);
    // 11.
    return jerry_acquire_value(promise_capability);
}

// 25.4.4.4
static jerry_value_t reject_promise(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc)
{
    // 1. Let C be this value
    // 2. If C is not object throw TypeError
    if (!jerry_value_is_object(this)) {
        return TYPE_ERROR("this is not object");
    }
    // 3.
    ZVAL promise_capability = new_promise_capability(this);
    // 4. ReturnIfAbrupt(promise_capability)
    if (jerry_value_has_error_flag(promise_capability)) {
        ZJS_PRINT("\n\npromise has error\n\n");
    }
    // 5.
    ZVAL reject = zjs_get_property(promise_capability, "Reject");
    zjs_callback_id cb = zjs_add_callback_once(reject, ZJS_UNDEFINED, NULL, NULL);
    zjs_signal_callback(cb, argv, sizeof(jerry_value_t));

    // 7.
    jerry_value_t promise = zjs_get_property(promise_capability, "Promise");

    return promise;
}

// 25.4.4.5
static jerry_value_t resolve_promise(const jerry_value_t function_obj,
                                     const jerry_value_t this,
                                     const jerry_value_t argv[],
                                     const jerry_length_t argc)
{
    // 1. Let C be this value
    // 2. If C is not object throw TypeError
    // 3. IsPromise(argv[0]) == true

    // 4.
    ZVAL promise_capability = new_promise_capability(this);
    // 5. ReturnIfAbrupt(promise_capability)
    // 6.
    ZVAL resolve = zjs_get_property(promise_capability, "Resolve");
    zjs_callback_id cb = zjs_add_callback_once(resolve, ZJS_UNDEFINED, NULL, NULL);
    zjs_signal_callback(cb, argv, sizeof(jerry_value_t));

    // 8.
    jerry_value_t promise = zjs_get_property(promise_capability, "Promise");

    return promise;
}

// 25.4.5.3.1
static jerry_value_t perform_promise_then(jerry_value_t promise, jerry_value_t onfulfilled, jerry_value_t onrejected, jerry_value_t capability)
{
    jerry_value_t set_fulfill;
    jerry_value_t set_reject;
    // 1.
    if (!is_promise(promise)) {
        return TYPE_ERROR("value is not promise");
    }
    // 2. Assert capability is PromiseCapability
    // 3.
    if (!jerry_value_is_function(onfulfilled)) {
        // a. let onFulfilled be "Identity"
        set_fulfill = jerry_create_string("Identity");
    } else {
        set_fulfill = onfulfilled;
    }
    // 4.
    if (!jerry_value_is_function(onrejected)) {
        // a. let onRejected be "Thrower"
        set_reject = jerry_create_string("Thrower");
    } else {
        set_reject = onrejected;
    }
    // 5.
    ZVAL fulfill_reaction = jerry_create_object();
    zjs_set_property(fulfill_reaction, "Capabilities", capability);
    zjs_set_property(fulfill_reaction, "Handler", set_fulfill);
    if (set_fulfill != onfulfilled) {
        jerry_release_value(set_fulfill);
    }
    // 6.
    ZVAL reject_reaction = jerry_create_object();
    zjs_set_property(reject_reaction, "Capabilities", capability);
    zjs_set_property(reject_reaction, "Handler", set_reject);
    if (set_reject != onrejected) {
        jerry_release_value(set_reject);
    }
    ZVAL promise_state = zjs_get_property(promise, "PromiseState");
    uint32_t size = 16;
    char state[size];
    zjs_copy_jstring(promise_state, state, &size);
    if (strcmp(state, "pending") == 0) {
        // 7.
        // a.
        ZVAL fulfill_reaction_array = zjs_get_property(promise, "PromiseFulfillReactions");
        if (jerry_value_is_undefined(fulfill_reaction_array)) {
            ZVAL array = jerry_create_array(1);
            jerry_set_property_by_index(array, 0, fulfill_reaction);
            zjs_set_property(promise, "PromiseFulfillReactions", array);
        } else if (jerry_value_is_array(fulfill_reaction_array)) {
            uint32_t length = jerry_get_array_length(fulfill_reaction_array);
            ZVAL array = jerry_create_array(length + 1);
            int i = 0;
            for (i = 0; i < length; ++i) {
                ZVAL val = jerry_get_property_by_index(fulfill_reaction_array, i);
                jerry_set_property_by_index(array, i, val);
            }
            jerry_set_property_by_index(array, length, fulfill_reaction);
            zjs_set_property(promise, "PromiseFulfillReactions", array);
        } else {
            ERR_PRINT("ARRAY NOT FOUND, ERROR\n");
        }

        // b.
        ZVAL reject_reaction_array = zjs_get_property(promise, "PromiseRejectReactions");
        if (jerry_value_is_undefined(reject_reaction_array)) {
            ZVAL array = jerry_create_array(1);
            jerry_set_property_by_index(array, 0, reject_reaction);
            zjs_set_property(promise, "PromiseRejectReactions", array);
        } else if (jerry_value_is_array(reject_reaction_array)) {
            uint32_t length = jerry_get_array_length(reject_reaction_array);
            ZVAL array = jerry_create_array(length + 1);
            int i = 0;
            for (i = 0; i < length; ++i) {
                ZVAL val = jerry_get_property_by_index(reject_reaction_array, i);
                jerry_set_property_by_index(array, i, val);
            }
            jerry_set_property_by_index(array, length, reject_reaction);
            zjs_set_property(promise, "PromiseRejectReactions", array);
        } else {
            ERR_PRINT("ARRAY NOT FOUND, ERROR\n");
        }
    } else if (strcmp(state, "fulfilled") == 0) {
        // 8.
        ZVAL value = zjs_get_property(promise, "PromiseResult");
        promise_reaction_job(fulfill_reaction, value);
    } else if (strcmp(state, "rejected") == 0) {
        // 9.
        ZVAL value = zjs_get_property(promise, "PromiseResult");
        promise_reaction_job(reject_reaction, value);
    }
    // 10.
    jerry_value_t ret = zjs_get_property(capability, "Promise");
    return ret;
}

// 25.4.5.3
static jerry_value_t do_promise_then(const jerry_value_t function_obj,
                                     const jerry_value_t this,
                                     const jerry_value_t argv[],
                                     const jerry_length_t argc)
{
    // 1. let promise be this
    // 2.
    if (!is_promise(this)) {
        return TYPE_ERROR("this is not promise object");
    }
    // 3. let C be SpeciesConstructor(promise, %Promise%)
    ZVAL constructor = jerry_create_external_function(promise_constructor);
    // 4. ReturnIfAbrupt(C)
    // 5.
    ZVAL result_capability = new_promise_capability(constructor);

    // 6. ReturnIfAbrupt(result_capability)
    // 7.
    if (argc < 2) {
        return perform_promise_then(this, argv[0], ZJS_UNDEFINED, result_capability);
    } else {
        return perform_promise_then(this, argv[0], argv[1], result_capability);
    }
}

// 25.4.5.1
static jerry_value_t do_promise_catch(const jerry_value_t function_obj,
                                      const jerry_value_t this,
                                      const jerry_value_t argv[],
                                      const jerry_length_t argc)
{
    jerry_value_t args[] = { ZJS_UNDEFINED, argv[0] };

    /*
     * TODO: may be better to use jerry_call_function() for this since this
     *       could not be overridable in JS.
     */
    return do_promise_then(function_obj, this, args, 2);
}

jerry_value_t zjs_make_promise(void)
{
    ZVAL constructor = jerry_create_external_function(promise_constructor);

    // 4. ReturnIfAbrupt(C)
    // 5.
    ZVAL result_capability = new_promise_capability(constructor);

    return zjs_get_property(result_capability, "Promise");
}

void zjs_fulfill_promise(jerry_value_t obj, const jerry_value_t arg)
{
    do_fulfill_promise(obj, arg);
}

void zjs_reject_promise(jerry_value_t obj, const jerry_value_t arg)
{
    do_reject_promise(obj, arg);
}

void zjs_init_promise()
{
    // Promise prototype
    zjs_native_func_t array[] = {
        { do_promise_then, "then" },
        { do_promise_catch, "catch" },
        { reject_promise, "reject" },
        { resolve_promise, "resolve" },
        { NULL, NULL }
    };

    ZVAL global_obj = jerry_get_global_object();

    promise_prototype = jerry_create_object();
    zjs_obj_add_functions(promise_prototype, array);

    ZVAL promise = jerry_create_external_function(promise_constructor);
    zjs_obj_add_function(promise, reject_promise, "reject");
    zjs_obj_add_function(promise, resolve_promise, "resolve");

    zjs_set_property(global_obj, "Promise", promise);
}
