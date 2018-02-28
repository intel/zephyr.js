// Copyright (c) 2016-2018, Intel Corporation.

// C includes
#include <string.h>

// ZJS includes
#include "zjs_buffer.h"
#include "zjs_common.h"
#include "zjs_util.h"
#ifndef ZJS_LINUX_BUILD
#include "zjs_zephyr_port.h"
#endif
#ifdef ZJS_TRACE_MALLOC
#define MAX_LIST_SIZE 300
mem_stats_t mem_array[MAX_LIST_SIZE];
#endif

void *zjs_malloc_with_retry(size_t size)
{
    void *ptr = malloc(size);
    if (!ptr) {
        // see if stale JerryScript objects are holding memory
        jerry_gc();
        ptr = malloc(size);
    }
    return ptr;
}

#ifdef ZJS_TRACE_MALLOC
void zjs_print_mem_stats()
{
    // run garbage collection to get the cleanest output
    jerry_gc();

    for (int i = 0; i < MAX_LIST_SIZE; i++) {
        if (mem_array[i].ptr != NULL) {
            ZJS_PRINT("index %i %s - %s:%d: %p\n", i, mem_array[i].file,
                      mem_array[i].func, mem_array[i].line, mem_array[i].ptr);
        }
    }
}

void zjs_push_mem_stat(void *ptr, char *file, const char *func, int line)
{
    int i = 0;
    // Find an open spot
    while (mem_array[i].ptr != NULL && i < MAX_LIST_SIZE) {
        i++;
    }

    if (i >= MAX_LIST_SIZE) {
        ZJS_PRINT("No memory stat slots available\n");
        return;
    }

    mem_array[i].ptr = ptr;
    mem_array[i].file = file;
    mem_array[i].func = func;
    mem_array[i].line = line;
}

void zjs_pop_mem_stat(void *rm_ptr)
{
    if (rm_ptr != NULL) {
        for (int i = 0; i < MAX_LIST_SIZE; i++) {
            if (mem_array[i].ptr == rm_ptr) {
                mem_array[i].ptr = NULL;
                mem_array[i].file = "";
                mem_array[i].line = 0;
            }
        }
    }
}
#endif  // ZJS_TRACE_MALLOC

void zjs_set_property(const jerry_value_t obj, const char *name,
                      const jerry_value_t prop)
{
    // requires: obj is an object, name is a property name string
    //  effects: create a new field in the object named name, and
    //             set it to prop
    ZVAL jname = jerry_create_string((const jerry_char_t *)name);
    jerry_set_property(obj, jname, prop);
}

void zjs_set_readonly_property(const jerry_value_t obj, const char *name,
                               const jerry_value_t prop)
{
    // requires: obj is an object, name is a property name string
    //  effects: create a new readonly field in the object named name, and
    //             set it to prop
    ZVAL jname = jerry_create_string((const jerry_char_t *)name);
    jerry_property_descriptor_t pd;
    jerry_init_property_descriptor_fields(&pd);
    pd.is_writable_defined = true;
    pd.is_writable = false;
    pd.is_configurable_defined = true;
    pd.is_configurable = true;
    pd.is_value_defined = true;
    pd.value = prop;
    jerry_define_own_property(obj, jname, &pd);
    jerry_free_property_descriptor_fields(&pd);
}

jerry_value_t zjs_get_property(const jerry_value_t obj, const char *name)
{
    // requires: obj is an object, name is a property name string
    //  effects: looks up the property name in obj, and returns it; the value
    //             will be owned by the caller and must be released
    ZVAL jname = jerry_create_string((const jerry_char_t *)name);
    jerry_value_t rval = jerry_get_property(obj, jname);
    return rval;
}

bool zjs_delete_property(const jerry_value_t obj, const char *name)
{
    // requires: obj is an object, name is a property name string
    //  effects: looks up the property name in obj, and deletes it; return true
    //             if success, false otherwise
    ZVAL jname = jerry_create_string((const jerry_char_t *)name);
    bool rval = jerry_delete_property(obj, jname);
    return rval;
}

void zjs_obj_add_functions(jerry_value_t obj, zjs_native_func_t *funcs)
{
    // requires: obj is an existing JS object
    //           funcs is an array of zjs_native_func_t structs, terminated by a
    //             struct with a NULL function field
    //  effects: adds all of the described functions to obj with given names
    for (zjs_native_func_t *map = funcs; map->function; map++) {
        zjs_obj_add_function(obj, map->name, map->function);
    }
}

