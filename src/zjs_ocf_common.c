// Copyright (c) 2016, Intel Corporation.

#ifdef BUILD_MODULE_OCF

#include "jerryscript.h"

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
    struct props_handle *handle = (struct props_handle *)data;

    ZJS_GET_STRING(prop_name, name, OCF_MAX_PROP_NAME_LEN);

    /*
     * Use zjs_alloc_from_jstring
     */
    // Skip id/resourcePath/resourceType because that is not a resource property that is sent out
    if ((strcmp(name, "deviceId") != 0) &&
        (strcmp(name, "resourcePath") != 0) &&
        (strcmp(name, "resourceType") != 0)) {
        handle->names_array[handle->size] = zjs_malloc(jerry_get_string_size(prop_name) + 1);
        memcpy(handle->names_array[handle->size], name, strlen(name));
        handle->names_array[handle->size][strlen(name)] = '\0';
        ZVAL ret = jerry_set_property_by_index(handle->props_array,
                                               handle->size++, prop_value);
    }

    return true;
}

/*
 * Turn a Jerry object into an array of its properties. The returned handle
 * will contain an array of strings with all the property names.
 */
static struct props_handle *ocf_get_all_properties(jerry_value_t resource)
{
    struct props_handle *handle = zjs_malloc(sizeof(struct props_handle));

    ZVAL keys_array = jerry_get_object_keys(resource);
    uint32_t arr_length = jerry_get_array_length(keys_array);

    handle->props_array = jerry_create_array(arr_length - 1);
    handle->size = 0;
    handle->names_array = zjs_malloc(sizeof(char *) * arr_length);

    jerry_foreach_object_property(resource, ocf_foreach_prop, handle);

    return handle;
}


/*
 * Takes an 'encoder' object and encodes properties found in 'props_object'. The
 * 'root' parameter should be true if this is being called on a root object,
 * otherwise it should be false and the encoder object should be provided.
 */
void *zjs_ocf_props_setup(jerry_value_t props_object,
                          CborEncoder *encoder,
                          bool root)
{
    void *ret = NULL;
    int i;
    struct props_handle *h = ocf_get_all_properties(props_object);
    ret = h;
    fflush(stdout);
    CborEncoder *enc;
    if (root) {
        enc = &root_map;
    } else {
        enc = encoder;
    }

    for (i = 0; i < h->size; ++i) {
        ZVAL prop = jerry_get_property_by_index(h->props_array, i);

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
            ZJS_GET_STRING(prop, str, OCF_MAX_PROP_NAME_LEN);
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
            /*
             * TODO: need to start object?
             */
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
    }

    return ret;
}

/*
 * Free the handle returned from get_all_ocf_properties()
 */
void zjs_ocf_free_props(void *h)
{
    struct props_handle *handle = (struct props_handle *)h;
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
        jerry_release_value(handle->props_array);
        zjs_free(handle);
    }
}

jerry_value_t ocf_object;

/*
 * Must be defined for iotivity-constrained
 */
void oc_signal_main_loop(void)
{
}

#ifdef OC_CLIENT
// Probably can remove this
static void issue_requests(void)
{
    DBG_PRINT("issue_requests()\n");
}
#endif

#ifdef ZJS_LINUX_BUILD
#define CONFIG_DEVICE_NAME "ZJS Device"
#else
#define CONFIG_DEVICE_NAME CONFIG_BLUETOOTH_DEVICE_NAME
#endif

void zjs_set_uuid(char *uuid)
{
    ZVAL device = zjs_get_property(ocf_object, "device");
    if (!jerry_value_is_undefined(device)) {
        zjs_obj_add_string(device, uuid, "uuid");
    }
}

