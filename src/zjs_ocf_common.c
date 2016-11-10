#ifdef BUILD_MODULE_OCF

#include "jerry-api.h"

#include "zjs_util.h"
#include "zjs_common.h"

#include "zjs_ocf_client.h"
#include "zjs_ocf_server.h"
#include "zjs_ocf_common.h"
#include "zjs_ocf_encoder.h"

#include "oc_api.h"
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include "port/oc_clock.h"

int zjs_ocf_is_int(jerry_value_t val) {
    int ret = 0;
    double n = jerry_get_number_value(val);
    ret = (n - (int) n == 0);
    if (ret) {
        // Integer type
        if (n < 0) {
            return TYPE_IS_INT;
        } else {
            return TYPE_IS_UINT;
        }
    } else {
        return TYPE_IS_NUMBER;
    }
}

static bool ocf_foreach_prop(const jerry_value_t prop_name,
                          const jerry_value_t prop_value,
                          void *data)
{
    struct props_handle* handle = (struct props_handle*)data;

    ZJS_GET_STRING(prop_name, name);

    // Skip id/resourcePath/resourceType because that is not a resource property that is sent out
    if ((strcmp(name, "deviceId") != 0) &&
        (strcmp(name, "resourcePath") != 0) &&
        (strcmp(name, "resourceType") != 0)) {
        handle->names_array[handle->size] = zjs_malloc(jerry_get_string_size(prop_name) + 1);
        memcpy(handle->names_array[handle->size], name, strlen(name));
        handle->names_array[handle->size][strlen(name)] = '\0';
        jerry_set_property_by_index(handle->props_array, handle->size++, prop_value);
    }

    return true;
}

/*
 * Turn a Jerry object into an array of its properties. The returned handle
 * will contain an array of strings with all the property names.
 */
static struct props_handle* ocf_get_all_properties(jerry_value_t resource,
                                                       jerry_value_t* prop_array,
                                                       uint32_t* prop_cnt)
{
    struct props_handle* handle = zjs_malloc(sizeof(struct props_handle));

    jerry_value_t keys_array = jerry_get_object_keys(resource);
    uint32_t arr_length = jerry_get_array_length(keys_array);
    jerry_value_t array = jerry_create_array(arr_length - 1);

    jerry_release_value(keys_array);

    handle->props_array = array;
    handle->size = 0;
    handle->names_array = zjs_malloc(sizeof(char*) * arr_length);

    jerry_foreach_object_property(resource, ocf_foreach_prop, handle);

    *prop_array = handle->props_array;
    *prop_cnt = handle->size;

    return handle;
}


/*
 * Takes an 'encoder' object and encodes properties found in 'props_object'. The
 * 'root' parameter should be true if this is being called on a root object,
 * otherwise it should be false and the encoder object should be provided.
 */
