// Copyright (c) 2016-2018, Intel Corporation.

#ifdef BUILD_MODULE_OCF

// C includes
#ifdef ZJS_LINUX_BUILD
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#endif

// JerryScript includes
#include "jerryscript.h"

// OCF includes
#include "oc_api.h"
#include "port/oc_clock.h"

// ZJS includes
#include "zjs_common.h"
#include "zjs_event.h"
#include "zjs_ocf_client.h"
#include "zjs_ocf_common.h"
#include "zjs_ocf_encoder.h"
#include "zjs_util.h"

//#define USE_PROMISES

typedef enum {
    RES_STATE_SEARCHING,
    RES_STATE_FOUND
} resource_state;

#define FLAG_OBSERVE        1 << 0
#define FLAG_DISCOVERABLE   1 << 1
#define FLAG_SLOW           1 << 2
#define FLAG_SECURE         1 << 3

jerry_value_t ocf_client = 0;

struct client_resource {
    char *device_id;
    char *resource_path;
    char *resource_type;
    jerry_value_t types_array;
    jerry_value_t iface_array;
    oc_endpoint_t *endpoint;
    resource_state state;
    u32_t flags;
    u32_t error_code;
    struct client_resource *next;
};

struct ocf_handler {
    jerry_value_t promise_obj;
    struct client_resource *res;
};

static struct client_resource *resource_list = NULL;

static struct ocf_handler *new_ocf_handler(struct client_resource *res)
{
    struct ocf_handler *h = zjs_malloc(sizeof(struct ocf_handler));
    if (!h) {
        ERR_PRINT("could not allocate OCF handle, out of memory\n");
        return NULL;
    }
    memset(h, 0, sizeof(struct ocf_handler));
    h->res = res;

    return h;
}

static jerry_value_t make_ocf_error(const char *name, const char *msg,
                                    struct client_resource *res)
{
    if (res) {
        ZVAL ret = zjs_create_object();
        if (name) {
            zjs_obj_add_string(ret, "name", name);
        } else {
            ERR_PRINT("error must have a name\n");
            return ZJS_UNDEFINED;
        }
        if (msg) {
            zjs_obj_add_string(ret, "message", msg);
        } else {
            ERR_PRINT("error must have a message\n");
            return ZJS_UNDEFINED;
        }
        if (res->device_id) {
            zjs_obj_add_string(ret, "deviceId", res->device_id);
        }
        if (res->resource_path) {
            zjs_obj_add_string(ret, "resourcePath", res->resource_path);
        }
        zjs_obj_add_number(ret, "errorCode", (double)res->error_code);

        return jerry_acquire_value(ret);
    } else {
        ERR_PRINT("client resource was NULL\n");
        return ZJS_UNDEFINED;
    }
}

/*
 * Combine a UUID and path to form a URI
 */
static char *create_url(const char *uuid, const char *path)
{
    // create URL of form oic://<uuid>/<path>
    if (path[0] == '/') {
        path++;
    }

    int url_len = strlen(uuid) + strlen(path) + 8;
    char *url = zjs_malloc(url_len);
    snprintf(url, url_len, "oic://%s/%s", uuid, path);
    return url;
}

#ifdef DEBUG_BUILD
#define do_print  \
    do_prefix(p); \
    ZJS_PRINT

static void do_prefix(int p)
{
    int i;
    for (i = 0; i < p; i++) {
        ZJS_PRINT("\t");
    }
}