void zjs_obj_add_boolean(jerry_value_t obj, const char *name, bool flag)
{
    // requires: obj is an existing JS object
    //  effects: creates a new field in parent named name, set to value
    ZVAL jname = jerry_create_string((const jerry_char_t *)name);
    ZVAL jbool = jerry_create_boolean(flag);
    jerry_set_property(obj, jname, jbool);
}

void zjs_obj_add_readonly_boolean(jerry_value_t obj, const char *name,
                                  bool flag)
{
    // requires: obj is an existing JS object
    //  effects: creates a new readonly field in parent named name, set to value
    zjs_set_readonly_property(obj, name, jerry_create_boolean(flag));
}

void zjs_obj_add_function(jerry_value_t obj, const char *name, void *func)
{
    // requires: obj is an existing JS object, function is a native C function
    //  effects: creates a new field in object named name, that will be a JS
    //             JS function that calls the given C function

    // NOTE: The docs on this function make it look like func obj should be
    //   released before we return, but in a loop of 25k buffer creates there
    //   seemed to be no memory leak. Reconsider with future intelligence.
    ZVAL jname = jerry_create_string((const jerry_char_t *)name);
    ZVAL jfunc = jerry_create_external_function(func);
    if (jerry_value_is_function(jfunc)) {
        jerry_set_property(obj, jname, jfunc);
    }
}

void zjs_obj_add_object(jerry_value_t parent, const char *name,
                        jerry_value_t child)
{
    // requires: parent and child are existing JS objects
    //  effects: creates a new field in parent named name, that refers to child
    ZVAL jname = jerry_create_string((const jerry_char_t *)name);
    jerry_set_property(parent, jname, child);
}

void zjs_obj_add_string(jerry_value_t obj, const char *name, const char *str)
{
    // requires: obj is an existing JS object
    //  effects: creates a new field in parent named name, set to str
    ZVAL jname = jerry_create_string((const jerry_char_t *)name);
    ZVAL jstr = jerry_create_string((const jerry_char_t *)str);
    jerry_set_property(obj, jname, jstr);
}

void zjs_obj_add_readonly_string(jerry_value_t obj, const char *name,
                                 const char *str)
{
    // requires: obj is an existing JS object
    //  effects: creates a new readonly field in parent named name, set to str
    zjs_set_readonly_property(obj, name,
                              jerry_create_string((const jerry_char_t *)str));
}

void zjs_obj_add_number(jerry_value_t obj, const char *name, double num)
{
    // requires: obj is an existing JS object
    //  effects: creates a new field in parent named name, set to nval
    ZVAL jname = jerry_create_string((const jerry_char_t *)name);
    ZVAL jnum = jerry_create_number(num);
    jerry_set_property(obj, jname, jnum);
}

void zjs_obj_add_readonly_number(jerry_value_t obj, const char *name,
                                 double num)
{
    // requires: obj is an existing JS object
    //  effects: creates a new readonly field in parent named name, set to num
    zjs_set_readonly_property(obj, name, jerry_create_number(num));
}

bool zjs_obj_get_boolean(jerry_value_t obj, const char *name, bool *flag)
{
    // requires: obj is an existing JS object
    //  effects: retrieves field specified by name as a boolean
    ZVAL value = zjs_get_property(obj, name);
    bool rval = false;

    if (!jerry_value_has_error_flag(value) && jerry_value_is_boolean(value)) {
        *flag = jerry_get_boolean_value(value);
        rval = true;
    }

    return rval;
}

bool zjs_obj_get_string(jerry_value_t obj, const char *name, char *buffer,
                        int len)
{
    // requires: obj is an existing JS object, buffer can receive the string,
    //             len is its size
    //  effects: retrieves field specified by name; if it exists, and is a
    //             string, and can fit into the given buffer, copies it plus
    //             a null terminator into buffer and returns true; otherwise,
    //             returns false
    ZVAL value = zjs_get_property(obj, name);
    bool rval = false;

    if (!jerry_value_has_error_flag(value) && jerry_value_is_string(value)) {
        jerry_size_t size = len;
        zjs_copy_jstring(value, buffer, &size);
        if (size)
            rval = true;
    }

    return rval;
}

bool zjs_obj_get_double(jerry_value_t obj, const char *name, double *num)
{
    // requires: obj is an existing JS object
    //  effects: retrieves field specified by name as a uint32
    ZVAL value = zjs_get_property(obj, name);
    bool rval = false;

    if (!jerry_value_has_error_flag(value) && jerry_value_is_number(value)) {
        *num = jerry_get_number_value(value);
        rval = true;
    }

    return rval;
}