static void platform_init(void *data)
{
    uint32_t size;
    ZVAL platform = zjs_get_property(ocf_object, "platform");
    if (!jerry_value_is_undefined(platform)) {
        // osVersion
        ZVAL os_version = zjs_get_property(platform, "osVersion");
        if (!jerry_value_is_undefined(os_version) &&
            jerry_value_is_string(os_version)) {
            size = 8;
            char s[size];
            zjs_copy_jstring(os_version, s, &size);
            if (size) {
                zjs_rep_set_text_string(&root_map, "mnos", s);
            } else {
                ERR_PRINT("osVersion string is too long\n");
            }
        }

        // model
        ZVAL model = zjs_get_property(platform, "model");
        if (!jerry_value_is_undefined(model) &&
            jerry_value_is_string(model)) {
            size = 32;
            char s[size];
            zjs_copy_jstring(model, s, &size);
            if (size) {
                zjs_rep_set_text_string(&root_map, "mnmo", s);
            } else {
                ERR_PRINT("model string is too long\n");
            }
        }

        // manufacturerURL
        ZVAL manuf_url = zjs_get_property(platform, "manufacturerURL");
        if (!jerry_value_is_undefined(manuf_url) &&
            jerry_value_is_string(manuf_url)) {
            size = 64;
            char s[size];
            zjs_copy_jstring(manuf_url, s, &size);
            if (size) {
                zjs_rep_set_text_string(&root_map, "mnml", s);
            } else {
                ERR_PRINT("manufacturerURL string is too long\n");
            }
        }

        // manufacturerDate
        ZVAL manuf_date = zjs_get_property(platform, "manufacturerDate");
        if (!jerry_value_is_undefined(manuf_date) &&
            jerry_value_is_string(manuf_date)) {
            size = 12;
            char s[size];
            zjs_copy_jstring(manuf_date, s, &size);
            if (size) {
                zjs_rep_set_text_string(&root_map, "mndt", s);
            } else {
                ERR_PRINT("manufacturerDate string is too long\n");
            }
        }

        // platformVersion
        ZVAL plat_ver = zjs_get_property(platform, "platformVersion");
        if (!jerry_value_is_undefined(plat_ver) &&
            jerry_value_is_string(plat_ver)) {
            size = 8;
            char s[size];
            zjs_copy_jstring(plat_ver, s, &size);
            if (size) {
                zjs_rep_set_text_string(&root_map, "mnpv", s);
            } else {
                ERR_PRINT("platformVersion string is too long\n");
            }
        }

        // firmwareVersion
        ZVAL fw_ver = zjs_get_property(platform, "firmwareVersion");
        if (!jerry_value_is_undefined(fw_ver) &&
            jerry_value_is_string(fw_ver)) {
            size = 8;
            char s[size];
            zjs_copy_jstring(fw_ver, s, &size);
            if (size) {
                zjs_rep_set_text_string(&root_map, "mnfv", s);
            } else {
                ERR_PRINT("firmwareVersion string is too long\n");
            }
        }

        // supportURL
        ZVAL supp_url = zjs_get_property(platform, "supportURL");
        if (!jerry_value_is_undefined(supp_url) &&
            jerry_value_is_string(supp_url)) {
            size = 64;
            char s[size];
            zjs_copy_jstring(supp_url, s, &size);
            if (size) {
                zjs_rep_set_text_string(&root_map, "mnsl", s);
            } else {
                ERR_PRINT("supportURL string is too long\n");
            }
        }
    }
}

