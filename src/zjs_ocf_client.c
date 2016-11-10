#ifdef BUILD_MODULE_OCF

#include "jerry-api.h"

#include "zjs_util.h"
#include "zjs_common.h"

#include "zjs_ocf_client.h"
#include "zjs_ocf_common.h"

#include "oc_api.h"
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include "port/oc_clock.h"

#include "zjs_ocf_encoder.h"
#include "zjs_event.h"
#include "zjs_promise.h"

//#define USE_PROMISES

typedef enum {
    RES_STATE_SEARCHING,
    RES_STATE_FOUND
} resource_state;

#define FLAG_OBSERVE        1 << 0
#define FLAG_DISCOVERABLE   1 << 1
#define FLAG_SLOW           1 << 2
#define FLAG_SECURE         1 << 3

struct client_resource {
    char* device_id;
    char* resource_type;
    char* resource_path;
    oc_server_handle_t server;
    resource_state state;
    jerry_value_t client;
    uint32_t flags;
    uint32_t error_code;
    struct client_resource* next;
};

struct ocf_handler {
    jerry_value_t promise_obj;
    jerry_value_t* argv;
    int32_t promise_id;
    struct client_resource* res;
};

struct client_resource* resources = NULL;

#define MAX_URI_LENGTH (30)

static struct ocf_handler* new_ocf_handler(struct client_resource* res)
{
    struct ocf_handler* h = zjs_malloc(sizeof(struct ocf_handler));
    if (!h) {
        ERR_PRINT("could not allocate OCF handle, out of memory\n");
        return NULL;
    }
    memset(h, 0, sizeof(struct ocf_handler*));
    h->res = res;

    return h;
}

static jerry_value_t make_ocf_error(const char* name, const char* msg, struct client_resource* res)
{
    if (res) {
        jerry_value_t ret = jerry_create_object();
        if (name) {
            zjs_obj_add_string(ret, name, "name");
        } else {
            ERR_PRINT("error must have a name\n");
            jerry_release_value(ret);
            return ZJS_UNDEFINED;
        }
        if (msg) {
            zjs_obj_add_string(ret, msg, "message");
        } else {
            ERR_PRINT("error must have a message\n");
            jerry_release_value(ret);
            return ZJS_UNDEFINED;
        }
        if (res->device_id) {
            zjs_obj_add_string(ret, res->device_id, "deviceId");
        }
        if (res->resource_path) {
            zjs_obj_add_string(ret, res->resource_path, "resourcePath");
        }
        zjs_obj_add_number(ret, (double)res->error_code, "errorCode");

        return ret;
    } else {
        ERR_PRINT("client resource was NULL\n");
        return ZJS_UNDEFINED;
    }
}

/*
 * Combine a UUID and path to form a URI
 */
static char* create_url(const char* uuid, const char* path)
{
    //    oic://<uuid>/<path>
    char* url = zjs_malloc(strlen(uuid) + strlen(path) + 8);
    int count = 0;
    url[0] = 'o';
    url[1] = 'i';
    url[2] = 'c';
    url[3] = ':';
    url[4] = '/';
    url[5] = '/';
    count = 6;
    memcpy(url + count, uuid, strlen(uuid));
    count += strlen(uuid);
    if (path[0] != '/') {
        url[count] = '/';
        count++;
    }
    memcpy(url + count, path, strlen(path));
    count += strlen(path);
    url[count] = '\0';
    return url;
}

/*
 * Get properties in 'data' and convert them to a jerry_value_t
 */
static jerry_value_t get_props_from_response(oc_client_response_t* data)
{
    jerry_value_t prop_object = jerry_create_object();
    int i;
    oc_rep_t *rep = data->payload;

    DBG_PRINT("properties:\n");

    while (rep != NULL) {
        DBG_PRINT("Type: %u, Key: %s, Value: ", rep->type, oc_string(rep->name));
        switch (rep->type) {
        case BOOL:
            zjs_obj_add_boolean(prop_object, rep->value_boolean, oc_string(rep->name));
            DBG_PRINT("%d\n", rep->value_boolean);
            break;
        case INT:
            zjs_obj_add_number(prop_object, (double)rep->value_int, oc_string(rep->name));
            DBG_PRINT("%ld\n", (uint32_t)rep->value_int);
            break;
        case BYTE_STRING:
        case STRING:
            zjs_obj_add_string(prop_object, oc_string(rep->value_string), oc_string(rep->name));
            DBG_PRINT("%s\n", oc_string(rep->value_string));
            break;
        case STRING_ARRAY:
            DBG_PRINT("[ ");
            for (i = 0; i < oc_string_array_get_allocated_size(rep->value_array); i++) {
                DBG_PRINT("%s ", oc_string_array_get_item(rep->value_array, i));
            }
            DBG_PRINT("]\n");
            break;
        case OBJECT:
            DBG_PRINT("{ Object }\n");
            break;
        default:
            break;
        }
        rep = rep->next;
    }

    return prop_object;
}

