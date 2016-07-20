// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_util_h__
#define __zjs_util_h__

// The util code is only for the X86 side

#include "jerry-api.h"
#include "zjs_common.h"

#define ZJS_UNDEFINED jerry_create_undefined()

#ifdef DEBUG_BUILD
#define DBG_PRINT PRINT
#else
#define DBG_PRINT(fmat ...) do {} while(0);
#endif

struct zjs_callback;

typedef void (*zjs_cb_wrapper_t)(struct zjs_callback *);

struct zjs_callback {
    void *fifo_reserved;
    jerry_value_t js_callback;
    // function called from task context to execute the callback
    zjs_cb_wrapper_t call_function;
    // embed this within your own struct to add data fields you need
};

// TODO: We may want to reuse the queue code on ARC side at some point, and move
//   this to zjs_common
void zjs_queue_init();
void zjs_queue_callback(struct zjs_callback *cb);
void zjs_run_pending_callbacks();

void zjs_set_property(const jerry_value_t obj_val, const char *str_p,
                      const jerry_value_t prop_val);
jerry_value_t zjs_get_property (const jerry_value_t obj_val, const char *str_p);

void zjs_obj_add_boolean(jerry_value_t obj_val, bool value,
                         const char *name);
void zjs_obj_add_function(jerry_value_t obj_val, void *function,
                          const char *name);
void zjs_obj_add_object(jerry_value_t parent_val, jerry_value_t child_val,
                        const char *name);
void zjs_obj_add_string(jerry_value_t obj_val, const char *value,
                        const char *name);
void zjs_obj_add_number(jerry_value_t obj_val, double value,
                        const char *name);

bool zjs_obj_get_boolean(jerry_value_t obj_val, const char *name, bool *bval);
bool zjs_obj_get_string(jerry_value_t obj_val, const char *name, char *buffer,
                        int len);
bool zjs_obj_get_double(jerry_value_t obj_val, const char *name, double *num);
bool zjs_obj_get_uint32(jerry_value_t obj_val, const char *name, uint32_t *num);

bool zjs_strequal(const jerry_value_t str_val, const char *str);

bool zjs_hex_to_byte(char *buf, uint8_t *byte);

int zjs_identity(int num);

jerry_value_t zjs_error(const char *error);

#endif  // __zjs_util_h__