static void print_props_data(int prefix, oc_rep_t *data)
{
    int p = prefix;
    int i;
    oc_rep_t *rep = data;
    while (rep != NULL) {
        switch (rep->type) {
        case BOOL:
            do_print("%s: %d\n", oc_string(rep->name), rep->value.boolean);
            break;
        case INT:
            do_print("%s: %d\n", oc_string(rep->name), rep->value.integer);
            break;
        case BYTE_STRING:
        case STRING:
            do_print("%s: %s\n", oc_string(rep->name),
                     oc_string(rep->value.string));
            break;
        case OBJECT:
            do_print("%s: {\n", oc_string(rep->name));
            print_props_data(p + 1, rep->value.object);
            do_print("}\n");
            break;
        case STRING_ARRAY:
            do_print("%s: [", oc_string(rep->name));
            int sz = oc_string_array_get_allocated_size(rep->value.array);
            for (i = 0; i < sz; i++) {
                ZJS_PRINT("\"%s\",",
                          oc_string_array_get_item(rep->value.array, i));
            }
            ZJS_PRINT("]\n");
            break;
        case INT_ARRAY:
            do_print("%s: [", oc_string(rep->name));
            int *i_array = oc_int_array(rep->value.array);
            for (i = 0; i < oc_int_array_size(rep->value.array); i++) {
                ZJS_PRINT("%u,", i_array[i]);
            }
            ZJS_PRINT("]\n");
            break;
        case DOUBLE_ARRAY:
            do_print("%s: [", oc_string(rep->name));
            double *d_array = oc_double_array(rep->value.array);
            for (i = 0; i < oc_double_array_size(rep->value.array); i++) {
                ZJS_PRINT("%lf,", d_array[i]);
            }
            ZJS_PRINT("]\n");
            break;
        case BOOL_ARRAY:
            do_print("%s: [", oc_string(rep->name));
            bool *b_array = oc_bool_array(rep->value.array);
            for (i = 0; i < oc_bool_array_size(rep->value.array); i++) {
                ZJS_PRINT("%d,", b_array[i]);
            }
            ZJS_PRINT("]\n");
            break;
        case OBJECT_ARRAY: {
            oc_rep_t *iter = rep->value.object_array;
            do_print("%s: [", oc_string(rep->name));
            p++;
            while (iter) {
                ZJS_PRINT("{\n");
                print_props_data(p + 1, iter->value.object);
                iter = iter->next;
                do_print("}");
                ZJS_PRINT(",");
            }
            ZJS_PRINT("\n");
            p--;
            do_print("]\n");
        } break;
        default:
            break;
        }
        rep = rep->next;
    }
}
#else
#define print_props_data(p, d) do {} while(0)
#endif

/*
 * Find a client_resource by searching with a device ID
 */
static struct client_resource *find_resource_by_id(const char *device_id)
{
    if (device_id) {
        struct client_resource *cur = resource_list;
        while (cur) {
            if (cur->state != RES_STATE_SEARCHING) {
                if (strequal(cur->device_id, device_id)) {
                    return cur;
                }
            }
            cur = cur->next;
        }
    }
    return NULL;
}

/*
 * Create a new resource object
 */
static jerry_value_t create_resource(struct client_resource *client)
{
    jerry_value_t resource = zjs_create_object();

    if (client->device_id) {
        zjs_obj_add_string(resource, "deviceId", client->device_id);
    }
    if (client->resource_path) {
        zjs_obj_add_string(resource, "resourcePath", client->resource_path);
    }
    ZVAL props = zjs_create_object();
    zjs_set_property(resource, "properties", props);

    zjs_set_property(resource, "resourceTypes", client->types_array);
    zjs_set_property(resource, "interfaces", client->iface_array);

    DBG_PRINT("id=%s, path=%s, obj number=%x\n", client->device_id,
              client->resource_path, resource);

    return resource;
}

// a zjs_event_free callback
static void client_free_cb(void *native)
{
    struct client_resource *cur = resource_list;
    while (cur) {
        if (cur->device_id) {
            zjs_free(cur->device_id);
        }
        if (cur->resource_type) {
            zjs_free(cur->resource_type);
        }
        if (cur->resource_path) {
            zjs_free(cur->resource_path);
        }
        jerry_release_value(cur->types_array);
        jerry_release_value(cur->iface_array);
        resource_list = resource_list->next;
        zjs_free(cur);
        cur = resource_list;
    }
}

/*
 * Add a discovered resource to the list of resource_list
 */