bool zjs_obj_get_uint32(jerry_value_t obj, const char *name, u32_t *num)
{
    // requires: obj is an existing JS object
    //  effects: retrieves field specified by name as a uint32
    ZVAL value = zjs_get_property(obj, name);
    bool rval = false;

    if (!jerry_value_has_error_flag(value) && jerry_value_is_number(value)) {
        *num = (u32_t)jerry_get_number_value(value);
        rval = true;
    }

    return rval;
}

bool zjs_obj_get_int32(jerry_value_t obj, const char *name, s32_t *num)
{
    // requires: obj is an existing JS object
    //  effects: retrieves field specified by name as a int32
    ZVAL value = zjs_get_property(obj, name);
    bool rval = false;

    if (!jerry_value_has_error_flag(value) && jerry_value_is_number(value)) {
        *num = (s32_t)jerry_get_number_value(value);
        rval = true;
    }

    return rval;
}

jerry_value_t zjs_push_array(jerry_value_t array, jerry_value_t val)
{
    if (!jerry_value_is_array(array)) {
        jerry_value_t new = jerry_create_array(1);
        jerry_set_property_by_index(new, 0, val);
        return new;
    } else {
        u32_t size = jerry_get_array_length(array);
        jerry_value_t new = jerry_create_array(size + 1);
        for (int i = 0; i < size; i++) {
            ZVAL v = jerry_get_property_by_index(array, i);
            jerry_set_property_by_index(new, i, v);
        }
        jerry_set_property_by_index(new, size, val);
        return new;
    }
}

void zjs_copy_jstring(jerry_value_t jstr, char *buffer, jerry_size_t *maxlen)
{
    jerry_size_t size = jerry_get_string_size(jstr);
    jerry_size_t len = 0;
    if (*maxlen > size)
        len = jerry_string_to_utf8_char_buffer(jstr, (jerry_char_t *)buffer,
                                               size);
    buffer[len] = '\0';
    *maxlen = len;
}

char *zjs_alloc_from_jstring(jerry_value_t jstr, jerry_size_t *maxlen)
{
    jerry_size_t size = jerry_get_string_size(jstr);
    char *buffer = zjs_malloc(size + 1);
    if (!buffer) {
        ERR_PRINT("allocation failed (%u bytes)\n", (unsigned int)(size + 1));
        return NULL;
    }

    jerry_size_t len;
    len = jerry_string_to_utf8_char_buffer(jstr, (jerry_char_t *)buffer, size);
    buffer[len] = '\0';

    if (maxlen) {
        if (*maxlen && *maxlen < len) {
            DBG_PRINT("string limited to %u bytes\n", (unsigned int)*maxlen);
            buffer[*maxlen] = '\0';
        } else {
            *maxlen = len;
        }
    }

    return buffer;
}

char *zjs_alloc_from_string(const char *str, size_t *maxlen)
{
    // if no max or string small enough, copy the whole string
    if (!maxlen || !*maxlen || strnlen(str, *maxlen) < *maxlen) {
        return (char *)strdup(str);
    }

    // otherwise, limit the size of the string
    char *buffer = zjs_malloc(*maxlen);
    memcpy(buffer, str, *maxlen - 1);
    buffer[*maxlen - 1] = '\0';
    return buffer;
}

bool zjs_hex_to_byte(const char *buf, u8_t *byte)
{
    // requires: buf is a string with at least two hex chars
    //  effects: converts the first two hex chars in buf to a number and
    //             returns it in *byte; returns true on success, false otherwise
    u8_t num = 0;
    for (int i = 0; i < 2; i++) {
        num <<= 4;
        if (buf[i] >= 'A' && buf[i] <= 'F')
            num += buf[i] - 'A' + 10;
        else if (buf[i] >= 'a' && buf[i] <= 'f')
            num += buf[i] - 'a' + 10;
        else if (buf[i] >= '0' && buf[i] <= '9')
            num += buf[i] - '0';
        else
            return false;
    }
    *byte = num;
    return true;
}

void zjs_default_convert_pin(u32_t orig, int *dev, int *pin)
{
    // effects: reads top three bits of the bottom byte of orig and writes them
    //            to dev and the bottom five bits and writes them to pin; thus
    //            up to eight devices are supported and up to 32 pins each; but
    //            if orig is 0xff, returns 0 for dev and -1 for pin
    if (orig == 0xff) {
        *dev = 0;
        *pin = -1;
    } else {
        *dev = (orig & 0xe0) >> 5;
        *pin = orig & 0x1f;
    }
}