#ifdef DEBUG_BUILD
static void print_props_data(oc_client_response_t *data)
{
    int i;
    oc_rep_t *rep = data->payload;
    while (rep != NULL) {
        ZJS_PRINT("Type: %u, Key: %s, Value: ", rep->type, oc_string(rep->name));
        switch (rep->type) {
        case BOOL:
            ZJS_PRINT("%d\n", rep->value_boolean);
            break;
        case INT:
            ZJS_PRINT("%ld\n", (uint32_t)rep->value_int);
            break;
        case BYTE_STRING:
        case STRING:
            ZJS_PRINT("%s\n", oc_string(rep->value_string));
            break;
        case STRING_ARRAY:
            ZJS_PRINT("[ ");
            for (i = 0; i < oc_string_array_get_allocated_size(rep->value_array); i++) {
                ZJS_PRINT("%s ", oc_string_array_get_item(rep->value_array, i));
            }
            ZJS_PRINT("]\n");
            break;
        case OBJECT:
            ZJS_PRINT("{ Object }\n");
            break;
        default:
            break;
        }
        rep = rep->next;
    }
}
#else
#define print_props_data(d) do {} while(0)
#endif

/*
 * Find a client_resource by searching with a device ID
 */
static struct client_resource* find_resource_by_id(const char* device_id)
{
    if (device_id) {
        struct client_resource* cur = resources;
        while (cur) {
            if (cur->state != RES_STATE_SEARCHING) {
                if (strcmp(cur->device_id, device_id) == 0) {
                    return cur;
                }
            }
            cur = cur->next;
        }
    }
    return NULL;
}

#if 0
/*
 * Find a client_resource by searching with a resource path
 */
static struct client_resource* find_resource_by_path(const char* path)
{
    if (path) {
        struct client_resource* cur = resources;
        while (cur) {
            if (cur->state != RES_STATE_SEARCHING) {
                if (strcmp(cur->resource_path, path) == 0) {
                    return cur;
                }
            }
            cur = cur->next;
        }
    }
    return NULL;
}
#endif

/*
 * Create a new resource object
 */
static jerry_value_t create_resource(const char* device_id, const char* path)
{
    jerry_value_t resource = jerry_create_object();

    if (device_id) {
        zjs_obj_add_string(resource, device_id, "deviceId");
    }
    if (path) {
        zjs_obj_add_string(resource, path, "resourcePath");
    }

    DBG_PRINT("id=%s, path=%s, obj number=%lu\n", device_id, path, resource);

    return resource;
}

/*
 * Add a discovered resource to the list of resources
 */
static void add_resource(char* id, char* type, char* path, jerry_value_t client, jerry_value_t listener)
{
    struct client_resource* new = zjs_malloc(sizeof(struct client_resource));

    memset(new, 0, sizeof(struct client_resource));
    new->state = RES_STATE_SEARCHING;

    if (id) {
        new->device_id = zjs_malloc(strlen(id) + 1);
        memcpy(new->device_id, id, strlen(id));
        new->device_id[strlen(id)] = '\0';
    }
    if (type) {
        new->resource_type = zjs_malloc(strlen(type) + 1);
        memcpy(new->resource_type, type, strlen(type));
        new->resource_type[strlen(type)] = '\0';
    }
    if (id) {
        new->resource_path = zjs_malloc(strlen(path) + 1);
        memcpy(new->resource_path, path, strlen(path));
        new->resource_path[strlen(path)] = '\0';
    }

    new->client = client;

    if (!jerry_value_is_undefined(listener)) {
        zjs_add_event_listener(new->client, "resourcefound", listener);
    }

    new->next = resources;
    resources = new;
}