static void add_resource(char *id, char *type, char *path,
                         jerry_value_t listener)
{
    struct client_resource *new = zjs_malloc(sizeof(struct client_resource));

    memset(new, 0, sizeof(struct client_resource));
    new->state = RES_STATE_SEARCHING;

    if (id) {
        new->device_id = zjs_alloc_from_string(id, NULL);
    }
    if (type) {
        new->resource_type = zjs_alloc_from_string(type, NULL);
    }
    if (path) {
        new->resource_path = zjs_alloc_from_string(path, NULL);
    }

    if (jerry_value_is_function(listener)) {
        zjs_add_event_listener(ocf_client, "resourcefound", listener);
    }

    new->next = resource_list;
    resource_list = new;
}

/*
 * Callback to observe
 */
static void observe_callback(oc_client_response_t *data)
{
    if (data && data->user_data) {
        struct client_resource *resource =
            (struct client_resource *)data->user_data;
        jerry_value_t resource_val = create_resource(resource);
        ZVAL properties_val = zjs_ocf_decode_value(data->payload);
        zjs_set_property(resource_val, "properties", properties_val);

        zjs_defer_emit_event(ocf_client, "update", &resource_val,
                             sizeof(resource_val), zjs_copy_arg,
                             zjs_release_args);

#ifdef DEBUG_BUILD
        print_props_data(0, data->payload);
#endif
    }
}

/*
 * Callback for resource discovery
 */
static oc_discovery_flags_t discovery(const char *di,
                                      const char *uri,
                                      oc_string_array_t types,
                                      oc_interface_mask_t interfaces,
                                      oc_endpoint_t *endpoint,
                                      void *user_handle)
{
    struct ocf_handler *h = (struct ocf_handler *)user_handle;
    int i;

    for (i = 0; i < oc_string_array_get_allocated_size(types); i++) {
        char *t = oc_string_array_get_item(types, i);
        struct client_resource *cur = resource_list;
        while (cur) {
            if (cur->state == RES_STATE_SEARCHING) {
                // check if resource has any filter constraints
                if (cur->device_id) {
                    if (strequal(cur->device_id, di)) {
                        goto Found;
                    } else {
                        goto NotFound;
                    }
                }
                if (cur->resource_type) {
                    if (strequal(cur->resource_type, t)) {
                        goto Found;
                    } else {
                        goto NotFound;
                    }
                }
                if (cur->resource_path) {
                    if (strequal(cur->resource_path, uri)) {
                        goto Found;
                    } else {
                        goto NotFound;
                    }
                }
                // TODO: If there are no filters what is supposed to happen?
                goto NotFound;
            } else {
NotFound:
                cur = cur->next;
                continue;
            }
Found:
            cur->state = RES_STATE_FOUND;

            cur->endpoint = endpoint;

            if (!cur->device_id) {
                cur->device_id = zjs_alloc_from_string(di, NULL);
            }
            if (!cur->resource_path) {
                // TODO: before this code was pretending to limit things to a
                //   MAX_URI_LENGTH of 30, but it wasn't working right anyway;
                //   not sure if there was a point to that
                cur->resource_path = zjs_alloc_from_string(uri, NULL);
            }
            /*
             * Add the array of resource types to newly discovered resource
             */
            u32_t sz = oc_string_array_get_allocated_size(types);
            cur->types_array = jerry_create_array(sz);

            for (i = 0; i < sz; i++) {
                char *t = oc_string_array_get_item(types, i);
                ZVAL val = jerry_create_string(t);
                jerry_set_property_by_index(cur->types_array, i, val);
            }
            /*
             * Add array of interfaces
             */
            sz = 0;
            // count up set ifaces
            for (i = 1; i < 8; ++i) {
                if ((1 << i) & interfaces) {
                    sz++;
                }
            }
            cur->iface_array = jerry_create_array(sz);
            if (interfaces & OC_IF_BASELINE) {
                ZVAL val = jerry_create_string("oic.if.baseline");
                jerry_set_property_by_index(cur->iface_array, --sz, val);
            }
            if (interfaces & OC_IF_LL) {
                ZVAL val = jerry_create_string("oic.if.ll");
                jerry_set_property_by_index(cur->iface_array, --sz, val);
            }
            if (interfaces & OC_IF_B) {
                ZVAL val = jerry_create_string("oic.if.b");
                jerry_set_property_by_index(cur->iface_array, --sz, val);
            }
            if (interfaces & OC_IF_R) {
                ZVAL val = jerry_create_string("oic.if.r");
                jerry_set_property_by_index(cur->iface_array, --sz, val);
            }
            if (interfaces & OC_IF_RW) {
                ZVAL val = jerry_create_string("oic.if.rw");
                jerry_set_property_by_index(cur->iface_array, --sz, val);
            }
            if (interfaces & OC_IF_A) {
                ZVAL val = jerry_create_string("oic.if.a");
                jerry_set_property_by_index(cur->iface_array, --sz, val);
            }
            if (interfaces & OC_IF_S) {
                ZVAL val = jerry_create_string("oic.if.s");
                jerry_set_property_by_index(cur->iface_array, --sz, val);
            }

            // Work-around for #1332
            // Pass duplicate resource to trigger event and promise
            jerry_value_t res = create_resource(cur);
            ZVAL res2 = create_resource(cur);
            // FIXME: see if we can do without the second one now with the
            //   emit_event implementation
            zjs_defer_emit_event(ocf_client, "resourcefound", &res,
                                 sizeof(res), zjs_copy_arg, zjs_release_args);
            jerry_resolve_or_reject_promise(h->promise_obj, res2, true);
            jerry_release_value(h->promise_obj);

            DBG_PRINT("resource found, id=%s, path=%s\n", cur->device_id,
                      cur->resource_path);

            return OC_STOP_DISCOVERY;
        }
    }
    oc_free_server_endpoints(endpoint);
    return OC_CONTINUE_DISCOVERY;
}

