// Copyright (c) 2016, Intel Corporation.

#include "jerry-api.h"

#if defined(CONFIG_STDOUT_CONSOLE)
#include <stdio.h>
#define PRINT           printf
#else
#include <misc/printk.h>
#define PRINT           printk
#endif

struct zjs_callback;

typedef void (*zjs_cb_wrapper_t)(struct zjs_callback *);

struct zjs_callback {
    void *fifo_reserved;
    jerry_object_t *js_callback;
    // function called from task context to execute the callback
    zjs_cb_wrapper_t call_function;
    // embed this within your own struct to add data fields you need
};

void zjs_queue_init();
void zjs_queue_callback(struct zjs_callback *cb);
void zjs_run_pending_callbacks();

void zjs_obj_add_boolean(jerry_object_t *obj, bool value, const char *name);
void zjs_obj_add_function(jerry_object_t *obj, void *function,
                          const char *name);
void zjs_obj_add_object(jerry_object_t *parent, jerry_object_t *child,
                        const char *name);
void zjs_obj_add_string(jerry_object_t *obj, const char *value,
                        const char *name);
void zjs_obj_add_uint32(jerry_object_t *obj, uint32_t value,
                        const char *name);

bool zjs_obj_get_boolean(jerry_object_t *obj, const char *name,
                         bool *bval);
bool zjs_obj_get_string(jerry_object_t *obj, const char *name,
                        char *buffer, int len);
bool zjs_obj_get_uint32(jerry_object_t *obj, const char *name,
                        uint32_t *num);

#define ZJS_IS_BOOL(jval) (jval.type == JERRY_DATA_TYPE_BOOLEAN)
#define ZJS_IS_FLOAT32(jval) (jval.type == JERRY_DATA_TYPE_FLOAT32)
#define ZJS_IS_OBJ(jval) (jval.type == JERRY_DATA_TYPE_OBJECT)
#define ZJS_IS_STRING(jval) (jval.type == JERRY_DATA_TYPE_STRING)

bool zjs_strequal(const jerry_string_t *jstr, const char *str);

void zjs_init_value_object(jerry_value_t *out_value_p, jerry_object_t *v);
void zjs_init_value_string(jerry_value_t *out_value_p, const char *v);

bool zjs_hex_to_byte(char *buf, uint8_t *byte);