static void post_ocf_promise(void* handle)
{
    struct ocf_handler* h = (struct ocf_handler*)handle;
    if (h) {
        if (h->argv) {
            //jerry_release_value(h->argv[0]);
            zjs_free(h->argv);
        }
        jerry_release_value(h->promise_obj);
        zjs_free(h);
    }
}

// Used to free the resource found argument
static void post_resource_found(void* handle)
{
    if (handle) {
        zjs_free(handle);
    }
}

/*
 * Callback to observe, does not do anything currently
 */
static void observe_callback(oc_client_response_t *data)
{
    print_props_data(data);
}

#if 0
static oc_event_callback_retval_t stop_observe(void* data)
{
    struct client_resource* cli = (struct client_resource*)data;
    oc_stop_observe(cli->resource_path, &cli->server);

    DBG_PRINT("path=%s, id=%s\n", cli->resource_path, cli->device_id);

    return DONE;
}
#endif

/*
 * Callback for resource discovery
 */
static oc_discovery_flags_t discovery(const char *di,
                                      const char *uri,
                                      oc_string_array_t types,
                                      oc_interface_mask_t interfaces,
                                      oc_server_handle_t *server,
                                      void *user_handle)
{
    struct ocf_handler* h = (struct ocf_handler*)user_handle;
    int i;
    int uri_len = strlen(uri);
    uri_len = (uri_len >= MAX_URI_LENGTH)?MAX_URI_LENGTH-1:uri_len;

    for (i = 0; i < oc_string_array_get_allocated_size(types); i++) {
        char *t = oc_string_array_get_item(types, i);
        struct client_resource* cur = resources;
        while (cur) {
            if (cur->state == RES_STATE_SEARCHING) {
                // check if resource has any filter constraints
                if (cur->device_id) {
                    if (strcmp(cur->device_id, di) == 0) {
                        goto Found;
                    } else {
                        goto NotFound;
                    }
                }
                if (cur->resource_type) {
                    if (strcmp(cur->resource_type, t) == 0) {
                        goto Found;
                    } else {
                        goto NotFound;
                    }
                }
                if (cur->resource_path) {
                    if (strcmp(cur->resource_path, uri) == 0) {
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
            memcpy(&cur->server, server, sizeof(oc_server_handle_t));

            if (!cur->device_id) {
                cur->device_id = zjs_malloc(strlen(di) + 1);
                memcpy(cur->device_id, di, strlen(di));
                cur->device_id[strlen(di)] = '\0';
            }
            if (!cur->resource_path) {
                cur->resource_path = zjs_malloc(strlen(uri) + 1);
                memcpy(cur->resource_path, uri, uri_len);
                cur->resource_path[uri_len] = '\0';
            }
            jerry_value_t args = create_resource(cur->device_id, cur->resource_path);
            jerry_value_t* args_arr = zjs_malloc(sizeof(jerry_value_t));
            args_arr[0] = args;
            zjs_trigger_event(cur->client, "resourcefound", args_arr, 1, post_resource_found, args_arr);

            h->argv = zjs_malloc(sizeof(jerry_value_t));
            h->argv[0] = args;

            zjs_fulfill_promise(h->promise_obj, h->argv, 1);

            DBG_PRINT("resource found, id=%s, path=%s\n", cur->device_id, cur->resource_path);

            return OC_STOP_DISCOVERY;
        }
    }
    return OC_CONTINUE_DISCOVERY;
}

static jerry_value_t ocf_find_resources(const jerry_value_t function_val,
                                        const jerry_value_t this,
                                        const jerry_value_t argv[],
                                        const jerry_length_t argc)
{
    char* resource_type = NULL;
    char* device_id = NULL;
    char* resource_path = NULL;
    uint8_t listen_idx = 0xff;
    jerry_value_t listener = ZJS_UNDEFINED;
    jerry_value_t promise = jerry_create_object();

    if (jerry_value_is_object(argv[0])) {
        // has options parameter

        jerry_value_t device_id_val = zjs_get_property(argv[0], "deviceId");
        jerry_value_t res_type_val = zjs_get_property(argv[0], "resourceType");
        jerry_value_t res_path_val = zjs_get_property(argv[0], "resourcePath");

        if (jerry_value_is_string(device_id_val)) {
            int sz = jerry_get_string_size(device_id_val);
            device_id = zjs_malloc(sz + 1);
            int len = jerry_string_to_char_buffer(device_id_val, (jerry_char_t *)device_id, sz);
            device_id[len] = '\0';

            DBG_PRINT("deviceId: %s\n", device_id);
        }
        if (jerry_value_is_string(res_type_val)) {
            int sz = jerry_get_string_size(res_type_val);
            resource_type = zjs_malloc(sz + 1);
            int len = jerry_string_to_char_buffer(res_type_val, (jerry_char_t *)resource_type, sz);
            resource_type[len] = '\0';

            DBG_PRINT("resourceType: %s\n", resource_type);
        }
        if (jerry_value_is_string(res_path_val)) {
            int sz = jerry_get_string_size(res_path_val);
            resource_path = zjs_malloc(sz + 1);
            int len = jerry_string_to_char_buffer(res_path_val, (jerry_char_t *)resource_path, sz);
            resource_path[len] = '\0';

            DBG_PRINT("resourcePath: %s\n", resource_path);
        }
    }

    if (jerry_value_is_function(argv[0])) {
        // has listener parameter
        listen_idx = 0;
    } else if (jerry_value_is_function(argv[1])) {
        listen_idx = 1;
    }

    if (listen_idx != 0xff) {
        listener = argv[listen_idx];
        DBG_PRINT("'resourcefound' listener provided\n");
    }

    add_resource(device_id, resource_type, resource_path, this, listener);

    //if (resource_type) {
    //    zjs_free(resource_type);
    //}
    if (device_id) {
        zjs_free(device_id);
    }
    if (resource_path) {
        zjs_free(resource_path);
    }

    struct ocf_handler* h = new_ocf_handler(NULL);
    h->promise_obj = promise;

    zjs_make_promise(promise, post_ocf_promise, h);

    oc_do_ip_discovery(resource_type, &discovery, h);

    return promise;
}

static void ocf_get_handler(oc_client_response_t *data)
{
    if (data && data->user_data) {
        struct ocf_handler* h = (struct ocf_handler*)data->user_data;
        if (h && h->res) {
            struct client_resource* resource = h->res;
            if (data->code == OC_STATUS_OK) {
                jerry_value_t resource_val = create_resource(resource->device_id, resource->resource_path);
                jerry_value_t properties_val = get_props_from_response(data);

                zjs_set_property(resource_val, "properties", properties_val);

                zjs_trigger_event(resource->client, "update", &resource_val, 1, NULL, NULL);

                h->argv = zjs_malloc(sizeof(jerry_value_t));
                h->argv[0] = resource_val;
                zjs_fulfill_promise(h->promise_obj, h->argv, 1);

                DBG_PRINT("GET response OK, device_id=%s\n", resource->device_id);
            } else {
                // Reject promise
                jerry_value_t err = make_ocf_error("NetworkError", "Error code from GET", resource);
                h->argv = zjs_malloc(sizeof(jerry_value_t));
                h->argv[0] = err;
                zjs_reject_promise(h->promise_obj, h->argv, 1);

                ERR_PRINT("GET response code %d\n", data->code);
            }
        }
    }
}

static jerry_value_t ocf_retrieve(const jerry_value_t function_val,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    jerry_value_t options = 0;
    jerry_value_t listener;
    jerry_value_t promise = jerry_create_object();
    struct ocf_handler* h;

    if (!jerry_value_is_string(argv[0])) {
        ERR_PRINT("first parameter must be deviceId\n");
        REJECT(promise, "TypeMismatchError", "first parameter must be device ID", h);
        return promise;
    }

    ZJS_GET_STRING(argv[0], device_id);

    struct client_resource* resource = find_resource_by_id(device_id);
    if (!resource) {
        ERR_PRINT("could not find resource %s\n", device_id);
        REJECT(promise, "NotFoundError", "resource was not found", h);
        return promise;
    }

    if (argc > 1) {
        if (argc == 2) {
            if (jerry_value_is_function(argv[1])) {
                listener = argv[1];
                zjs_add_event_listener(this, "update", listener);
            } else {
                options = argv[1];
            }
        } else if (argc == 3) {
            if (jerry_value_is_function(argv[1])) {
                listener = argv[1];
                options = argv[2];
            } else {
                listener = argv[2];
                options = argv[1];
            }
            zjs_add_event_listener(this, "update", listener);
        }
        jerry_value_t observe_flag = zjs_get_property(options, "observable");
        if (jerry_value_is_boolean(observe_flag)) {
            bool val = jerry_get_boolean_value(observe_flag);
            if (val) {
                resource->flags |= FLAG_OBSERVE;
            }
        }
        jerry_value_t discover_flag = zjs_get_property(options, "discoverable");
        if (jerry_value_is_boolean(discover_flag)) {
            bool val = jerry_get_boolean_value(discover_flag);
            if (val) {
                resource->flags |= FLAG_DISCOVERABLE;
            }
        }
        jerry_value_t secure_flag = zjs_get_property(options, "secure");
        if (jerry_value_is_boolean(secure_flag)) {
            bool val = jerry_get_boolean_value(secure_flag);
            if (val) {
                resource->flags |= FLAG_SECURE;
            }
        }
        jerry_value_t slow_flag = zjs_get_property(options, "slow");
        if (jerry_value_is_boolean(slow_flag)) {
            bool val = jerry_get_boolean_value(slow_flag);
            if (val) {
                resource->flags |= FLAG_SLOW;
            }
        }
    }

    if (resource->flags & FLAG_OBSERVE) {
        oc_do_observe(resource->resource_path, &resource->server, NULL, &observe_callback, LOW_QOS, resource);
    }

    DBG_PRINT("resource found in lookup: path=%s, id=%s\n", resource->resource_path, resource->device_id);

    h = new_ocf_handler(resource);
    h->res = resource;
    h->promise_obj = promise;

    zjs_make_promise(promise, post_ocf_promise, h);

    if (!oc_do_get(resource->resource_path,
                   &resource->server,
                   NULL,
                   &ocf_get_handler,
                   LOW_QOS,
                   h)) {
        h->argv = zjs_malloc(sizeof(jerry_value_t));
        h->argv[0] = make_ocf_error("NetworkError", "GET call failed", resource);
        zjs_reject_promise(promise, h->argv, 1);
    }

    return promise;
}

static void put_finished(oc_client_response_t *data)
{
    if (data) {
        struct ocf_handler* h = (struct ocf_handler*)data->user_data;
        if (h->res) {
            struct client_resource* resource = h->res;
            h->res->error_code = data->code;
            if (data->code == OC_STATUS_CHANGED) {
                DBG_PRINT("PUT response OK, device_id=%s\n", resource->device_id);
                jerry_value_t resource_val = create_resource(resource->device_id, resource->resource_path);
                h->argv = zjs_malloc(sizeof(jerry_value_t));
                h->argv[0] = resource_val;
                zjs_fulfill_promise(h->promise_obj, h->argv, 1);
            } else {
                ERR_PRINT("PUT response code %d\n", data->code);
                jerry_value_t err = make_ocf_error("NetworkError", "PUT response error code", resource);
                h->argv = zjs_malloc(sizeof(jerry_value_t));
                h->argv[0] = err;
                zjs_reject_promise(h->promise_obj, h->argv, 1);
            }
        }
    }
}

static jerry_value_t ocf_update(const jerry_value_t function_val,
                                const jerry_value_t this,
                                const jerry_value_t argv[],
                                const jerry_length_t argc)
{
    jerry_value_t promise = jerry_create_object();
    struct ocf_handler* h;

    if (!jerry_value_is_object(argv[0])) {
        ERR_PRINT("first parameter must be resource object\n");
        REJECT(promise, "TypeMismatchError", "first parameter must be resource object", h);
        return promise;
    }
    // Get device ID property from resource
    jerry_value_t device_id_val = zjs_get_property(argv[0], "deviceId");

    ZJS_GET_STRING(device_id_val, device_id);

    jerry_release_value(device_id_val);

    struct client_resource* resource = find_resource_by_id(device_id);
    if (!resource) {
        ERR_PRINT("could not find resource %s\n", device_id);
        REJECT(promise, "NotFoundError", "resource was not found", h);
        return promise;
    }

    DBG_PRINT("update resource '%s'\n", resource->device_id);

    h = new_ocf_handler(resource);
    zjs_make_promise(promise, post_ocf_promise, h);
    h->res = resource;
    h->promise_obj = promise;

    if (oc_init_put(resource->resource_path,
                    &resource->server,
                    NULL,
                    &put_finished,
                    LOW_QOS,
                    h)) {
        void* ret;
        // Start the root encoding object
        zjs_rep_start_root_object();
        // Encode all properties from resource (argv[0])
        ret = zjs_ocf_props_setup(argv[0], &g_encoder, true);
        zjs_rep_end_root_object();
        // Free property return handle
        zjs_ocf_free_props(ret);
        if (!oc_do_put()) {
            ERR_PRINT("error sending PUT request\n");
            h->argv = zjs_malloc(sizeof(jerry_value_t));
            h->argv[0] = make_ocf_error("NetworkError", "PUT call failed", resource);
            zjs_reject_promise(promise, h->argv, 1);
        }
    } else {
        ERR_PRINT("error initializing PUT\n");
        h->argv = zjs_malloc(sizeof(jerry_value_t));
        h->argv[0] = make_ocf_error("NetworkError", "PUT init failed", resource);
        zjs_reject_promise(promise, h->argv, 1);
    }

    return promise;
}

static void delete_finished(oc_client_response_t *data)
{
    if (data && data->user_data) {
        struct ocf_handler* h = (struct ocf_handler*)data->user_data;
        struct client_resource* resource = h->res;
        if (data->code == OC_STATUS_DELETED) {
            zjs_fulfill_promise(h->promise_obj, NULL, 0);

            DBG_PRINT("DELETE response OK, device_id=%s\n", resource->device_id);
        } else {
            h->argv = zjs_malloc(sizeof(jerry_value_t));
            h->argv[0] = make_ocf_error("NetworkError", "DELETE had error code", resource);
            zjs_reject_promise(h->promise_obj, h->argv, 1);

            ERR_PRINT("DELETE response code %d\n", data->code);
        }
    }
}

static jerry_value_t ocf_delete(const jerry_value_t function_val,
                                const jerry_value_t this,
                                const jerry_value_t argv[],
                                const jerry_length_t argc)
{
    struct ocf_handler* h;
    jerry_value_t promise = jerry_create_object();

    if (!jerry_value_is_string(argv[0])) {
        ERR_PRINT("first parameter must be deviceId\n");
        REJECT(promise, "TypeMismatchError", "first parameter must be device ID", h);
        return promise;
    }

    ZJS_GET_STRING(argv[0], uri);

    DBG_PRINT("DELETE call, uri=%s\n", uri);

    struct client_resource* resource = find_resource_by_id(uri);
    if (!resource) {
        ERR_PRINT("resource '%s' not found\n", uri);
        REJECT(promise, "NotFoundError", "resource was not found", h);
        return promise;
    }

    h = new_ocf_handler(resource);
    zjs_make_promise(promise, post_ocf_promise, h);
    h->promise_obj = promise;

    if (!oc_do_delete(uri, &resource->server, delete_finished, LOW_QOS, h)) {
        ERR_PRINT("DELETE call failed\n");
        h->argv = zjs_malloc(sizeof(jerry_value_t));
        h->argv[0] = make_ocf_error("NetworkError", "DELETE call failed", resource);
        zjs_reject_promise(promise, h->argv, 1);
    }

    return promise;
}

static jerry_value_t ocf_create(const jerry_value_t function_val,
                                const jerry_value_t this,
                                const jerry_value_t argv[],
                                const jerry_length_t argc)
{
    struct ocf_handler* h;
    jerry_value_t promise = jerry_create_object();
    REJECT(promise, "NotSupportedError", "create() is not supported", h);
    DBG_PRINT("create is not supported by iotivity-constrained\n");
    return promise;
}

static void ocf_get_platform_info_handler(oc_client_response_t *data)
{
    if (data && data->user_data) {
        struct ocf_handler* h = (struct ocf_handler*)data->user_data;
        struct client_resource* resource = h->res;
        jerry_value_t platform_info = jerry_create_object();

        /*
         * TODO: This while loop is repeated in several functions. It would be nice
         *       to have a universal way to do it but the properties that go OTA
         *       don't have the same names as the ones exposed in JavaScript. Perhaps
         *       changing this to be a function that takes a C callback which is
         *       called for each property.
         */
        int i;
        oc_rep_t *rep = data->payload;
        while (rep != NULL) {
            DBG_PRINT("Key: %s, Value: ", oc_string(rep->name));
            switch (rep->type) {
            case BOOL:
                DBG_PRINT("%d\n", rep->value_boolean);
                break;
            case INT:
                DBG_PRINT("%ld\n", (uint32_t)rep->value_int);
                break;
            case BYTE_STRING:
            case STRING:
                if (strcmp(oc_string(rep->name), "mnmn") == 0) {
                    zjs_obj_add_string(platform_info, oc_string(rep->value_string), "manufacturerName");
                } else if (strcmp(oc_string(rep->name), "pi") == 0) {
                    zjs_obj_add_string(platform_info, oc_string(rep->value_string), "id");
                }
                DBG_PRINT("%s\n", oc_string(rep->value_string));
                break;
            case STRING_ARRAY:
                DBG_PRINT("[ ");
                for (i = 0; i < oc_string_array_get_allocated_size(rep->value_array); i++) {
                    DBG_PRINT("%s ", oc_string_array_get_item(rep->value_array, i));
                }
                DBG_PRINT("]\n");
                break;
            default:
                break;
            }
            rep = rep->next;
        }

        zjs_trigger_event(resource->client, "platformfound", &platform_info, 1, NULL, NULL);
        h->argv = zjs_malloc(sizeof(jerry_value_t));
        h->argv[0] = platform_info;
        zjs_fulfill_promise(h->promise_obj, h->argv, 1);
    }
}

static jerry_value_t ocf_get_platform_info(const jerry_value_t function_val,
                                           const jerry_value_t this,
                                           const jerry_value_t argv[],
                                           const jerry_length_t argc)
{
    struct ocf_handler* h;
    jerry_value_t promise = jerry_create_object();

    if (!jerry_value_is_string(argv[0])) {
        ERR_PRINT("first parameter must be device ID\n");
        REJECT(promise, "TypeMismatchError", "first parameter must be device ID", h);
        return promise;
    }

    ZJS_GET_STRING(argv[0], device_id);

    struct client_resource* resource = find_resource_by_id(device_id);
    if (!resource) {
        ERR_PRINT("resource was not found: %s\n", device_id);
        REJECT(promise, "NotFoundError", "resource was not found", h);
        return promise;
    }

    h = new_ocf_handler(resource);
    zjs_make_promise(promise, post_ocf_promise, h);
    h->promise_obj = promise;

    DBG_PRINT("sending GET to /oic/p\n");

    if (!oc_do_get("/oic/p",
                   &resource->server,
                   NULL,
                   &ocf_get_platform_info_handler,
                   LOW_QOS,
                   h)) {
        h->argv = zjs_malloc(sizeof(jerry_value_t));
        h->argv[0] = make_ocf_error("NetworkError", "GET call failed", resource);
        zjs_reject_promise(promise, h->argv, 1);
    }

    return promise;
}

static void ocf_get_device_info_handler(oc_client_response_t *data)
{
    if (data && data->user_data) {
        struct ocf_handler* h = (struct ocf_handler*)data->user_data;
        struct client_resource* resource = h->res;
        jerry_value_t device_info = jerry_create_object();

        /*
         * TODO: This while loop is repeated in several functions. It would be nice
         *       to have a universal way to do it but the properties that go OTA
         *       don't have the same names as the ones exposed in JavaScript. Perhaps
         *       changing this to be a function that takes a C callback which is
         *       called for each property.
         */
        int i;
        oc_rep_t *rep = data->payload;
        while (rep != NULL) {
            DBG_PRINT("Key: %s, Value: ", oc_string(rep->name));
            switch (rep->type) {
            case BOOL:
                DBG_PRINT("%d\n", rep->value_boolean);
                break;
            case INT:
                DBG_PRINT("%ld\n", (uint32_t)rep->value_int);
                break;
            case BYTE_STRING:
            case STRING:
                if (strcmp(oc_string(rep->name), "di") == 0) {
                    zjs_obj_add_string(device_info, oc_string(rep->value_string), "uuid");
                    /*
                     * TODO: Where do we get the devices path to construct the URL.
                     * For now, the existing resources path will be used, but this is
                     * incorrect, because there could be devices found that are not
                     * already in our list of resources.
                     */
                    zjs_obj_add_string(device_info, create_url(oc_string(rep->value_string), resource->resource_path), "url");
                } else if (strcmp(oc_string(rep->name), "n") == 0) {
                    zjs_obj_add_string(device_info, oc_string(rep->value_string), "name");
                } else if (strcmp(oc_string(rep->name), "icv") == 0) {
                    zjs_obj_add_string(device_info, oc_string(rep->value_string), "coreSpecVersion");
                }
                DBG_PRINT("%s\n", oc_string(rep->value_string));
                break;
            case STRING_ARRAY:
                DBG_PRINT("[ ");
                for (i = 0; i < oc_string_array_get_allocated_size(rep->value_array); i++) {
                    DBG_PRINT("%s ", oc_string_array_get_item(rep->value_array, i));
                }
                DBG_PRINT("]\n");
                break;
            default:
                break;
            }
            rep = rep->next;
        }

        zjs_trigger_event(resource->client, "devicefound", &device_info, 1, NULL, NULL);
        h->argv = zjs_malloc(sizeof(jerry_value_t));
        h->argv[0] = device_info;
        zjs_fulfill_promise(h->promise_obj, h->argv, 1);
    }
}

static jerry_value_t ocf_get_device_info(const jerry_value_t function_val,
                                         const jerry_value_t this,
                                         const jerry_value_t argv[],
                                         const jerry_length_t argc)
{
    struct ocf_handler* h;
    jerry_value_t promise = jerry_create_object();

    if (!jerry_value_is_string(argv[0])) {
        ERR_PRINT("first parameter must be device ID\n");
        REJECT(promise, "TypeMismatchError", "first parameter must be device ID", h);
        return promise;
    }

    ZJS_GET_STRING(argv[0], device_id);

    struct client_resource* resource = find_resource_by_id(device_id);
    if (!resource) {
        ERR_PRINT("resource was not found: %s\n", device_id);
        REJECT(promise, "NotFoundError", "resource was not found", h);
        return promise;
    }

    h = new_ocf_handler(resource);
    zjs_make_promise(promise, post_ocf_promise, h);
    h->promise_obj = promise;

    DBG_PRINT("sending GET to /oic/d\n");

    if (!oc_do_get("/oic/d",
                   &resource->server,
                   NULL,
                   &ocf_get_device_info_handler,
                   LOW_QOS,
                   h)) {
        h->argv = zjs_malloc(sizeof(jerry_value_t));
        h->argv[0] = make_ocf_error("NetworkError", "GET call failed", resource);
        zjs_reject_promise(promise, h->argv, 1);
    }

    return promise;
}

static jerry_value_t ocf_find_devices(const jerry_value_t function_val,
                                      const jerry_value_t this,
                                      const jerry_value_t argv[],
                                      const jerry_length_t argc)
{
    ERR_PRINT("findDevices() is not yet supported\n");
    struct ocf_handler* h;
    jerry_value_t promise = jerry_create_object();
    REJECT(promise, "NotSupportedError", "findDevices() is not supported", h);
    return promise;
}

static jerry_value_t ocf_find_platforms(const jerry_value_t function_val,
                                        const jerry_value_t this,
                                        const jerry_value_t argv[],
                                        const jerry_length_t argc)
{
    ERR_PRINT("findPlatforms() is not yet supported\n");

    jerry_value_t promise = jerry_create_object();
    struct ocf_handler* h;
    REJECT(promise, "NotSupportedError", "findPlatforms() is not supported", h);
    return promise;
}

jerry_value_t zjs_ocf_client_init()
{
    jerry_value_t ocf_client = jerry_create_object();

    zjs_make_event(ocf_client);

    zjs_obj_add_function(ocf_client, ocf_find_resources, "findResources");
    zjs_obj_add_function(ocf_client, ocf_retrieve, "retrieve");
    zjs_obj_add_function(ocf_client, ocf_update, "update");
    zjs_obj_add_function(ocf_client, ocf_delete, "delete");
    zjs_obj_add_function(ocf_client, ocf_create, "create");
    zjs_obj_add_function(ocf_client, ocf_get_platform_info, "getPlatformInfo");
    zjs_obj_add_function(ocf_client, ocf_get_device_info, "getDeviceInfo");
    zjs_obj_add_function(ocf_client, ocf_find_devices, "findDevices");
    zjs_obj_add_function(ocf_client, ocf_find_platforms, "findPlatforms");

    return ocf_client;
}

#endif // BUILD_MODULE_OCF
