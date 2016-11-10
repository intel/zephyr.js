#ifdef BUILD_MODULE_OCF

#include "oc_api.h"
#include "port/oc_clock.h"
//#include "port/oc_signal_main_loop.h"

#include "zjs_util.h"
#include "zjs_common.h"

#include "zjs_ocf_server.h"
#include "zjs_ocf_encoder.h"
#include "zjs_ocf_common.h"

#include "zjs_event.h"
#include "zjs_promise.h"

struct server_resource {
    jerry_value_t object;
    char* device_id;
    char* resource_path;
    uint32_t error_code;
    oc_resource_t *res;
};

struct ocf_handler {
    jerry_value_t promise_obj;
    jerry_value_t* argv;
    jerry_value_t properties;
    struct ocf_response* resp;
    struct server_resource* res;
};

struct ocf_response {
    oc_method_t method;         // Current method being executed
    struct server_resource* res;
};

#define FLAG_OBSERVE        1 << 0
#define FLAG_DISCOVERABLE   1 << 1
#define FLAG_SLOW           1 << 2
#define FLAG_SECURE         1 << 3

static struct ocf_handler* new_ocf_handler(struct server_resource* res)
{
    struct ocf_handler* h = zjs_malloc(sizeof(struct ocf_handler));
    if (!h) {
        ERR_PRINT("could not allocate OCF handle, out of memory\n");
        return NULL;
    }
    memset(h, 0, sizeof(struct ocf_handler));
    h->res = res;

    return h;
}

static void post_ocf_promise(void* handle)
{
    struct ocf_handler* h = (struct ocf_handler*)handle;
    if (h) {
        if (h->argv) {
            zjs_free(h->argv);
        }
        if (h->resp) {
            zjs_free(h->resp);
        }
        zjs_free(h);
    }
}

static jerry_value_t make_ocf_error(const char* name, const char* msg, struct server_resource* res)
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

static jerry_value_t request_to_jerry_value(oc_request_t *request)
{
    jerry_value_t props = jerry_create_object();
    oc_rep_t *rep = request->request_payload;
    while (rep != NULL) {
        switch (rep->type) {
        case BOOL:
            zjs_obj_add_boolean(props, rep->value_boolean, oc_string(rep->name));
            break;
        case INT:
            zjs_obj_add_number(props, rep->value_int, oc_string(rep->name));
            break;
        case BYTE_STRING:
        case STRING:
            zjs_obj_add_string(props, oc_string(rep->value_string), oc_string(rep->name));
            break;
            /*
             * TODO: Implement encoding for complex types
             */
        case STRING_ARRAY:
        case OBJECT:
            ZJS_PRINT("{ Object }\n");
            break;
        default:
            break;
        }
        rep = rep->next;
    }
    return props;
}

struct server_resource* new_server_resource(char* path)
{
    struct server_resource* resource = zjs_malloc(sizeof(struct server_resource));
    memset(resource, 0, sizeof(struct server_resource));

    resource->resource_path = zjs_malloc(strlen(path));
    memcpy(resource->resource_path, path, strlen(path));
    resource->resource_path[strlen(path)] = '\0';

    return resource;
}

static struct ocf_response* create_response(struct server_resource* resource, oc_method_t method)
{
    struct ocf_response* resp = zjs_malloc(sizeof(struct ocf_response));
    memset(resp, 0, sizeof(struct ocf_response));
    resp->res = resource;
    resp->method = method;

    return resp;
}

static jerry_value_t create_resource(const char* path, jerry_value_t resource_init)
{
    jerry_value_t res = jerry_create_object();

    if (path) {
        zjs_obj_add_string(res, path, "resourcePath");
    }
    jerry_value_t properties = zjs_get_property(resource_init, "properties");

    zjs_set_property(res, "properties", properties);

    DBG_PRINT("path=%s, obj number=%lu\n", path, res);

    return res;
}

static jerry_value_t create_request(struct server_resource* resource, oc_method_t method, struct ocf_handler* handler)
{

    handler->resp = create_response(resource, method);
    jerry_value_t object = jerry_create_object();
    jerry_value_t target = jerry_create_object();
    jerry_value_t source = jerry_create_object();

    if (resource->res) {
        zjs_obj_add_string(source, oc_string(resource->res->uri), "resourcePath");
        zjs_obj_add_string(target, oc_string(resource->res->uri), "resourcePath");
    }

    // source is the resource requesting the operation
    zjs_set_property(object, "source", source);

    // target is the resource being retrieved
    zjs_set_property(object, "target", target);

    jerry_set_object_native_handle(object, (uintptr_t)handler, NULL);

    return object;
}