// when accuracy isn't as important as space
u16_t zjs_compress_32_to_16(u32_t num)
{
    if (num == 0) {
        // GCC states that can't use __builtin_clz
        // if x is 0, because the result is undefined
        return 0;
    }

    int zeroes = __builtin_clz(num);

    if (zeroes >= 17)
        return (u16_t)num;

    // take the top 16 bits
    u16_t compressed = (num << zeroes) >> 16;

    // clear the bottom five bits to save leading zeroes
    compressed &= 0xffe0;

    // save the number of zeroes in bottom five bits
    return compressed | (u16_t)zeroes;
}

u32_t zjs_uncompress_16_to_32(u16_t num)
{
    if ((num & 0x8000) == 0)
        return (u32_t)num;

    // take top 11 bits
    u32_t uncompressed = (num & 0xffe0) >> 5;

    // recover the number of leading zeroes
    int zeroes = num & 0x1f;

    // shift back up to the right point
    return uncompressed << (21 - zeroes);
}

#ifdef ZJS_FIND_FUNC_NAME
typedef struct name_element {
    jerry_value_t obj;
    jerry_value_t name;
    struct name_element *next;
    struct name_element *parent;
} name_element_t;

typedef struct head_element {
    name_element_t *head;
    name_element_t *match;
    char *name;
    jerry_value_t obj;
} head_element_t;

static head_element_t *search_list = NULL;

static void search_helper(jerry_value_t obj, name_element_t *parent);

static bool foreach_prop(const jerry_value_t prop_name,
                         const jerry_value_t prop_value,
                         void *data)
{
    name_element_t *parent = (name_element_t *)data;
    jerry_value_t obj = search_list->obj;
    if (obj == prop_value) {
        // found
        search_list->name = zjs_alloc_from_jstring(prop_name, NULL);
        search_list->match = parent;
        return false;
    }
    // continue searching
    if (jerry_value_is_object(prop_value)) {
        // check for duplicate/circular references
        if (ZJS_LIST_FIND(name_element_t, search_list->head, obj, prop_value)) {
            // duplicate/circular reference found
            return true;
        }
        name_element_t *new = zjs_malloc(sizeof(name_element_t));
        new->obj = prop_value;
        new->name = jerry_acquire_value(prop_name);
        new->parent = parent;
        ZJS_LIST_PREPEND(name_element_t, search_list->head, new);
        search_helper(prop_value, new);
    }
    return true;
}

static void search_helper(jerry_value_t obj, name_element_t *parent)
{
    jerry_foreach_object_property(obj, foreach_prop, parent);
}

static char *create_js_path(char *obj_name, name_element_t *parent)
{
    // requires: obj_name is a null-terminated string
    int total = strlen(obj_name) + 1;

    if (!obj_name) {
        return NULL;
    }

    char *str = zjs_malloc(total);
    if (!str) {
        return NULL;
    }
    strcpy(str, obj_name);

    while (parent) {
        jerry_size_t size = 32;
        char name[size];
        zjs_copy_jstring(parent->name, name, &size);
        total += size + 1;
        char *newstr = zjs_malloc(total);
        if (!newstr) {
            break;
        }
        snprintf(newstr, total, "%s.%s", name, str);
        zjs_free(str);
        str = newstr;
        parent = parent->parent;
    }
    return str;
}

static void free_element(name_element_t *e)
{
    jerry_release_value(e->name);
    zjs_free(e);
}

static char *object_search(jerry_value_t obj, jerry_value_t start)
{
    // requires: jerry_value_t is the object being searched for; start is the
    //             top level object to start searching from (if not an object,
    //             e.g. 0, the global object will be used)
    //  effects: if obj can be found as property of start or a subobject,
    //             returns the "path" to the object from start
    search_list = zjs_malloc(sizeof(head_element_t));
    search_list->head = NULL;
    search_list->name = NULL;
    search_list->obj = obj;
    search_list->match = NULL;

    ZVAL global_obj = jerry_get_global_object();
    if (!jerry_value_is_object(start)) {
        start = global_obj;
    }
    search_helper(start, NULL);

    char *ret = NULL;
    if (search_list->name) {
        ret = create_js_path(search_list->name, search_list->match);
    }
    ZJS_LIST_FREE(name_element_t, search_list->head, free_element);
    zjs_free(search_list);
    return ret;
}
#endif