static ZJS_DECL_FUNC(ocf_find_resources)
{
    // args: options object
    ZJS_VALIDATE_ARGS(Z_OPTIONAL Z_OBJECT, Z_OPTIONAL Z_FUNCTION);

    char *resource_type = NULL;
    char *device_id = NULL;
    char *resource_path = NULL;
    jerry_value_t listener = ZJS_UNDEFINED;

    if (argc > 0 && !jerry_value_is_function(argv[0])) {
        // has options parameter
        ZVAL device_id_val = zjs_get_property(argv[0], "deviceId");
        ZVAL res_type_val = zjs_get_property(argv[0], "resourceType");
        ZVAL res_path_val = zjs_get_property(argv[0], "resourcePath");

        if (jerry_value_is_string(device_id_val)) {
            jerry_size_t size = OCF_MAX_DEVICE_ID_LEN;
            device_id = zjs_alloc_from_jstring(device_id_val, &size);
            if (device_id)
                DBG_PRINT("deviceId: %s\n", device_id);
        }

        if (jerry_value_is_string(res_type_val)) {
            jerry_size_t size = OCF_MAX_RES_TYPE_LEN;
            resource_type = zjs_alloc_from_jstring(res_type_val, &size);
            if (resource_type)
                DBG_PRINT("resourceType: %s\n", resource_type);
        }

        if (jerry_value_is_string(res_path_val)) {
            jerry_size_t size = OCF_MAX_RES_PATH_LEN;
            resource_path = zjs_alloc_from_jstring(res_path_val, &size);
            if (resource_path)
                DBG_PRINT("resourcePath: %s\n", resource_path);
        }
    }

    if (jerry_value_is_function(argv[0])) {
        listener = argv[0];
    } else if (argc >= 2 && jerry_value_is_function(argv[1])) {
        listener = argv[1];
    }

    add_resource(device_id, resource_type, resource_path, listener);

    if (device_id) {
        zjs_free(device_id);
    }
    if (resource_path) {
        zjs_free(resource_path);
    }

    jerry_value_t promise = jerry_create_promise();
    struct ocf_handler *h = new_ocf_handler(NULL);
    h->promise_obj = jerry_acquire_value(promise);

    oc_do_ip_discovery(resource_type, discovery, h);

    if (resource_type) {
        zjs_free(resource_type);
    }

    return promise;
}

