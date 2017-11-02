// Copyright (c) 2017, Linaro Limited.

#ifdef BUILD_MODULE_DGRAM

// Zephyr includes
#include <net/net_context.h>
#include <net/net_pkt.h>
#include <net/udp.h>
#if defined(CONFIG_NET_L2_BT)
#include <bluetooth/bluetooth.h>
#endif

// ZJS includes
#include "zjs_buffer.h"
#include "zjs_callbacks.h"
#include "zjs_net_config.h"
#include "zjs_util.h"

static jerry_value_t zjs_dgram_socket_prototype;

typedef struct dgram_handle {
    struct net_context *udp_sock;
    zjs_callback_id message_cb_id;
    zjs_callback_id error_cb_id;
} dgram_handle_t;

#define CHECK(x)                                 \
    ret = (x);                                   \
    if (ret < 0) {                               \
        ERR_PRINT("Error in " #x ": %d\n", ret); \
        return zjs_error(#x);                    \
    }

// FIXME: Quick hack to allow context into the regular CHECK while fixing build
//        for call sites without JS binding context
#define CHECK_ALT(x)                             \
    ret = (x);                                   \
    if (ret < 0) {                               \
        ERR_PRINT("Error in " #x ": %d\n", ret); \
        return zjs_error_context(#x, 0, 0);      \
    }

#define GET_STR(jval, buf)                    \
    {                                         \
        jerry_size_t str_sz = sizeof(buf);    \
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
    CHECK_ALT(net_addr_pton(family, addr_str, &sockaddr_in->sin_addr));
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

static const jerry_object_native_info_t dgram_type_info = {
    .free_cb = zjs_dgram_free_cb
};

// Copy data from Zephyr net_pkt chain into linear buffer
static char *net_pkt_gather(struct net_pkt *pkt, char *to)
{
    struct net_buf *tmp = pkt->frags;
    int header_len = net_pkt_appdata(pkt) - tmp->data;
    net_buf_pull(tmp, header_len);

    while (tmp) {
        memcpy(to, tmp->data, tmp->len);
        to += tmp->len;
        tmp = net_pkt_frag_del(pkt, NULL, tmp);
    }

    return to;
}

// Zephyr "packet received" callback
static void udp_received(struct net_context *context,
                         struct net_pkt *net_pkt,
                         int status,
                         void *user_data)
{
    DBG_PRINT("udp_received: %p, buf=%p, st=%d, appdatalen=%d, udata=%p\n",
              context, net_pkt, status, net_pkt_appdatalen(net_pkt), user_data);

    dgram_handle_t *handle = user_data;
    if (handle->message_cb_id == -1)
        return;

    int recv_len = net_pkt_appdatalen(net_pkt);
    sa_family_t family = net_pkt_family(net_pkt);
    char addr_str[40];

    void *addr;
    if (family == AF_INET) {
        addr = &NET_IPV4_HDR(net_pkt)->src;
    } else {
        addr = &NET_IPV6_HDR(net_pkt)->src;
    }
    net_addr_ntop(family, addr, addr_str, sizeof(addr_str));

    zjs_buffer_t *buf;
    ZVAL_MUTABLE buf_js = zjs_buffer_create(recv_len, &buf);
    ZVAL rinfo = zjs_create_object();
    if (buf) {
        struct net_udp_hdr placeholder;
        struct net_udp_hdr *hdr = net_udp_get_hdr(net_pkt, &placeholder);
        zjs_obj_add_number(rinfo, "port", ntohs(hdr->src_port));
        zjs_obj_add_string(rinfo, "family",
                           family == AF_INET ? "IPv4" : "IPv6");
        zjs_obj_add_string(rinfo, "address", addr_str);

        net_pkt_gather(net_pkt, buf->buffer);
        net_pkt_unref(net_pkt);
    } else {
        // can't pass object with error flag as a JS arg
        jerry_value_clear_error_flag(&buf_js);
    }

    jerry_value_t args[2] = { buf_js, rinfo };
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
    if (strequal(type_str, "udp4"))
        family = AF_INET;
    else if (strequal(type_str, "udp6"))
        family = AF_INET6;
    else
        return zjs_error("invalid argument");

    struct net_context *udp_sock;
    CHECK(net_context_get(family, SOCK_DGRAM, IPPROTO_UDP, &udp_sock));

    jerry_value_t sockobj = zjs_create_object();
    jerry_set_prototype(sockobj, zjs_dgram_socket_prototype);

    dgram_handle_t *handle = zjs_malloc(sizeof(dgram_handle_t));
    if (!handle)
        return zjs_error("out of memory");
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
    if (strequal(event, "message"))
        cb_slot = &handle->message_cb_id;
    else if (strequal(event, "error"))
        cb_slot = &handle->error_cb_id;
    else
        return zjs_error("unsupported event type");

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
            rval = zjs_standard_error(NetworkError, errbuf, 0, 0);
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

    struct net_pkt *send_buf = net_pkt_get_tx(handle->udp_sock, K_NO_WAIT);
    if (!send_buf) {
        return zjs_error("no netbuf");
    }
    if (!net_pkt_append(send_buf, len, buf->buffer + offset, K_NO_WAIT)) {
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

static void zjs_dgram_cleanup(void *native)
{
    jerry_release_value(zjs_dgram_socket_prototype);
}

static const jerry_object_native_info_t dgram_module_type_info = {
   .free_cb = zjs_dgram_cleanup
};

static jerry_value_t zjs_dgram_init()
{
    zjs_net_config_default();

    // create socket prototype object
    zjs_native_func_t array[] = {
        { zjs_dgram_sock_on, "on" },
        { zjs_dgram_sock_send, "send" },
        { zjs_dgram_sock_bind, "bind" },
        { zjs_dgram_sock_close, "close" },
        { NULL, NULL }
    };
    zjs_dgram_socket_prototype = zjs_create_object();
    zjs_obj_add_functions(zjs_dgram_socket_prototype, array);

    // create module object
    jerry_value_t dgram_obj = zjs_create_object();
    zjs_obj_add_function(dgram_obj, "createSocket", zjs_dgram_createSocket);
    // Set up cleanup function for when the object gets freed
    jerry_set_object_native_pointer(dgram_obj, NULL, &dgram_module_type_info);
    return dgram_obj;
}

JERRYX_NATIVE_MODULE(dgram, zjs_dgram_init)
#endif  // BUILD_MODULE_DGRAM
