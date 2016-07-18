// Copyright (c) 2016, Intel Corporation.

// Zephyr includes
#include <zephyr.h>
#include <string.h>
#include <stdlib.h>

// ZJS includes
#include "zjs_modules.h"
#include "zjs_util.h"

struct modItem {
    const char *name;
    InitCB init;
    struct modItem *next;
};

static struct modItem *modList;

static jerry_value_t native_require_handler(const jerry_value_t function_obj_val,
                                            const jerry_value_t this_val,
                                            const jerry_value_t args_p[],
                                            const jerry_length_t args_cnt)
{
    char module[80];
    jerry_size_t sz;

    jerry_value_t arg = args_p[0];
    if (!jerry_value_is_string(arg)) {
        PRINT ("native_require_handler: invalid arguments\n");
        return zjs_error("native_require_handler: invalid argument");
    }

    sz = jerry_get_string_size(arg);
    int len = jerry_string_to_char_buffer(arg,
                                          (jerry_char_t *)module,
                                          sz);
    module[len] = '\0';


    if (modList) {
        struct modItem *t = modList;
        while (t) {
            if (!strcmp(t->name, module)) {
                jerry_value_t obj_val = jerry_acquire_value(t->init());
                return obj_val;
            }
            t = t->next;
        }
    }

    // Module is not in our list if it gets to this point.
    PRINT("native_require_handler: module `%s'  not found\n", module);
    return zjs_error("native_require_handler: module not found");
}

void zjs_modules_init()
{
    jerry_value_t global_obj = jerry_get_global_object();

    // create the C handler for require JS call
    zjs_obj_add_function(global_obj, native_require_handler, "require");
}

void zjs_modules_add(const char *name, InitCB cb)
{
    struct modItem *item = (struct modItem *)task_malloc(sizeof(struct modItem));
    if (!item) {
        PRINT("Error: out of memory!\n");
        exit(1);
    }

    item->name = name;
    item->init = cb;
    item->next = NULL;

    if (!modList) {
        modList = item;
        return;
    }

    struct modItem *t = modList;
    while (t->next)
        t = t->next;

    t->next = item;
}