static void ocf_get_handler(oc_client_response_t *data)
{
    if (data && data->user_data) {
        struct ocf_handler *h = (struct ocf_handler *)data->user_data;
        if (h && h->res) {
            struct client_resource *resource = h->res;
            if (data->code == OC_STATUS_OK) {
                print_props_data(0, data->payload);

                ZVAL resource_val = create_resource(resource);
                ZVAL properties_val = zjs_ocf_decode_value(data->payload);

                zjs_set_property(resource_val, "properties", properties_val);
                jerry_resolve_or_reject_promise(h->promise_obj, resource_val,
                                                true);
                jerry_release_value(h->promise_obj);

                DBG_PRINT("GET response OK, device_id=%s\n",
                          resource->device_id);
            } else {
                // Reject promise
                /*
                 * TODO: change to use real errors
                 */
                ZVAL err = make_ocf_error("NetworkError", "Error code from GET",
                                          resource);
                jerry_resolve_or_reject_promise(h->promise_obj, err, false);
                jerry_release_value(h->promise_obj);

                ERR_PRINT("GET response code %d\n", data->code);
            }
        }
    }
}

static ZJS_DECL_FUNC(ocf_retrieve)
{
    // args: device id[, options][, listener]
    ZJS_VALIDATE_ARGS(Z_STRING, Z_OPTIONAL Z_OBJECT, Z_OPTIONAL Z_FUNCTION);

    jerry_value_t options = 0;
    jerry_value_t listener = 0;
    struct ocf_handler *h;

    ZJS_GET_STRING(argv[0], device_id, OCF_MAX_DEVICE_ID_LEN + 1);

    struct client_resource *resource = find_resource_by_id(device_id);
    if (!resource) {
        ERR_PRINT("could not find resource %s\n", device_id);
        REJECT("NotFoundError", "resource was not found");
    }

    if (argc > 1) {
        if (jerry_value_is_function(argv[1])) {
            listener = argv[1];
        } else {
            options = argv[1];
            if (argc > 2) {
                listener = argv[2];
            }
        }

        if (listener) {
            zjs_add_event_listener(this, "update", listener);
        }
    }

    if (options) {
        ZVAL observe_flag = zjs_get_property(options, "observable");
        if (jerry_value_is_boolean(observe_flag)) {
            bool val = jerry_get_boolean_value(observe_flag);
            if (val) {
                resource->flags |= FLAG_OBSERVE;
            }
        }

        ZVAL discover_flag = zjs_get_property(options, "discoverable");
        if (jerry_value_is_boolean(discover_flag)) {
            bool val = jerry_get_boolean_value(discover_flag);
            if (val) {
                resource->flags |= FLAG_DISCOVERABLE;
            }
        }

        ZVAL secure_flag = zjs_get_property(options, "secure");
        if (jerry_value_is_boolean(secure_flag)) {
            bool val = jerry_get_boolean_value(secure_flag);
            if (val) {
                resource->flags |= FLAG_SECURE;
            }
        }

        ZVAL slow_flag = zjs_get_property(options, "slow");
        if (jerry_value_is_boolean(slow_flag)) {
            bool val = jerry_get_boolean_value(slow_flag);
            if (val) {
                resource->flags |= FLAG_SLOW;
            }
        }
    }

    if (resource->flags & FLAG_OBSERVE) {
        oc_do_observe(resource->resource_path, resource->endpoint, NULL,
                      &observe_callback, LOW_QOS, resource);
    }

    DBG_PRINT("resource found in lookup: path=%s, id=%s\n",
              resource->resource_path, resource->device_id);

    jerry_value_t promise = jerry_create_promise();
    h = new_ocf_handler(resource);
    h->res = resource;
    h->promise_obj = jerry_acquire_value(promise);

    if (!oc_do_get(resource->resource_path, resource->endpoint, NULL,
                   ocf_get_handler, LOW_QOS, h)) {

        ZVAL err = make_ocf_error("NetworkError", "GET call failed", resource);
        jerry_resolve_or_reject_promise(promise, err, false);
        jerry_release_value(h->promise_obj);
    }

    return promise;
}

