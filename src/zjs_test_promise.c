// Copyright (c) 2016-2017, Intel Corporation.

#ifdef BUILD_MODULE_TEST_PROMISE

#include "jerryscript.h"
#include "zjs_common.h"
#include "zjs_callbacks.h"
#include "zjs_util.h"

static ZJS_DECL_FUNC(create_promise)
{
    return jerry_create_promise();
}

static ZJS_DECL_FUNC(test_fulfill)
{
    // args: promise object
    ZJS_VALIDATE_ARGS(Z_OBJECT);

    if (argc > 1) {
        jerry_resolve_or_reject_promise(argv[0], argv[1], true);
    } else {
        jerry_resolve_or_reject_promise(argv[0], ZJS_UNDEFINED, true);
    }
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(test_reject)
{
    // args: promise object
    ZJS_VALIDATE_ARGS(Z_OBJECT);

    if (argc > 1) {
        jerry_resolve_or_reject_promise(argv[0], argv[1], false);
    } else {
        jerry_resolve_or_reject_promise(argv[0], ZJS_UNDEFINED, false);
    }
    return ZJS_UNDEFINED;
}


jerry_value_t zjs_test_promise_init(void)
{
    jerry_value_t test = jerry_create_object();
    zjs_obj_add_function(test, create_promise, "create_promise");
    zjs_obj_add_function(test, test_fulfill, "fulfill");
    zjs_obj_add_function(test, test_reject, "reject");

    return test;
}

void zjs_test_promise_cleanup()
{
}

#endif  // BUILD_MODULE_CONSOLE
