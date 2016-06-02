// Copyright (c) 2016, Intel Corporation.

#include "jerry-api.h"

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