#define MAX_ERROR_NAME_LENGTH    32
#define MAX_ERROR_MESSAGE_LENGTH 128

void zjs_print_error_message(jerry_value_t error, jerry_value_t func)
{
    char *func_name = NULL;

#ifdef ZJS_FIND_FUNC_NAME
    ZVAL this_obj = zjs_get_property(error, "this");
    ZVAL err_func = zjs_get_property(error, "function");
    if (jerry_value_is_object(this_obj) && jerry_value_is_function(err_func)) {
        char *this_path = object_search(this_obj, 0);
        if (this_path) {
            char *func_part = object_search(err_func, this_obj);
            if (func_part) {
                int len = strlen(this_path) + strlen(func_part) + 2;
                func_name = zjs_malloc(len);
                if (func_name) {
                    snprintf(func_name, len, "%s.%s", this_path, func_part);
                }
                zjs_free(func_part);
            }
            zjs_free(this_path);
        }
        if (!func_name) {
            DBG_PRINT("function %snot found\n", "object context ");
        }
    }

    if (!func_name && jerry_value_is_function(err_func)) {
        func_name = object_search(err_func, 0);
        if (!func_name) {
            DBG_PRINT("function %snot found\n", "context ");
        }
    }

    if (!func_name && jerry_value_is_function(func)) {
        func_name = object_search(func, 0);
        if (!func_name) {
            DBG_PRINT("function %snot found\n", "");
        }
    }
    if (!func_name) {
        // see if a name property was set
        ZVAL name = zjs_get_property(func, ZJS_HIDDEN_PROP("function_name"));
        if (jerry_value_is_string(name)) {
            func_name = zjs_alloc_from_jstring(name, NULL);
        }
    }
#endif

    if (func_name) {
        ZJS_PRINT("In function %s():\n", func_name);
        zjs_free(func_name);
    }

    jerry_value_clear_error_flag(&error);
    ZVAL err_str_val = jerry_value_to_string(error);

    jerry_size_t size = 0;
    char *msg = zjs_alloc_from_jstring(err_str_val, &size);
    const char *err_str = msg;
    if (!msg) {
        err_str = "(Error message too long)";
    }
    ZJS_PRINT("%s\n", err_str);
    zjs_free(msg);
}

void zjs_free_value(const jerry_value_t *value)
{
    jerry_release_value(*value);
}

static bool zjs_true(const jerry_value_t value)
{
    // return true for any value
    return true;
}

// leave as non-static to avoid compile warning when not used
bool zjs_false(const jerry_value_t value)
{
    // return true for any value
    return true;
}

typedef bool (*zjs_type_test)(const jerry_value_t);

// NOTE: These must match the order of Z_ANY to Z_UNDEFINED in zjs_util.h
zjs_type_test zjs_type_map[] = {
    zjs_true,
    jerry_value_is_array,
    jerry_value_is_boolean,
    jerry_value_is_function,
    jerry_value_is_null,
    jerry_value_is_number,
    jerry_value_is_object,
    jerry_value_is_string,
#ifdef BUILD_MODULE_BUFFER
    zjs_value_is_buffer,
#else
    zjs_false,
#endif
    jerry_value_is_undefined
};

static int zjs_validate_arg(const char *expectation, jerry_value_t arg)
{
    // requires: If this argument is optional, first char of expectation must
    //             be '?'; then additional chars are one of "abflnosu", meaning
    //             array, boolean, float, nul(l), number, object, string or
    //             undefined, and it must be null-terminated.
    //  effects: returns a value from enum above
    bool optional = false;
    int index = 0;
    if (expectation[0] == Z_OPTIONAL[0]) {
        optional = true;
        ++index;
    }

    while (expectation[index] != '\0') {
        // NOTE: This relies on Z_ANY to Z_UNDEFINED being consecutive and
        //   Z_UNDEFINED being the last one
        int type_index = expectation[index] - Z_ANY[0];
        if (type_index < 0 || expectation[index] > Z_UNDEFINED[0]) {
            ERR_PRINT("invalid argument type: '%c'\n", expectation[index]);
            return ZJS_INTERNAL_ERROR;
        }

        if (zjs_type_map[type_index](arg)) {
            return optional ? ZJS_VALID_OPTIONAL : ZJS_VALID_REQUIRED;
        }

        ++index;
    }

    return optional ? ZJS_SKIP_OPTIONAL : ZJS_INVALID_ARG;
}

