#include "jerry-api.h"

#include "zjs_util.h"
#include "zjs_common.h"

#include "zjs_ocf_client.h"

#include "oc_api.h"
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
//#include "port/oc_signal_main_loop.h"
#include "port/oc_clock.h"

struct props_handle {
    jerry_value_t props_array;
    uint32_t size;
    char** names_array;
};

#define TYPE_IS_NUMBER 0
#define TYPE_IS_INT    1
#define TYPE_IS_UINT   2

// Helper to get the string from a jerry_value_t
#define ZJS_GET_STRING(jval, name) \
    int name##_sz = jerry_get_string_size(jval); \
    char name[name##_sz]; \
    int name##_len = jerry_string_to_char_buffer(jval, (jerry_char_t *)name, name##_sz); \
    name[name##_len] = '\0';

#define REJECT(promise, err_name, err_msg, handler) \
    handler = new_ocf_handler(NULL); \
    zjs_make_promise(promise, post_ocf_promise, handler); \
    handler->argv = zjs_malloc(sizeof(jerry_value_t)); \
    handler->argv[0] = make_ocf_error(err_name, err_msg, NULL); \
    zjs_reject_promise(promise, handler->argv, 1);

/*
 * Test if value at index is a double or integer
 *
 * @param val           JerryScript value to test
 *
 * @return              0=number, 1=int, 2=uint
 */
int zjs_ocf_is_int(jerry_value_t val);

/*
 * Encode all properties in props_object into a CborEncoder for an
 * iotivity-constrained callback.
 *
 * @param props_object  JerryScript object containing properties to encode
 * @param encoder       CborEncoder to encode the properties into
 * @param root          Flag if this is the root object or a sub-object
 *
 * @return              Handle to free the properties later
 */
void* zjs_ocf_props_setup(jerry_value_t props_object,
                          CborEncoder* encoder,
                          bool root);

/*
 * Free properties after a call to zjs_ocf_props_setup()
 *
 * @param h             Handler returned from zjs_ocf_props_setup()
 */
void zjs_ocf_free_props(void* h);

/*
 * Routine to call into iotivity-constrained
 */
void main_poll_routine(void* handle);

/*
 * Object returned from require('ocf')
 */
jerry_value_t zjs_ocf_init();