static void put_finished(oc_client_response_t *data)
{
    if (data) {
        struct ocf_handler *h = (struct ocf_handler *)data->user_data;
        if (h->res) {
            struct client_resource *resource = h->res;
            h->res->error_code = data->code;
            if (data->code == OC_STATUS_CHANGED) {
                DBG_PRINT("PUT response OK, device_id=%s\n",
                          resource->device_id);
                ZVAL resource_val = create_resource(resource);
                jerry_resolve_or_reject_promise(h->promise_obj, resource_val,
                                                true);
                jerry_release_value(h->promise_obj);
            } else {
                ERR_PRINT("PUT response code %d\n", data->code);
                ZVAL err = make_ocf_error("NetworkError",
                                          "PUT response error code", resource);
                jerry_resolve_or_reject_promise(h->promise_obj, err, false);
                jerry_release_value(h->promise_obj);
            }
        }
    }
}

static ZJS_DECL_FUNC(ocf_update)
{
    // args: resource object
    ZJS_VALIDATE_ARGS(Z_OBJECT);

    struct ocf_handler *h;

    // Get device ID property from resource
    ZVAL device_id_val = zjs_get_property(argv[0], "deviceId");
    ZJS_GET_STRING(device_id_val, device_id, OCF_MAX_DEVICE_ID_LEN + 1);

    struct client_resource *resource = find_resource_by_id(device_id);
    if (!resource) {
        ERR_PRINT("could not find resource %s\n", device_id);
        REJECT("NotFoundError", "resource was not found");
    }

    DBG_PRINT("update resource '%s'\n", resource->device_id);

    jerry_value_t promise = jerry_create_promise();
    h = new_ocf_handler(resource);
    h->res = resource;
    h->promise_obj = jerry_acquire_value(promise);

    if (oc_init_put(resource->resource_path, resource->endpoint, NULL,
                    put_finished, LOW_QOS, h)) {
        ZVAL props = zjs_get_property(argv[0], "properties");

        zjs_ocf_encode_value(props);

        if (!oc_do_put()) {
            ERR_PRINT("error sending PUT request\n");
            ZVAL err = make_ocf_error("NetworkError", "PUT call failed",
                                      resource);
            jerry_resolve_or_reject_promise(promise, err, false);
            jerry_release_value(promise);
        }
    } else {
        ERR_PRINT("error initializing PUT\n");
        ZVAL err = make_ocf_error("NetworkError", "PUT init failed", resource);
        jerry_resolve_or_reject_promise(promise, err, false);
        jerry_release_value(promise);
    }

    return promise;
}