int zjs_validate_args(const char *expectations[], const jerry_length_t argc,
                      const jerry_value_t argv[])
{
    // effects: returns number of optional arguments found, or a negative
    //            number on error
    int expect_index = 0, arg_index = 0, opt_args = 0;
    while (expectations[expect_index] != NULL && arg_index < argc) {
        int rval = zjs_validate_arg(expectations[expect_index],
                                    argv[arg_index]);
        switch (rval) {
        case ZJS_VALID_OPTIONAL:
            ++opt_args;
        case ZJS_VALID_REQUIRED:
            ++arg_index;
        case ZJS_SKIP_OPTIONAL:
            ++expect_index;
            break;

        default:
            return rval;
        }
    }

    // check for any more required args
    while (expectations[expect_index] != NULL) {
        if (expectations[expect_index][0] != '?')
            return ZJS_INSUFFICIENT_ARGS;
        ++expect_index;
    }

    if (arg_index < argc) {
        DBG_PRINT("API received %u unexpected arg(s)\n",
                  (unsigned int)(argc - arg_index));
    }
    return opt_args;
}

#define ZJS_VALUE_INVALID -1
#define ZJS_VALUE_NOT_IN_MAP -2

int zjs_require_bool_if_prop(jerry_value_t obj, const char *prop, bool *result)
{
    ZVAL value = zjs_get_property(obj, prop);
    if (jerry_value_is_undefined(value) || jerry_value_has_error_flag(value)) {
        // not found; leave default
        return 0;
    }

    if (!jerry_value_is_boolean(value)) {
        return ZJS_VALUE_INVALID;
    }

    *result = jerry_get_boolean_value(value);
    return 0;
}

int zjs_require_string_if_prop_map(jerry_value_t obj, const char *prop,
                                   str2int_t map[], int maxlen, int *result)
{
    ZVAL value = zjs_get_property(obj, prop);
    if (jerry_value_is_undefined(value) || jerry_value_has_error_flag(value)) {
        // not found; leave default
        return 0;
    }

    if (!jerry_value_is_string(value)) {
        return ZJS_VALUE_INVALID;
    }

    char str[maxlen];
    jerry_size_t size = maxlen;
    zjs_copy_jstring(value, str, &size);
    for (int i = 0; map[i].first != NULL; ++i) {
        if (strequal(str, map[i].first)) {
            *result = map[i].second;
            return 0;
        }
    }
    return ZJS_VALUE_NOT_IN_MAP;
}

void free_handle_nop(void *h)
{
}

bool zjs_str_matches(char *str, char *array[])
{
    // requires: the final element of array must be NULL
    for (int i = 0; array[i]; ++i) {
        if (strequal(str, array[i])) {
            return true;
        }
    }
    return false;
}

#ifndef ZJS_LINUX_BUILD
#ifndef ZJS_ASHELL
static zjs_port_sem block;
void zjs_loop_unblock(void)
{
    zjs_port_sem_give(&block);
}

void zjs_loop_block(int time)
{
    zjs_port_sem_take(&block, time);
}

void zjs_loop_init(void)
{
    zjs_port_sem_init(&block, 0, 1);
}
#endif
#endif

#ifdef DEBUG_BUILD
void dump_buffer(const char *label, const void *buf, int len)
{
    ZJS_PRINT("%s: (%d bytes)\n", label, len);
    int cols = 16;
    int rows = (len + cols - 1) / cols;
    char *spaces = "  ";
    u8_t *u8buf = (u8_t *)buf;
    for (int i = 0; i < rows; i++) {
        ZJS_PRINT(" %x :", (unsigned int)(uintptr_t)(buf + i * cols));
        for (int j = 0; j < cols; j++) {
            if (i * cols + j >= len) {
                int left = (i + 1) * cols - len;
                int skip = 3 * left;
                if (left >= 8) {
                    skip += 1;
                }
                for (int k = 0; k < skip; k++) {
                    ZJS_PRINT(" ");
                }
                break;
            }
            char *prefix = spaces;
            if (j != 8) {
                prefix++;
            }
            ZJS_PRINT("%s%02x", prefix, u8buf[i * cols + j]);
        }
        ZJS_PRINT("  ");
        for (int j = 0; j < cols; j++) {
            if (i * cols + j >= len) {
                break;
            }
            char *prefix = spaces + 1;
            if (j != 8) {
                prefix++;
            }
            char c = u8buf[i * cols + j];
            if (c < 32) {
                c = '.';
            }
            ZJS_PRINT("%s%c", prefix, c);
        }
        ZJS_PRINT("\n");
    }
}
#endif
