// Copyright (c) 2016-2018, Intel Corporation.

#ifdef BUILD_MODULE_OCF

// JerryScript includes
#include "jerryscript.h"

// C includes
#ifdef ZJS_LINUX_BUILD
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#endif

// ZJS includes
#include "zjs_common.h"
#include "zjs_util.h"

#include "zjs_net_config.h"
#include "zjs_ocf_client.h"
#include "zjs_ocf_common.h"
#include "zjs_ocf_encoder.h"
#include "zjs_ocf_server.h"

// OCF includes
#include "oc_api.h"
#include "port/oc_clock.h"

#ifdef DEBUG_BUILD
#include <stdarg.h>
static inline void PROPS_PRINT(const char *tag, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);

    if (tag) {
        printf("%s: ", tag);
    }

    vprintf(format, ap);
    va_end(ap);
}
#else
#define PROPS_PRINT(n, ...) do {} while (0)
#endif

#define TYPE_IS_NUMBER 0
#define TYPE_IS_INT    1
#define TYPE_IS_UINT   2

static int is_int(jerry_value_t val)
{
    int ret = 0;
    double n = jerry_get_number_value(val);
    ret = (n - (int)n == 0);
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

static void zjs_ocf_encode_value_helper(jerry_value_t object, const char *name,
                                        CborEncoder *encoder);

static bool foreach_prop(const jerry_value_t prop_name,
                         const jerry_value_t prop_value,
                         void *data)
{
    CborEncoder *encoder = (CborEncoder *)data;
    ZJS_GET_STRING(prop_name, name, OCF_MAX_PROP_NAME_LEN);

    zjs_ocf_encode_value_helper(prop_value, name, encoder);

    return true;
}

static void zjs_ocf_encode_value_helper(jerry_value_t object, const char *name,
                                        CborEncoder *encoder)
{
    if (jerry_value_is_object(object)) {
        CborEncoder child;
        if (jerry_value_is_array(object)) {
            int i;
            zjs_rep_start_array(encoder, &child);
            zjs_rep_set_array(encoder, &child, name);

            PROPS_PRINT(name, "[");
            u32_t len = jerry_get_array_length(object);
            for (i = 0; i < len; i++) {
                ZVAL element = jerry_get_property_by_index(object, i);
                zjs_ocf_encode_value_helper(element, NULL, &child);
            }
            PROPS_PRINT(NULL, "],");

            zjs_rep_close_array(encoder, &child);
            zjs_rep_end_array(encoder, &child);
        } else {
            zjs_rep_start_object(encoder, &child);
            zjs_rep_set_object(encoder, &child, name);

            PROPS_PRINT(name, "{");
            jerry_foreach_object_property(object, foreach_prop, &child);
            PROPS_PRINT(NULL, "},");

            zjs_rep_close_object(encoder, &child);
            zjs_rep_end_object(encoder, &child);
        }
    } else if (jerry_value_is_number(object)) {
        // Number could be double, int, uint
        int type = is_int(object);
        if (type == TYPE_IS_NUMBER) {
            double num = jerry_get_number_value(object);
            zjs_rep_set_double(encoder, name, num);
            PROPS_PRINT(name, "%lf,", num);
        } else if (type == TYPE_IS_UINT) {
            unsigned int num = (unsigned int)jerry_get_number_value(object);
            zjs_rep_set_uint(encoder, name, num);
            PROPS_PRINT(name, "%u,", num);
        } else if (type == TYPE_IS_INT) {
            int num = (int)jerry_get_number_value(object);
            zjs_rep_set_int(encoder, name, num);
            PROPS_PRINT(name, "%d,", num);
        }
    } else if (jerry_value_is_boolean(object)) {
        bool boolean = jerry_get_boolean_value(object);
        zjs_rep_set_boolean(encoder, name, boolean);
        PROPS_PRINT(name, "%s,", boolean ? "true" : "false");
    } else if (jerry_value_is_string(object)) {
        ZJS_GET_STRING(object, str, OCF_MAX_PROP_NAME_LEN);
        zjs_rep_set_text_string(encoder, name, str);
        PROPS_PRINT(name, "\"%s\",", str);
    }
}

void zjs_ocf_encode_value(jerry_value_t object)
{
    PROPS_PRINT(NULL, "Starting object encode: {\n");

    zjs_rep_start_root_object();

    jerry_foreach_object_property(object, foreach_prop, &root_map);

    zjs_rep_end_root_object();

    PROPS_PRINT(NULL, "\n} Ending object encode\n");
}

static jerry_value_t decode_value_helper(jerry_value_t object, oc_rep_t *data)
{
    u32_t sz;
    int i;
    oc_rep_t *rep = data;

    while (rep != NULL) {
        switch (rep->type) {
        case BOOL:
            zjs_obj_add_boolean(object, oc_string(rep->name),
                                rep->value.boolean);
            break;
        case INT:
            zjs_obj_add_number(object, oc_string(rep->name),
                               (double)rep->value.integer);
            break;
        case BYTE_STRING:
        case STRING:
            zjs_obj_add_string(object, oc_string(rep->name),
                               oc_string(rep->value.string));
            break;
        case DOUBLE:
            zjs_obj_add_number(object, oc_string(rep->name),
                               rep->value.double_p);
            break;
        case STRING_ARRAY: {
            sz = oc_string_array_get_allocated_size(rep->value.array);
            ZVAL array = jerry_create_array(sz);
            for (i = 0; i < sz; i++) {
                ZVAL str = jerry_create_string(
                    oc_string_array_get_item(rep->value.array, i));
                jerry_set_property_by_index(array, i, str);
            }
            zjs_set_property(object, oc_string(rep->name), array);
        } break;
        case DOUBLE_ARRAY: {
            sz = oc_double_array_size(rep->value.array);
            double *d_array = oc_double_array(rep->value.array);
            ZVAL array = jerry_create_array(sz);
            for (i = 0; i < sz; i++) {
                ZVAL d = jerry_create_number(d_array[i]);
                jerry_set_property_by_index(array, i, d);
            }
            zjs_set_property(object, oc_string(rep->name), array);
        } break;
        case INT_ARRAY: {
            sz = oc_int_array_size(rep->value.array);
            int *i_array = oc_int_array(rep->value.array);
            ZVAL array = jerry_create_array(sz);
            for (i = 0; i < sz; i++) {
                ZVAL integer = jerry_create_number(i_array[i]);
                jerry_set_property_by_index(array, i, integer);
            }
            zjs_set_property(object, oc_string(rep->name), array);
        } break;
        case BOOL_ARRAY: {
            sz = oc_bool_array_size(rep->value.array);
            bool *b_array = oc_bool_array(rep->value.array);
            ZVAL array = jerry_create_array(sz);
            for (i = 0; i < sz; i++) {
                ZVAL b = jerry_create_number(b_array[i]);
                jerry_set_property_by_index(array, i, b);
            }
            zjs_set_property(object, oc_string(rep->name), array);
        } break;
        case OBJECT: {
            jerry_value_t new_object = zjs_create_object();
            decode_value_helper(new_object, rep->value.object);
            if (!oc_string(rep->name)) {
                return new_object;
            }
            zjs_set_property(object, oc_string(rep->name), new_object);
            jerry_release_value(new_object);
        } break;
        case OBJECT_ARRAY: {
            oc_rep_t *iter = rep->value.object_array;
            jerry_value_t array = jerry_create_array(0);
            while (iter) {
                ZVAL element = decode_value_helper(ZJS_UNDEFINED, iter);
                array = zjs_push_array(array, element);
                iter = iter->next;
            }
            zjs_set_property(object, oc_string(rep->name), array);
            jerry_release_value(array);
        } break;
        default:
            break;
        }
        rep = rep->next;
    }

    return ZJS_UNDEFINED;
}

jerry_value_t zjs_ocf_decode_value(oc_rep_t *data)
{
    jerry_value_t object = zjs_create_object();
    decode_value_helper(object, data);

    return object;
}

jerry_value_t ocf_object;

/*
 * Must be defined for iotivity-constrained
 */
void oc_signal_main_loop(void)
{
#ifndef ZJS_LINUX_BUILD
    zjs_loop_unblock();
#endif
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
#define CONFIG_DEVICE_NAME CONFIG_BT_DEVICE_NAME
#endif

void zjs_set_uuid(char *uuid)
{
    ZVAL device = zjs_get_property(ocf_object, "device");
    if (!jerry_value_is_undefined(device)) {
        zjs_obj_add_string(device, "uuid", uuid);
    }
}

static void platform_init(void *data)
{
    jerry_size_t size;
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
        if (!jerry_value_is_undefined(model) && jerry_value_is_string(model)) {
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
    jerry_size_t size;
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
                               platform_init, NULL);
    if (manufacturer_name) {
        zjs_free(manufacturer_name);
    }
    ZVAL device = zjs_get_property(ocf_object, "device");
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
    ret |= oc_add_device("/oic/d", "oic.d.zjs", (name) ? name : "ZJS Device",
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

s32_t main_poll_routine(void *handle)
{
    return (s32_t)oc_main_poll();
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

static ZJS_DECL_FUNC(ocf_start)
{
#ifdef OC_DYNAMIC_ALLOCATION
    // The default app data size for dynamic allocation
    // is 8k, which is too high for Bluetooth 6lowpan
    // and cause random crashes. Reducing this to
    // a lower value like 1k will improve stability
    oc_set_max_app_data_size(MAX_APP_DATA_SIZE);
#endif

    if (oc_main_init(&handler) < 0) {
        return zjs_error("OCF failed to start");
    }
    return ZJS_UNDEFINED;
}

static void zjs_ocf_cleanup(void *native)
{
#ifdef OC_SERVER
    zjs_ocf_server_cleanup();
#endif
#ifdef OC_CLIENT
    zjs_ocf_client_cleanup();
#endif
    jerry_release_value(ocf_object);
}

static const jerry_object_native_info_t ocf_module_type_info = {
    .free_cb = zjs_ocf_cleanup
};

static jerry_value_t zjs_ocf_init()
{
    ocf_object = zjs_create_object();

    ZVAL device = ZJS_UNDEFINED;
    zjs_set_property(ocf_object, "device", device);

    ZVAL platform = ZJS_UNDEFINED;
    zjs_set_property(ocf_object, "platform", platform);

    zjs_obj_add_function(ocf_object, "start", ocf_start);
#ifdef OC_CLIENT
    ZVAL client = zjs_ocf_client_init();
    zjs_set_property(ocf_object, "client", client);
#endif

#ifdef OC_SERVER
    ZVAL server = zjs_ocf_server_init();
    zjs_set_property(ocf_object, "server", server);
#endif

#ifndef ZJS_LINUX_BUILD
#if defined(CONFIG_NET_L2_BT)
    // init BLE address
    zjs_init_ble_address();
#endif
#endif
    // Set up cleanup function for when the object gets freed
    jerry_set_object_native_pointer(ocf_object, NULL, &ocf_module_type_info);
    return ocf_object;
}

JERRYX_NATIVE_MODULE(ocf, zjs_ocf_init)
#endif  // BUILD_MODULE_OCF