static void ocf_get_platform_info_handler(oc_client_response_t *data)
{
    if (data && data->user_data) {
        struct ocf_handler *h = (struct ocf_handler *)data->user_data;
        jerry_value_t platform_info = zjs_create_object();

        /*
         * TODO: This while loop is repeated in several functions. It would be
         *    nice to have a universal way to do it but the properties that go
         *    OTA don't have the same names as the ones exposed in JavaScript.
         *    Perhaps changing this to be a function that takes a C callback
         *    which is called for each property.
         */
        int i;
        oc_rep_t *rep = data->payload;
        while (rep != NULL) {
            DBG_PRINT("Key: %s, Value: ", oc_string(rep->name));
            switch (rep->type) {
            case BOOL:
                DBG_PRINT("%d\n", rep->value.boolean);
                break;
            case INT:
                DBG_PRINT("%d\n", rep->value.integer);
                break;
            case BYTE_STRING:
            case STRING:
                if (strequal(oc_string(rep->name), "mnmn")) {
                    zjs_obj_add_readonly_string(platform_info,
                                                "manufacturerName",
                                                oc_string(rep->value.string));
                } else if (strequal(oc_string(rep->name), "pi")) {
                    zjs_obj_add_readonly_string(platform_info, "id",
                                                oc_string(rep->value.string));
                } else if (strequal(oc_string(rep->name), "mnmo")) {
                    zjs_obj_add_readonly_string(platform_info, "model",
                                                oc_string(rep->value.string));
                } else if (strequal(oc_string(rep->name), "mndt")) {
                    zjs_obj_add_readonly_string(platform_info,
                                                "manufacturerDate",
                                                oc_string(rep->value.string));
                } else if (strequal(oc_string(rep->name), "mnpv")) {
                    zjs_obj_add_readonly_string(platform_info,
                                                "platformVersion",
                                                oc_string(rep->value.string));
                } else if (strequal(oc_string(rep->name), "mnos")) {
                    zjs_obj_add_readonly_string(platform_info, "osVersion",
                                                oc_string(rep->value.string));
                } else if (strequal(oc_string(rep->name), "mnfv")) {
                    zjs_obj_add_readonly_string(platform_info,
                                                "firmwareVersion",
                                                oc_string(rep->value.string));
                } else if (strequal(oc_string(rep->name), "mnml")) {
                    zjs_obj_add_readonly_string(platform_info,
                                                "manufacturerURL",
                                                oc_string(rep->value.string));
                } else if (strequal(oc_string(rep->name), "mnsl")) {
                    zjs_obj_add_readonly_string(platform_info, "supportURL",
                                                oc_string(rep->value.string));
                }
                DBG_PRINT("%s\n", oc_string(rep->value.string));
                break;
            case STRING_ARRAY:
                DBG_PRINT("[ ");
                for (i = 0;
                     i < oc_string_array_get_allocated_size(rep->value.array);
                     i++) {
                    DBG_PRINT("%s ", oc_string_array_get_item(rep->value.array,
                                                              i));
                }
                DBG_PRINT("]\n");
                break;
            default:
                break;
            }
            rep = rep->next;
        }

        zjs_defer_emit_event(ocf_client, "platformfound", &platform_info,
                             sizeof(platform_info), zjs_copy_arg,
                             zjs_release_args);
        jerry_resolve_or_reject_promise(h->promise_obj, platform_info, true);
        jerry_release_value(h->promise_obj);
    }
}

static ZJS_DECL_FUNC(ocf_get_platform_info)
{
    // args: device ide
    ZJS_VALIDATE_ARGS(Z_STRING);
    struct ocf_handler *h;

    ZJS_GET_STRING(argv[0], device_id, OCF_MAX_DEVICE_ID_LEN + 1);

    struct client_resource *resource = find_resource_by_id(device_id);
    if (!resource) {
        ERR_PRINT("%s: %s\n", "resource was not found", device_id);
        REJECT("NotFoundError", "resource was not found");
    }

    jerry_value_t promise = jerry_create_promise();
    h = new_ocf_handler(resource);
    h->promise_obj = jerry_acquire_value(promise);

    DBG_PRINT("sending GET to /oic/p\n");

    if (!oc_do_get("/oic/p", resource->endpoint, NULL,
                   ocf_get_platform_info_handler, LOW_QOS, h)) {
        jerry_value_t err = make_ocf_error("NetworkError", "GET call failed",
                                           resource);
        jerry_resolve_or_reject_promise(promise, err, false);
        jerry_release_value(promise);
    }

    return promise;
}

