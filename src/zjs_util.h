// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_util_h__
#define __zjs_util_h__

// The util code is only for the X86 side

#include "jerry-api.h"
#include "zjs_common.h"

struct zjs_callback;

typedef void (*zjs_cb_wrapper_t)(struct zjs_callback *);

struct zjs_callback {
    void *fifo_reserved;
    jerry_object_t *js_callback;
    // function called from task context to execute the callback
    zjs_cb_wrapper_t call_function;
    // embed this within your own struct to add data fields you need
};

// TODO: We may want to reuse the queue code on ARC side at some point, and move
//   this to zjs_common
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
void zjs_obj_add_number(jerry_object_t *obj, double value, const char *name);

bool zjs_obj_get_boolean(jerry_object_t *obj, const char *name, bool *bval);
bool zjs_obj_get_string(jerry_object_t *obj, const char *name, char *buffer,
                        int len);
bool zjs_obj_get_double(jerry_object_t *obj, const char *name, double *num);
bool zjs_obj_get_uint32(jerry_object_t *obj, const char *name, uint32_t *num);

bool zjs_strequal(const jerry_string_t *jstr, const char *str);

void zjs_init_value_object(jerry_value_t *out_value_p, jerry_object_t *v);
void zjs_init_value_string(jerry_value_t *out_value_p, const char *v);

bool zjs_hex_to_byte(char *buf, uint8_t *byte);

int zjs_identity(int num);

#endif  // __zjs_util_h__
