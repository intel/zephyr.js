// Copyright (c) 2017, Linaro Limited.

#ifdef BUILD_MODULE_DGRAM

// Zephyr includes
#include <net/net_context.h>
#include <net/nbuf.h>
#if defined(CONFIG_NET_L2_BLUETOOTH)
#include <bluetooth/bluetooth.h>
#include <gatt/ipss.h>
#endif

// ZJS includes
#include "zjs_util.h"
#include "zjs_buffer.h"
#include "zjs_callbacks.h"
#include "zjs_net_config.h"

static jerry_value_t zjs_dgram_socket_prototype;

typedef struct dgram_handle {
    struct net_context *udp_sock;
    zjs_callback_id message_cb_id;
    zjs_callback_id error_cb_id;
} dgram_handle_t;

#define CHECK(x) \
    ret = (x); if (ret < 0) { ERR_PRINT("Error in " #x ": %d\n", ret); return zjs_error(#x); }

#define GET_STR(jval, buf) \
    { \
        jerry_size_t str_sz = sizeof(buf); \
        zjs_copy_jstring(jval, buf, &str_sz); \
    }

// Parse textual address of given address family (IPv4/IPv6) and numeric
// port and fill in sockaddr. Returns ZJS_UNDEFINED if everything is OK,
// or error instance otherwise.
static jerry_value_t get_addr(sa_family_t family,
                              const jerry_value_t addr,
                              const jerry_value_t port,
                              struct sockaddr *sockaddr)
{
    // requires: addr must be a string value, and port must be a number value
    int ret;

    // We employ the fact that port and address offsets are the same for IPv4&6
    struct sockaddr_in *sockaddr_in = (struct sockaddr_in *)sockaddr;
    sockaddr_in->sin_family = family;
    sockaddr_in->sin_port = htons((int)jerry_get_number_value(port));

    jerry_size_t str_len = 40;
    char addr_str[str_len];
    zjs_copy_jstring(addr, addr_str, &str_len);
    CHECK(net_addr_pton(family, addr_str, &sockaddr_in->sin_addr));
    return ZJS_UNDEFINED;
}

static void zjs_dgram_free_cb(void *native)
{
    dgram_handle_t *handle = (dgram_handle_t *)native;
    DBG_PRINT("zjs_dgram_free_cb: %p\n", handle);
    if (!handle) {
        return;
    }

    if (handle->udp_sock) {
        int ret = net_context_put(handle->udp_sock);
        if (ret < 0) {
            ERR_PRINT("dgram: net_context_put: err: %d\n", ret);
        }
    }
    zjs_remove_callback(handle->message_cb_id);
    zjs_remove_callback(handle->error_cb_id);
    zjs_free(handle);
}

static const jerry_object_native_info_t dgram_type_info =
{
   .free_cb = zjs_dgram_free_cb
};

// Copy data from Zephyr net_buf chain into linear buffer
static char *net_buf_gather(struct net_buf *buf, char *to)
{
    struct net_buf *tmp = buf->frags;
    int header_len = net_nbuf_appdata(buf) - tmp->data;
    net_buf_pull(tmp, header_len);

    while (tmp) {
        memcpy(to, tmp->data, tmp->len);
        to += tmp->len;
        tmp = net_buf_frag_del(buf, tmp);
    }

    return to;
}

// Zephyr "packet received" callback
static void udp_received(struct net_context *context,
                         struct net_buf *net_buf,
                         int status,
                         void *user_data)
{
    DBG_PRINT("udp_received: %p, buf=%p, st=%d, appdatalen=%d, udata=%p\n",
              context, net_buf, status, net_nbuf_appdatalen(net_buf),
              user_data);

    dgram_handle_t *handle = user_data;
    if (handle->message_cb_id == -1)
        return;

    int recv_len = net_nbuf_appdatalen(net_buf);
    sa_family_t family = net_nbuf_family(net_buf);
    char addr_str[40];

    void *addr;
    if (family == AF_INET) {
        addr = &NET_IPV4_BUF(net_buf)->src;
    } else {
        addr = &NET_IPV6_BUF(net_buf)->src;
    }
    net_addr_ntop(family, addr, addr_str, sizeof(addr_str));

    zjs_buffer_t *buf;
    ZVAL_MUTABLE buf_js = zjs_buffer_create(recv_len, &buf);
    ZVAL rinfo = jerry_create_object();
    if (buf) {
        zjs_obj_add_number(rinfo, ntohs(NET_UDP_BUF(net_buf)->src_port),
                           "port");
        zjs_obj_add_string(rinfo, family == AF_INET ? "IPv4" : "IPv6",
                           "family");
        zjs_obj_add_string(rinfo, addr_str, "address");

        net_buf_gather(net_buf, buf->buffer);
        net_nbuf_unref(net_buf);
    }
    else {
        // can't pass object with error flag as a JS arg
        jerry_value_clear_error_flag(&buf_js);
    }

    jerry_value_t args[2] = {buf_js, rinfo};
    zjs_signal_callback(handle->message_cb_id, args, sizeof(args));
}

static ZJS_DECL_FUNC(zjs_dgram_createSocket)
{
    // args: address family
    ZJS_VALIDATE_ARGS(Z_STRING);

    int ret;

    char type_str[8];
    GET_STR(argv[0], type_str);

    sa_family_t family;
    if (strcmp(type_str, "udp4") == 0)
        family = AF_INET;
    else if (strcmp(type_str, "udp6") == 0)
        family = AF_INET6;
    else
        return zjs_error("createSocket: invalid argument");

    struct net_context *udp_sock;
    CHECK(net_context_get(family, SOCK_DGRAM, IPPROTO_UDP, &udp_sock));

    jerry_value_t sockobj = jerry_create_object();
    jerry_set_prototype(sockobj, zjs_dgram_socket_prototype);

    dgram_handle_t *handle = zjs_malloc(sizeof(dgram_handle_t));
    if (!handle)
        return zjs_error("createSocket: OOM");
    handle->udp_sock = udp_sock;
    handle->message_cb_id = -1;
    handle->error_cb_id = -1;

    jerry_set_object_native_pointer(sockobj, handle, &dgram_type_info);

    // Can't call this here due to bug in Zephyr - called in .bind() instead
    //CHECK(net_context_recv(udp_sock, udp_received, K_NO_WAIT, handle));

    return sockobj;
}

static ZJS_DECL_FUNC(zjs_dgram_sock_on)
{
    // args: event name, callback
    ZJS_VALIDATE_ARGS(Z_STRING, Z_FUNCTION Z_NULL);

    ZJS_GET_HANDLE(this, dgram_handle_t, handle, dgram_type_info);

    jerry_size_t str_sz = 32;
    char event[str_sz];
    zjs_copy_jstring(argv[0], event, &str_sz);

    zjs_callback_id *cb_slot;
    if (!strcmp(event, "message"))
        cb_slot = &handle->message_cb_id;
    else if (!strcmp(event, "error"))
        cb_slot = &handle->error_cb_id;
    else
        return zjs_error("zjs_dgram_sock_on: unsupported event type");

    zjs_remove_callback(*cb_slot);
    if (!jerry_value_is_null(argv[1]))
        *cb_slot = zjs_add_callback(argv[1], this, handle, NULL);

    return ZJS_UNDEFINED;
}

// Zephyr "packet sent" callback
static void udp_sent(struct net_context *context, int status, void *token,
                     void *user_data)
{
    DBG_PRINT("udp_sent: %p, st=%d udata=%p\n", context, status, user_data);

    if (user_data) {
        ZVAL_MUTABLE rval = ZJS_UNDEFINED;
        if (status != 0) {
            char errbuf[8];
            snprintf(errbuf, sizeof(errbuf), "%d", status);
            rval = zjs_standard_error(NetworkError, errbuf);
            // We need error object, not error value (JrS doesn't allow to
            // pass the latter as a func argument).
            jerry_value_clear_error_flag(&rval);
        }

        zjs_callback_id id = zjs_add_callback_once((jerry_value_t)user_data,
                                                   ZJS_UNDEFINED, NULL, NULL);
        zjs_signal_callback(id, &rval, sizeof(rval));
    }
}

static ZJS_DECL_FUNC(zjs_dgram_sock_send)
{
    // NOTE: really argv[1] and argv[2] (offset and length) are supposed to be
    //   optional, which would require improvements to ZJS_VALIDATE_ARGS,
    //   but for now I'll leave them as required, as they were here before

    // args: buffer, offset, length, port, address[, callback]
    ZJS_VALIDATE_ARGS(Z_BUFFER, Z_NUMBER, Z_NUMBER, Z_NUMBER, Z_STRING,
                      Z_OPTIONAL Z_FUNCTION);

    int ret;

    ZJS_GET_HANDLE(this, dgram_handle_t, handle, dgram_type_info);

    zjs_buffer_t *buf = zjs_buffer_find(argv[0]);
    int offset = (int)jerry_get_number_value(argv[1]);
    int len = (int)jerry_get_number_value(argv[2]);
    if (offset + len > buf->bufsize) {
        return zjs_error("offset/len beyond buffer end");
    }

    sa_family_t family = net_context_get_family(handle->udp_sock);
    struct sockaddr sockaddr_buf;
    jerry_value_t err = get_addr(family, argv[4], argv[3], &sockaddr_buf);
    if (err != ZJS_UNDEFINED)
        return err;

    struct net_buf *send_buf = net_nbuf_get_tx(handle->udp_sock, K_NO_WAIT);
    if (!send_buf) {
        return zjs_error("no netbuf");
    }
    if (!net_nbuf_append(send_buf, len, buf->buffer + offset, K_NO_WAIT)) {
        return zjs_error("no data netbuf");
    }

    void *user_data = NULL;
    if (argc > 5) {
        user_data = (void *)argv[5];
    }

    CHECK(net_context_sendto(send_buf, &sockaddr_buf, sizeof(sockaddr_buf),
                             udp_sent, K_NO_WAIT, NULL, user_data));

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_dgram_sock_bind)
{
    // NOTE: really argv[0] and argv[1] are supposed to be optional, but
    //   leaving them as they were here for now

    // args: port, address
    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_STRING);

    int ret;

    ZJS_GET_HANDLE(this, dgram_handle_t, handle, dgram_type_info);

    sa_family_t family = net_context_get_family(handle->udp_sock);
    struct sockaddr sockaddr_buf;
    jerry_value_t err = get_addr(family, argv[1], argv[0], &sockaddr_buf);
    if (err != ZJS_UNDEFINED)
        return err;

    CHECK(net_context_bind(handle->udp_sock, &sockaddr_buf,
                               sizeof(sockaddr_buf)));
    // See comment in createSocket() why this is called here
    CHECK(net_context_recv(handle->udp_sock, udp_received, K_NO_WAIT, handle));

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_dgram_sock_close)
{
    ZJS_GET_HANDLE(this, dgram_handle_t, handle, dgram_type_info);
    zjs_dgram_free_cb((void *)handle);
    return ZJS_UNDEFINED;
}

jerry_value_t zjs_dgram_init()
{
    zjs_net_config();

    // create socket prototype object
    zjs_native_func_t array[] = {
        { zjs_dgram_sock_on, "on" },
        { zjs_dgram_sock_send, "send" },
        { zjs_dgram_sock_bind, "bind" },
        { zjs_dgram_sock_close, "close" },
        { NULL, NULL }
    };
    zjs_dgram_socket_prototype = jerry_create_object();
    zjs_obj_add_functions(zjs_dgram_socket_prototype, array);

    // create module object
    jerry_value_t dgram_obj = jerry_create_object();
    zjs_obj_add_function(dgram_obj, zjs_dgram_createSocket, "createSocket");
    return dgram_obj;
}

void zjs_dgram_cleanup()
{
    jerry_release_value(zjs_dgram_socket_prototype);
}

#endif // BUILD_MODULE_DGRAM
