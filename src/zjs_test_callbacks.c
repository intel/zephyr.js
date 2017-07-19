// Copyright (c) 2017, Intel Corporation.

// ZJS includes
#include "zjs_callbacks.h"
#include "zjs_common.h"
#include "zjs_util.h"

static ZJS_DECL_FUNC(add_callback)
{
    ZJS_VALIDATE_ARGS(Z_FUNCTION, Z_ANY);

    return jerry_create_number(zjs_add_callback(argv[0], argv[1], NULL, NULL));
}

static ZJS_DECL_FUNC(remove_callback)
{
    ZJS_VALIDATE_ARGS(Z_NUMBER);

    zjs_remove_callback(jerry_get_number_value(argv[0]));
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(signal_callback)
{
    ZJS_VALIDATE_ARGS(Z_NUMBER);

    zjs_callback_id id = jerry_get_number_value(argv[0]);
    if (argc > 1) {
        zjs_signal_callback(id, argv + 1, (argc - 1) * sizeof(jerry_value_t));
    } else {
        zjs_signal_callback(id, NULL, 0);
    }
    return ZJS_UNDEFINED;
}

jerry_value_t zjs_test_callbacks_init(void)
{
    jerry_value_t cb_obj = jerry_create_object();

    zjs_obj_add_function(cb_obj, add_callback, "addCallback");
    zjs_obj_add_function(cb_obj, signal_callback, "signalCallback");
    zjs_obj_add_function(cb_obj, remove_callback, "removeCallback");

    return cb_obj;
}
