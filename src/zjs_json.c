// Copyright (c) 2016, Intel Corporation.

#ifdef BUILD_MODULE_JSON

#include "zjs_util.h"
#include "zjs_common.h"

#include <string.h>

#define JSON_MAX_STRING_SIZE    512

static uint16_t position = 0;
static char buffer[JSON_MAX_STRING_SIZE];

struct props_handle {
    jerry_value_t* element_array;
    uint32_t size;
    char** names_array;
};

#define GET_STRING(jval, name) \
    int name##_sz = jerry_get_string_size(jval); \
    char name[name##_sz + 1]; \
    memset(name, 0, name##_sz); \
    int name##_len = jerry_string_to_char_buffer(jval, (jerry_char_t *)name, name##_sz); \
    name[name##_len] = '\0';

static bool foreach_prop(const jerry_value_t prop_name,
                         const jerry_value_t prop_value,
                         void *data)
{
    struct props_handle* handle = (struct props_handle*)data;

    GET_STRING(prop_name, name);

    handle->names_array[handle->size] = zjs_malloc(jerry_get_string_size(prop_name) + 1);
    strncpy(handle->names_array[handle->size], name, strlen(name));
    handle->names_array[handle->size][strlen(name)] = '\0';

    handle->element_array[handle->size] = jerry_acquire_value(prop_value);

    handle->size++;

    return true;
}

static void stringify(jerry_value_t object)
{
    if (jerry_value_is_boolean(object)) {
        bool val = jerry_get_boolean_value(object);
        if (val) {
            memcpy(buffer + position, " true,", strlen(" true,"));
            position += strlen("true, ");
        } else {
            memcpy(buffer + position, " false,", strlen(" false,"));
            position += strlen("false, ");
        }
    } else if (jerry_value_is_number(object)) {
        double val = jerry_get_number_value(object);
        position += sprintf(buffer + position, " %lf,", val);
    } else if (jerry_value_is_string(object)) {
        GET_STRING(object, str);
        position += sprintf(buffer + position, " \"%s\",", str);
    } else if (jerry_value_is_object(object)) {
        char open_braces;
        char close_braces;
        int i;
        struct props_handle* handle = zjs_malloc(sizeof(struct props_handle));
        jerry_value_t keys_array = jerry_get_object_keys(object);
        uint32_t arr_length = jerry_get_array_length(keys_array);
        handle->element_array = zjs_malloc(sizeof(jerry_value_t) * arr_length);

        if (jerry_value_is_array(object)) {
            open_braces = '[';
            close_braces = ']';
        } else {
            open_braces = '{';
            close_braces = '}';
        }

        position += sprintf(buffer + position, " %c", open_braces);

        handle->size = 0;
        handle->names_array = zjs_malloc(sizeof(char*) * arr_length);

        jerry_foreach_object_property(object, foreach_prop, handle);

        for (i = 0; i < handle->size; ++i) {
            if (open_braces == '{') {
                position += sprintf(buffer + position, " %s:", handle->names_array[i]);
            }
            stringify(handle->element_array[i]);
        }
        position += sprintf(buffer + position, " %c", close_braces);

        for (i = 0; i < handle->size; ++i) {
            jerry_release_value(handle->element_array[i]);
            zjs_free(handle->names_array[i]);
        }
        zjs_free(handle->element_array);
        zjs_free(handle->names_array);
        zjs_free(handle);
    }
}

static jerry_value_t json_stringify(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc)
{
    position = 0;
    memset(buffer, 0, JSON_MAX_STRING_SIZE);
    stringify(argv[0]);

    return jerry_create_string((jerry_char_t*)buffer);
}

jerry_value_t zjs_json_init(void)
{
    jerry_value_t json = jerry_create_object();
    zjs_obj_add_function(json, json_stringify, "stringify");
    return json;
}

#endif

