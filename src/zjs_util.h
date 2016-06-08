// Copyright (c) 2016, Intel Corporation.

#include "jerry-api.h"

struct zjs_callback;

typedef void (*zjs_cb_wrapper_t)(struct zjs_callback *);

struct zjs_callback {
    void *fifo_reserved;
    jerry_api_object_t *js_callback;
    // function called from task context to execute the callback
    zjs_cb_wrapper_t call_function;
    // embed this within your own struct to add data fields you need
};

void zjs_queue_init();
void zjs_queue_callback(struct zjs_callback *cb);
void zjs_run_pending_callbacks();

void zjs_obj_add_boolean(jerry_api_object_t *obj, bool value, const char *name);
void zjs_obj_add_function(jerry_api_object_t *obj, void *function,
                          const char *name);
void zjs_obj_add_object(jerry_api_object_t *parent, jerry_api_object_t *child,
                        const char *name);
void zjs_obj_add_string(jerry_api_object_t *obj, const char *value,
                        const char *name);
void zjs_obj_add_uint32(jerry_api_object_t *obj, uint32_t value,
                        const char *name);

bool zjs_obj_get_boolean(jerry_api_object_t *obj, const char *name,
                         bool *bval);
bool zjs_obj_get_string(jerry_api_object_t *obj, const char *name,
                        char *buffer, int len);
bool zjs_obj_get_uint32(jerry_api_object_t *obj, const char *name,
                        uint32_t *num);

bool zjs_strequal(const jerry_api_string_t *jstr, const char *str);

void zjs_init_api_value_object (jerry_api_value_t *out_value_p,
                                jerry_api_object_t *v);
void zjs_init_api_value_string (jerry_api_value_t *out_value_p,
                                const char *v);