void* zjs_ocf_props_setup(jerry_value_t props_object,
                          CborEncoder* encoder,
                          bool root)
{
    void* ret = NULL;
    int i;
    jerry_value_t prop_arr;
    uint32_t count;
    struct props_handle* h = ocf_get_all_properties(props_object, &prop_arr, &count);
    ret = h;
    fflush(stdout);
    CborEncoder* enc;
    if (root) {
        enc = &root_map;
    } else {
        enc = encoder;
    }

    for (i = 0; i < count; ++i) {
        jerry_value_t prop = jerry_get_property_by_index(prop_arr, i);

        if (jerry_value_is_number(prop)) {
            // Number could be double, int, uint
            int type = zjs_ocf_is_int(prop);
            if (type == TYPE_IS_NUMBER) {
                double num = jerry_get_number_value(prop);
                zjs_rep_set_double(enc, h->names_array[i], num);
                DBG_PRINT("Encoding number: %lf\n", num);
            } else if (type == TYPE_IS_UINT) {
                unsigned int num = (unsigned int)jerry_get_number_value(prop);
                zjs_rep_set_uint(enc, h->names_array[i], num);
                DBG_PRINT("Encoding unsinged int: %u\n", num);
            } else if (type == TYPE_IS_INT) {
                int num = (int)jerry_get_number_value(prop);
                zjs_rep_set_int(enc, h->names_array[i], num);
                DBG_PRINT("Encoding int: %d\n", num);
            }
        } else if (jerry_value_is_boolean(prop)) {
            bool boolean = jerry_get_boolean_value(prop);
            zjs_rep_set_boolean(enc, h->names_array[i], boolean);
            DBG_PRINT("Encoding boolean: %d\n", boolean);
        } else if (jerry_value_is_string(prop)) {
            ZJS_GET_STRING(prop, str);
            zjs_rep_set_text_string(enc, h->names_array[i], str);
            DBG_PRINT("Encoding string: %s\n", str);
        } else if (jerry_value_is_object(prop) && !jerry_value_is_array(prop)) {
            CborEncoder child;
            DBG_PRINT("Encoding object: %s   {\n", h->names_array[i]);
            //jerry_value_t jobject = zjs_get_property(prop, h->names_array[i]);
            // Start child object
            zjs_rep_start_object(enc, &child);
            // Set child object property name
            zjs_rep_set_object(enc, &child, h->names_array[i]);
            // Recursively add new properties to child object
            ret = zjs_ocf_props_setup(prop, &child, false);
            // Close/End object
            zjs_rep_close_object(enc, &child);
            zjs_rep_end_object(enc, &child);
            DBG_PRINT("}\n");
        } else if (jerry_value_is_array(prop)) {
            CborEncoder child;
            DBG_PRINT("Encoding array: %s   [\n", h->names_array[i]);
            //jerry_value_t jarray = zjs_get_property(prop, h->names_array[i]);
            zjs_rep_set_array(enc, &child, h->names_array[i]);
            // Recursively add new properties to child object (array)
            ret = zjs_ocf_props_setup(prop, &child, false);
            // Close the array
            zjs_rep_close_array(enc, &child);
            DBG_PRINT("]\n");
        }

        jerry_release_value(prop);
    }

    return ret;
}

/*
 * Free the handle returned from get_all_ocf_properties()
 */
void zjs_ocf_free_props(void* h)
{
    struct props_handle* handle = (struct props_handle*)h;
    if (handle) {
        if (handle->names_array) {
            int i;
            for (i = 0; i < handle->size; ++i) {
                if (handle->names_array[i]) {
                    zjs_free(handle->names_array[i]);
                }
            }
            zjs_free(handle->names_array);
        }
        zjs_free(handle);
    }
}

/*
 * Must be defined for iotivity-constrained
 */
void oc_signal_main_loop(void)
{
}

// Probably can remove this
static void issue_requests(void)
{
    DBG_PRINT("issue_requests()\n");
}

static void app_init(void)
{
    oc_init_platform("Zephyr.js", NULL, NULL);
    oc_add_device("/oic/d", "oic.d.zephyrjs", "Zephyr.js Device", "1.0", "1.0", NULL, NULL);
}

void main_poll_routine(void* handle)
{
    oc_main_poll();
}

static const oc_handler_t handler = { .init = app_init,
                                      .signal_event_loop = oc_signal_main_loop,
                                      .requests_entry = issue_requests,
                                      .register_resources = zjs_ocf_register_resources
};

jerry_value_t zjs_ocf_init()
{
    int ret;

    ret = oc_main_init(&handler);
    if (ret < 0) {
        ERR_PRINT("error initializing, ret=%u\n", ret);
        return ZJS_UNDEFINED;
    }
    jerry_value_t ocf = jerry_create_object();
    jerry_value_t client = zjs_ocf_client_init();
    jerry_value_t server = zjs_ocf_server_init();

    zjs_set_property(ocf, "client", client);
    zjs_set_property(ocf, "server", server);

    return ocf;
}

#endif // BUILD_MODULE_OCF