static int app_init(void)
{
    uint32_t size;
    // device props
    char *name = NULL;
    char *spec_version = NULL;
    char *data_model_version = NULL;

    // platform props
    char *manufacturer_name = NULL;

    ZVAL platform = zjs_get_property(ocf_object, "platform");
    if (!jerry_value_is_undefined(platform)) {
        ZVAL manuf_name = zjs_get_property(platform, "manufacturerName");
        if (!jerry_value_is_undefined(manuf_name) &&
            jerry_value_is_string(manuf_name)) {
            size = 32;
            manufacturer_name = zjs_alloc_from_jstring(manuf_name, &size);
            if (!manufacturer_name) {
                ERR_PRINT("manufacturerName is too long\n");
            }
        }
    }

    int ret = oc_init_platform((manufacturer_name) ? manufacturer_name : "ZJS",
                               platform_init,
                               NULL);
    if (manufacturer_name) {
        zjs_free(manufacturer_name);
    }
    ZVAL device  = zjs_get_property(ocf_object, "device");
    if (!jerry_value_is_undefined(device)) {
        ZVAL dev_name = zjs_get_property(device, "name");
        if (!jerry_value_is_undefined(dev_name) &&
            jerry_value_is_string(dev_name)) {
            size = 32;
            name = zjs_alloc_from_jstring(dev_name, &size);
            if (!name) {
                ERR_PRINT("name is too long\n");
            }
        }

        ZVAL spec_ver = zjs_get_property(device, "coreSpecVersion");
        if (!jerry_value_is_undefined(spec_ver) &&
            jerry_value_is_string(spec_ver)) {
            size = 8;
            spec_version = zjs_alloc_from_jstring(spec_ver, &size);
            if (!spec_version) {
                ERR_PRINT("coreSpecVersion is too long\n");
            }
        }

        /*
         * TODO: This property is defined as a "list of supported data models"
         *       Iotivity-constrained just allows a "data model version" to be
         *       inputed. For now a single "data model version" will be used.
         */
        ZVAL data_model_ver = zjs_get_property(device, "dataModels");
        if (!jerry_value_is_undefined(data_model_ver) &&
            jerry_value_is_string(data_model_ver)) {
            size = 8;
            data_model_version = zjs_alloc_from_jstring(data_model_ver, &size);
            if (!data_model_version) {
                ERR_PRINT("dataModels string is too long\n");
            }
        }
    }

    /*
     * TODO: uuid and url are automatically generated and cannot be set from JS
     */
    ret |= oc_add_device("/oic/d",
                         "oic.d.zjs",
                         (name) ? name : "ZJS Device",
                         (spec_version) ? spec_version : "1.0",
                         (data_model_version) ? data_model_version : "1.0",
                         NULL, NULL);
    if (name) {
        zjs_free(name);
    }
    if (spec_version) {
        zjs_free(spec_version);
    }
    if (data_model_version) {
        zjs_free(data_model_version);
    }
    return ret;
}

uint8_t main_poll_routine(void *handle)
{
    if (oc_main_poll()) {
        return 1;
    }
    return 0;
}

static const oc_handler_t handler = {
#ifdef OC_CLIENT
                                      .requests_entry = issue_requests,
#endif
#ifdef OC_SERVER
                                      .register_resources = zjs_ocf_register_resources,
#endif
                                      .init = app_init,
                                      .signal_event_loop = oc_signal_main_loop,
};

int zjs_ocf_start()
{
    int ret;

    ret = oc_main_init(&handler);
    if (ret < 0) {
        ERR_PRINT("error initializing, ret=%u\n", ret);
    }
    return ret;
}

jerry_value_t zjs_ocf_init()
{
    ocf_object = jerry_create_object();

    ZVAL device = ZJS_UNDEFINED;
    zjs_set_property(ocf_object, "device", device);

    ZVAL platform = ZJS_UNDEFINED;
    zjs_set_property(ocf_object, "platform", platform);

#ifdef OC_CLIENT
    ZVAL client = zjs_ocf_client_init();
    zjs_set_property(ocf_object, "client", client);
#endif

#ifdef OC_SERVER
    ZVAL server = zjs_ocf_server_init();
    zjs_set_property(ocf_object, "server", server);
#endif

#ifdef ZJS_CONFIG_BLE_ADDRESS
    zjs_obj_add_function(ocf_object, zjs_ocf_set_ble_address, "setBleAddress");
#endif
    return ocf_object;
}

void zjs_ocf_cleanup()
{
#ifdef OC_SERVER
    zjs_ocf_server_cleanup();
#endif
#ifdef OC_CLIENT
    zjs_ocf_client_cleanup();
#endif
    jerry_release_value(ocf_object);
}

#endif // BUILD_MODULE_OCF