#if 0
static void print_props_data(oc_request_t *data)
{
    int i;
    oc_rep_t *rep = data->request_payload;
    while (rep != NULL) {
        ZJS_PRINT("Type: %u, Key: %s, Value: ", rep->type, oc_string(rep->name));
        switch (rep->type) {
        case BOOL:
            ZJS_PRINT("%d\n", rep->value_boolean);
            break;
        case INT:
            ZJS_PRINT("%ld\n", (int32_t)rep->value_int);
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
#endif

static jerry_value_t ocf_respond(const jerry_value_t function_val,
                                 const jerry_value_t this,
                                 const jerry_value_t argv[],
                                 const jerry_length_t argc)
{
    jerry_value_t promise = jerry_create_object();
    struct ocf_handler* h;
    jerry_value_t request = argv[0];

    if (!jerry_value_is_object(request)) {
        ERR_PRINT("first parameter must be request object\n");
        return ZJS_UNDEFINED;
    }

    jerry_value_t data;

    if (argc > 2) {
        data = argv[2];
    } else {
        data = ZJS_UNDEFINED;
    }

    if (!jerry_get_object_native_handle(request, (uintptr_t*)&h)) {
        ERR_PRINT("native handle not found\n");
        REJECT(promise, "TypeMismatchError", "native handle not found", h);
        return promise;
    }

    h->properties = zjs_get_property(data, "properties");
    if (!jerry_value_is_object(h->properties)) {
        ERR_PRINT("'properties' not found in data argument\n");
        REJECT(promise, "TypeMismatchError", "'properties' not found in data argument", h);
        return promise;
    }

    DBG_PRINT("responding to method type=%u, properties=%lu\n", h->resp->method, h->properties);

    zjs_make_promise(promise, NULL, NULL);

    zjs_fulfill_promise(promise, NULL, 0);

    return promise;
}

static void post_get(void* handler)
{
    // ZJS_PRINT("POST GET\n");
}

static void ocf_get_handler(oc_request_t *request, oc_interface_mask_t interface, void* user_data)
{
    struct ocf_handler* h = new_ocf_handler((struct server_resource*)user_data);
    if (!h) {
        ERR_PRINT("handler was NULL\n");
        return;
    }
    h->argv = zjs_malloc(sizeof(jerry_value_t) * 2);
    h->argv[0] = create_request(h->res, OC_GET, h);
    h->argv[1] = jerry_create_boolean(0);
    zjs_trigger_event_now(h->res->object, "retrieve", h->argv, 2, post_get, h);

    if (!jerry_value_is_object(h->properties)) {
        ERR_PRINT("properties is not an object\n");
        oc_send_response(request, OC_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }

    void* ret;
    // Start the root encoding object
    zjs_rep_start_root_object();
    // Encode all properties from resource (argv[0])
    ret = zjs_ocf_props_setup(h->properties, &g_encoder, true);
    zjs_rep_end_root_object();
    // Free property return handle
    zjs_ocf_free_props(ret);

    oc_send_response(request, OC_STATUS_OK);

    DBG_PRINT("sent GET response, code=OK\n");

    zjs_free(h->argv);
    zjs_free(h);
}

static void post_put(void* handler)
{
    // ZJS_PRINT("POST PUT\n");
}

static void ocf_put_handler(oc_request_t *request, oc_interface_mask_t interface, void* user_data)
{
    struct ocf_handler* h = new_ocf_handler((struct server_resource*)user_data);
    if (!h) {
        ERR_PRINT("handler was NULL\n");
        return;
    }
    h->argv = zjs_malloc(sizeof(jerry_value_t));
    jerry_value_t request_val = create_request(h->res, OC_PUT, h);
    jerry_value_t props_val = request_to_jerry_value(request);
    jerry_value_t resource_val = jerry_create_object();

    zjs_set_property(resource_val, "properties", props_val);
    zjs_set_property(request_val, "resource", resource_val);

    h->argv[0] = request_val;
    zjs_trigger_event_now(h->res->object, "update", h->argv, 1, post_put, h);

    oc_send_response(request, OC_STATUS_CHANGED);

    DBG_PRINT("sent PUT response, code=CHANGED\n");

    jerry_release_value(props_val);
    jerry_release_value(resource_val);
    jerry_release_value(request_val);
    zjs_free(h->argv);
    zjs_free(h);
}

static void post_delete(void* handler)
{
    // ZJS_PRINT("POST DELETE\n");
}

static void ocf_delete_handler(oc_request_t *request, oc_interface_mask_t interface, void* user_data)
{
    struct ocf_handler* h = new_ocf_handler((struct server_resource*)user_data);
    if (!h) {
        ERR_PRINT("handler was NULL\n");
        return;
    }
    zjs_trigger_event_now(h->res->object, "delete", NULL, 0, post_delete, h);

    oc_send_response(request, OC_STATUS_DELETED);

    DBG_PRINT("sent DELETE response, code=OC_STATUS_DELETED\n");
}

static void ocf_post_handler(oc_request_t *request, oc_interface_mask_t interface, void* user_data)
{
    // ZJS_PRINT("ocf_post_handler(): POST\n");
}

static jerry_value_t ocf_notify(const jerry_value_t function_val,
                                const jerry_value_t this,
                                const jerry_value_t argv[],
                                const jerry_length_t argc)
{
    struct server_resource* resource;
    if (!jerry_get_object_native_handle(argv[0], (uintptr_t*)&resource)) {
        DBG_PRINT("native handle not found\n");
        return ZJS_UNDEFINED;
    }
    DBG_PRINT("path=%s\n", resource->resource_path);
    oc_notify_observers(resource->res);

    return ZJS_UNDEFINED;
}

static jerry_value_t ocf_register(const jerry_value_t function_val,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    struct server_resource* resource;
    int i;
    jerry_value_t promise = jerry_create_object();
    struct ocf_handler* h;

    if (!jerry_value_is_object(argv[0])) {
        ERR_PRINT("first parameter must be resource object\n");
        REJECT(promise, "TypeMismatchError", "first parameter must be resource object", h);
        return promise;
    }
    // Required
    jerry_value_t resource_path_val = zjs_get_property(argv[0], "resourcePath");
    if (!jerry_value_is_string(resource_path_val)) {
        ERR_PRINT("resourcePath not found\n");
        REJECT(promise, "TypeMismatchError", "resourcePath not found", h);
        return promise;
    }
    ZJS_GET_STRING(resource_path_val, resource_path);

    jerry_value_t res_type_array = zjs_get_property(argv[0], "resourceTypes");
    if (!jerry_value_is_array(res_type_array)) {
        ERR_PRINT("resourceTypes array not found\n");
        REJECT(promise, "TypeMismatchError", "resourceTypes array not found", h);
        return promise;
    }
    uint32_t num_types = jerry_get_array_length(res_type_array);

    // Optional
    uint32_t flags = 0;
    jerry_value_t observable_val = zjs_get_property(argv[0], "observable");
    if (jerry_value_is_boolean(observable_val)) {
        if (jerry_get_boolean_value(observable_val)) {
            flags |= FLAG_OBSERVE;
        }
    }
    jerry_value_t discoverable_val = zjs_get_property(argv[0], "discoverable");
    if (jerry_value_is_boolean(discoverable_val)) {
        if (jerry_get_boolean_value(discoverable_val)) {
            flags |= FLAG_DISCOVERABLE;
        }
    }
    jerry_value_t slow_val = zjs_get_property(argv[0], "slow");
    if (jerry_value_is_boolean(slow_val)) {
        if (jerry_get_boolean_value(slow_val)) {
            flags |= FLAG_SLOW;
        }
    }
    jerry_value_t secure_val = zjs_get_property(argv[0], "secure");
    if (jerry_value_is_boolean(secure_val)) {
        if (jerry_get_boolean_value(secure_val)) {
            flags |= FLAG_SECURE;
        }
    }

    resource = new_server_resource(resource_path);

    resource->res = oc_new_resource(resource_path, num_types, 0);

    for (i = 0; i < num_types; ++i) {
        jerry_value_t type_val = jerry_get_property_by_index(res_type_array, i);
        ZJS_GET_STRING(type_val, type_name);
        oc_resource_bind_resource_type(resource->res, type_name);
    }
    oc_resource_bind_resource_interface(resource->res, OC_IF_RW);
    oc_resource_set_default_interface(resource->res, OC_IF_RW);

    if (flags & FLAG_DISCOVERABLE) {
        oc_resource_set_discoverable(resource->res, 1);
    }
    if (flags & FLAG_OBSERVE) {
        oc_resource_set_periodic_observable(resource->res, 1);
    }
    oc_resource_set_request_handler(resource->res, OC_GET, ocf_get_handler, resource);
    oc_resource_set_request_handler(resource->res, OC_PUT, ocf_put_handler, resource);
    oc_resource_set_request_handler(resource->res, OC_DELETE, ocf_delete_handler, resource);
    oc_resource_set_request_handler(resource->res, OC_POST, ocf_post_handler, resource);
    oc_add_resource(resource->res);

    h = new_ocf_handler(resource);
    zjs_make_promise(promise, post_ocf_promise, h);
    h->promise_obj = promise;
    h->argv = zjs_malloc(sizeof(jerry_value_t));
    h->argv[0] = create_resource(resource_path, argv[0]);
    zjs_fulfill_promise(promise, h->argv, 1);

    resource->object = this;

    jerry_set_object_native_handle(h->argv[0], (uintptr_t)resource, NULL);

    DBG_PRINT("registered resource, path=%s\n", resource_path);

    return promise;
}

/*
 * TODO: This gets called by iotivity-constrained before we have a chance to
 *       get any of the resource information. If we register nothing, there is
 *       a seg-fault so we just have an empty function.
 */
void zjs_ocf_register_resources(void)
{
    // ZJS_PRINT("zjs_ocf_register_resources()\n");
}

jerry_value_t zjs_ocf_server_init()
{
    jerry_value_t server = jerry_create_object();

    zjs_obj_add_function(server, ocf_register, "register");
    zjs_obj_add_function(server, ocf_respond, "respond");
    zjs_obj_add_function(server, ocf_notify, "notify");

    zjs_make_event(server);

    return server;
}

#endif // BUILD_MODULE_OCF