static void ocf_get_device_info_handler(oc_client_response_t *data)
{
    if (data && data->user_data) {
        struct ocf_handler *h = (struct ocf_handler *)data->user_data;
        struct client_resource *resource = h->res;
        jerry_value_t device_info = zjs_create_object();

        /*
         * TODO: This while loop is repeated in several functions. It would be
         *   nice to have a universal way to do it but the properties that go
         *   OTA don't have the same names as the ones exposed in JavaScript.
         *   Perhaps changing this to be a function that takes a C callback
         *   which is called for each property.
         */
        int i;
        oc_rep_t *rep = data->payload;
        while (rep != NULL) {
            DBG_PRINT("Key: %s, Value: ", oc_string(rep->name));
            switch (rep->type) {
            case BOOL:
                DBG_PRINT("%d\n", rep->value.boolean);
                break;
            case INT:
                DBG_PRINT("%d\n", rep->value.integer);
                break;
            case BYTE_STRING:
            case STRING:
                if (strequal(oc_string(rep->name), "di")) {
                    zjs_obj_add_string(device_info, "uuid",
                                       oc_string(rep->value.string));
                    /*
                     * TODO: Where do we get the devices path to construct the
                     * URL. For now, the existing resources path will be used,
                     * but this is incorrect, because there could be devices
                     * found that are not already in our list of resources.
                     */
                    zjs_obj_add_string(device_info, "url",
                                       create_url(oc_string(rep->value.string),
                                                  resource->resource_path));
                } else if (strequal(oc_string(rep->name), "n")) {
                    zjs_obj_add_string(device_info, "name",
                                       oc_string(rep->value.string));
                } else if (strequal(oc_string(rep->name), "icv")) {
                    zjs_obj_add_string(device_info, "coreSpecVersion",
                                       oc_string(rep->value.string));
                } else if (strequal(oc_string(rep->name), "dmv")) {
                    zjs_obj_add_string(device_info, "dataModels",
                                       oc_string(rep->value.string));
                }
                DBG_PRINT("%s\n", oc_string(rep->value.string));
                break;
            case STRING_ARRAY:
                DBG_PRINT("[ ");
                for (i = 0;
                     i < oc_string_array_get_allocated_size(rep->value.array);
                     i++) {
                    DBG_PRINT("%s ", oc_string_array_get_item(rep->value.array,
                                                              i));
                }
                DBG_PRINT("]\n");
                break;
            default:
                break;
            }
            rep = rep->next;
        }

        zjs_defer_emit_event(ocf_client, "devicefound", &device_info,
                             sizeof(device_info), zjs_copy_arg,
                             zjs_release_args);
        jerry_resolve_or_reject_promise(h->promise_obj, device_info, true);
        jerry_release_value(h->promise_obj);
    }
}

static ZJS_DECL_FUNC(ocf_get_device_info)
{
    // args: device id
    ZJS_VALIDATE_ARGS(Z_STRING);

    struct ocf_handler *h;

    ZJS_GET_STRING(argv[0], device_id, OCF_MAX_DEVICE_ID_LEN + 1);

    struct client_resource *resource = find_resource_by_id(device_id);
    if (!resource) {
        ERR_PRINT("%s: %s\n", "resource was not found", device_id);
        REJECT("NotFoundError", "resource was not found");
    }

    jerry_value_t promise = jerry_create_promise();
    h = new_ocf_handler(resource);
    h->promise_obj = jerry_acquire_value(promise);

    DBG_PRINT("sending GET to /oic/d\n");

    if (!oc_do_get("/oic/d", resource->endpoint, NULL,
                   &ocf_get_device_info_handler, LOW_QOS, h)) {
        ZVAL err = make_ocf_error("NetworkError", "GET call failed", resource);
        jerry_resolve_or_reject_promise(promise, err, false);
        jerry_release_value(promise);
    }

    return promise;
}

jerry_value_t zjs_ocf_client_init()
{
    // create Client API object
    if (ocf_client) {
        return jerry_acquire_value(ocf_client);
    }

    ocf_client = zjs_create_object();
    zjs_obj_add_function(ocf_client, "findResources", ocf_find_resources);
    zjs_obj_add_function(ocf_client, "retrieve", ocf_retrieve);
    zjs_obj_add_function(ocf_client, "update", ocf_update);
    zjs_obj_add_function(ocf_client, "getPlatformInfo", ocf_get_platform_info);
    zjs_obj_add_function(ocf_client, "getDeviceInfo", ocf_get_device_info);

    zjs_make_emitter(ocf_client, ZJS_UNDEFINED, NULL, client_free_cb);

    return jerry_acquire_value(ocf_client);
}

void zjs_ocf_client_cleanup()
{
    // the ocf client object going out of scope will clean up everything
    jerry_release_value(ocf_client);
    ocf_client = 0;
}

#endif  // BUILD_MODULE_OCF
