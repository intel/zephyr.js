// Copyright (c) 2017, Intel Corporation.
#ifdef BUILD_MODULE_NET

// ZJS includes
#include "zjs_util.h"
#include "zjs_buffer.h"
#include "zjs_callbacks.h"
#include "zjs_event.h"
#include "zjs_modules.h"

#include <zephyr.h>
#include <sections.h>
#include <errno.h>

#include <net/nbuf.h>
#include <net/net_if.h>
#include <net/net_core.h>
#include <net/net_context.h>

#if defined(CONFIG_NET_L2_BLUETOOTH)
#include <bluetooth/bluetooth.h>
#include <gatt/ipss.h>
#endif

#define MAX_DBG_PRINT 64
static jerry_value_t zjs_net_socket_prototype;

typedef struct net_handle {
    struct net_context *tcp_sock;
    uint16_t port;
    jerry_value_t server;
    uint8_t listening;
    struct sockaddr local;
} net_handle_t;

typedef struct sock_handle {
    net_handle_t *handle;
    struct net_context *tcp_sock;
    struct sockaddr remote;
    jerry_value_t socket;
    zjs_callback_id tcp_read_id;
    uint8_t paused;
    uint8_t *rbuf;
    void *rptr;
    void *wptr;
    struct sock_handle *next;
} sock_handle_t;

static sock_handle_t *opened_sockets = NULL;

#define CHECK(x) \
    ret = (x); if (ret < 0) { ERR_PRINT("Error in " #x ": %d\n", ret); return zjs_error(#x); }

#define NET_DEFAULT_MAX_CONNECTIONS 5
#define NET_HOSTNAME_MAX            32
#define SOCK_READ_BUF_SIZE  128

static inline void pkt_sent(struct net_context *context,
                int status,
                void *token,
                void *user_data)
{
    if (!status) {
        DBG_PRINT("Sent %d bytes", POINTER_TO_UINT(token));
    }
}

static void tcp_c_callback(void *h, const void *args)
{
    sock_handle_t *handle = (sock_handle_t*)h;
    uint32_t len = handle->wptr - handle->rptr;
    zjs_buffer_t *zbuf;
    jerry_value_t data_buf = zjs_buffer_create(len, &zbuf);
    memcpy(zbuf->buffer, handle->rptr, len);
    handle->rptr = handle->wptr = handle->rbuf;

    zjs_trigger_event(handle->socket, "data", &data_buf, 1, NULL, NULL);

    zjs_remove_callback(handle->tcp_read_id);
}

static void post_server_closed(void *handle)
{
    net_handle_t *h = (net_handle_t*)handle;
    if (h) {
        DBG_PRINT("closing server");
        net_context_put(h->tcp_sock);
        zjs_free(h);
    }
}

static void post_closed(void *handle)
{
    sock_handle_t *h = (sock_handle_t*)handle;
    net_handle_t *net = h->handle;
    sock_handle_t *cur = opened_sockets;
    if (cur->next == NULL) {
        opened_sockets = NULL;
        net_context_put(cur->tcp_sock);
        jerry_release_value(cur->socket);
        zjs_free(cur->rbuf);
        zjs_free(cur);
        DBG_PRINT("Freed socket: opened_sockets=%p\n", opened_sockets);
    } else {
        while (cur->next) {
            if (cur->next == h) {
                cur->next = cur->next->next;
                net_context_put(cur->tcp_sock);
                jerry_release_value(cur->socket);
                zjs_free(cur->rbuf);
                zjs_free(cur);
                DBG_PRINT("Freed socket: opened_sockets=%p\n", opened_sockets);
            }
        }
    }
    if (net->listening == 0 && opened_sockets == NULL) {
        // no more sockets open and not listening, close server
        zjs_trigger_event(net->server, "close", NULL, 0, post_server_closed, net);
        DBG_PRINT("server signaled to close\n");
    }
}

static void tcp_received(struct net_context *context,
             struct net_buf *buf,
             int status,
             void *user_data)
{

    sock_handle_t *handle = (sock_handle_t*)user_data;

    uint32_t len = net_nbuf_appdatalen(buf);
    uint8_t *data = net_nbuf_appdata(buf);

    /*
     * TODO: Probably not the right way to check if a socket is closed
     */
    if (len == 0 && data == NULL) {
        // socket close
        DBG_PRINT("closing socket %p\n", context);
        zjs_trigger_event(handle->socket, "close", NULL, 0, post_closed, handle);
    }

    if (!handle->paused && len) {
        memcpy(handle->wptr, data, len);
        handle->wptr += len;
        handle->tcp_read_id = zjs_add_c_callback(handle, tcp_c_callback);
        zjs_signal_callback(handle->tcp_read_id, NULL, 0);

        DBG_PRINT("data recieved on context %p: data=%p, len=%u\n", context, data, len);
    }

    net_buf_unref(buf);
}

static jerry_value_t socket_write(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    sock_handle_t *handle = NULL;

    jerry_get_object_native_handle(this, (uintptr_t*)&handle);

    zjs_buffer_t *buf = zjs_buffer_find(argv[0]);
    struct net_buf *send_buf;
    send_buf = net_nbuf_get_tx(handle->tcp_sock, K_NO_WAIT);
    if (!send_buf) {
        ERR_PRINT("cannot acquire send_buf\n");
        return jerry_create_boolean(false);
    }

    bool status = net_nbuf_append(send_buf, buf->bufsize, buf->buffer, K_NO_WAIT);
    if (!status) {
        ERR_PRINT("cannot populate send_buf\n");
        return jerry_create_boolean(false);
    }

    int ret = net_context_send(send_buf, pkt_sent, K_NO_WAIT,
            UINT_TO_POINTER(net_buf_frags_len(send_buf)),
            NULL);
    if (ret < 0) {
        ERR_PRINT("Cannot send data to peer (%d)", ret);
        net_nbuf_unref(send_buf);
        return jerry_create_boolean(false);
    }

    net_nbuf_unref(send_buf);

    return jerry_create_boolean(true);
}

static jerry_value_t socket_pause(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    sock_handle_t *handle = NULL;
    jerry_get_object_native_handle(this, (uintptr_t*)&handle);
    handle->paused = 1;
    return ZJS_UNDEFINED;
}

static jerry_value_t socket_resume(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc)
{
    sock_handle_t *handle = NULL;
    jerry_get_object_native_handle(this, (uintptr_t*)&handle);
    handle->paused = 0;
    return ZJS_UNDEFINED;
}

static jerry_value_t socket_address(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc)
{
    sock_handle_t *handle = NULL;
    jerry_get_object_native_handle(this, (uintptr_t*)&handle);

    sa_family_t family = net_context_get_family(handle->tcp_sock);
    char ip[64];
    net_addr_ntop(family, (const void *)&handle->handle->local, ip, 64);

    jerry_value_t ret = jerry_create_object();

    zjs_obj_add_number(ret, (double)handle->handle->port, "port");
    zjs_obj_add_string(ret, "IPv6", "family");
    zjs_obj_add_string(ret, ip, "address");

    return ret;
}

static jerry_value_t create_socket(net_handle_t *net, struct net_context *new, struct sockaddr *remote, sock_handle_t **handle_out)
{
    jerry_value_t socket = jerry_create_object();

    sock_handle_t *handle = zjs_malloc(sizeof(sock_handle_t));

    memcpy(&handle->remote, remote, sizeof(struct sockaddr));
    handle->handle = net;
    handle->socket = socket;
    handle->rbuf = zjs_malloc(SOCK_READ_BUF_SIZE);
    handle->rptr = handle->wptr = handle->rbuf;
    handle->tcp_sock = new;
    handle->paused = 0;

    zjs_obj_add_function(socket, socket_address, "address");
    zjs_obj_add_function(socket, socket_write, "write");
    zjs_obj_add_function(socket, socket_pause, "pause");
    zjs_obj_add_function(socket, socket_resume, "resume");

    sa_family_t family = net_context_get_family(new);
    char remote_ip[64];
    net_addr_ntop(family, (const void *)remote, remote_ip, 64);

    zjs_obj_add_string(socket, remote_ip, "remoteAddress");
    zjs_obj_add_string(socket, "IPv6", "remoteFamily");
    zjs_obj_add_number(socket, net->port, "remotePort");

    char local_ip[64];
    net_addr_ntop(family, (const void *)&net->local, local_ip, 64);

    zjs_obj_add_string(socket, local_ip, "localAddress");
    zjs_obj_add_number(socket, net->port, "localPort");

    jerry_set_object_native_handle(socket, (uintptr_t)handle, NULL);

    zjs_make_event(socket, ZJS_UNDEFINED);

    *handle_out = handle;

    return socket;
}

static void tcp_accepted(struct net_context *context,
             struct sockaddr *addr,
             socklen_t addrlen,
             int error,
             void *user_data)
{
    net_handle_t *handle = (net_handle_t*)user_data;
    sock_handle_t *sock_handle;
    int ret;

    DBG_PRINT("connection made, context %p error %d\n", context, error);

    ZVAL sock = create_socket(handle, context, addr, &sock_handle);

    ret = net_context_recv(context, tcp_received, 0, sock_handle);
    if (ret < 0) {
        ERR_PRINT("Cannot receive TCP packet (family %d)",
            net_context_get_family(context));
    }

    // add new socket to list
    sock_handle->next = opened_sockets;
    opened_sockets = sock_handle;

    zjs_trigger_event(handle->server, "connection", &sock, 1, NULL, NULL);
    ZJS_PRINT("connection triggered\n");
}

static jerry_value_t server_close(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    net_handle_t *handle = NULL;
    jerry_get_object_native_handle(this, (uintptr_t*)&handle);
    handle->listening = 0;
    zjs_obj_add_boolean(this, false, "listening");

    /*
     * If there are no connections the server can be closed
     */
    if (opened_sockets == NULL) {
        zjs_trigger_event(handle->server, "close", NULL, 0, post_server_closed, handle);
        DBG_PRINT("server signaled to close\n");
    }
    return ZJS_UNDEFINED;
}

static jerry_value_t server_get_connections(const jerry_value_t function_obj,
                                            const jerry_value_t this,
                                            const jerry_value_t argv[],
                                            const jerry_length_t argc)
{
    int count = 0;
    net_handle_t *handle = NULL;
    jerry_get_object_native_handle(this, (uintptr_t*)&handle);

    sock_handle_t *cur = opened_sockets;
    while (cur) {
        if (cur->handle == handle) {
            count++;
        }
    }

    ZVAL err = jerry_create_number(0);
    ZVAL num = jerry_create_number(count);
    jerry_value_t args[2] = {err, num};

    zjs_callback_id id = zjs_add_callback_once(argv[0], this, NULL, NULL);
    zjs_signal_callback(id, args, 2);

    return ZJS_UNDEFINED;
}

static jerry_value_t server_listen(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc)
{
    int ret;
    uint16_t port = 0;
    uint32_t size = NET_HOSTNAME_MAX;
    char hostname[size];
    //jerry_value_t listen_cb = 0;
    int i;
    for (i = 0; i < argc; ++i) {
        if (jerry_value_is_number(argv[i])) {
            port = jerry_get_number_value(argv[i]);
        } else if (jerry_value_is_string(argv[i])) {
            zjs_copy_jstring(argv[i], hostname, &size);
            if (!size) {
                return zjs_error("invalid arguments");
            }
        } else if (jerry_value_is_function(argv[i])) {
            zjs_add_event_listener(this, "listening", argv[i]);
        }
    }

    net_handle_t *handle = NULL;

    jerry_get_object_native_handle(this, (uintptr_t*)&handle);

    struct sockaddr_in6 my_addr6 = { 0 };

    my_addr6.sin6_family = AF_INET6;
    my_addr6.sin6_port = htons(port);

    CHECK(net_context_bind(handle->tcp_sock,
                           (struct sockaddr *)&my_addr6,
                           sizeof(struct sockaddr_in6)));
    CHECK(net_context_listen(handle->tcp_sock, 0));

    handle->listening = 1;
    handle->port = port;
    memcpy(&handle->local, &my_addr6, sizeof(struct sockaddr *));
    zjs_obj_add_boolean(this, true, "listening");

    zjs_trigger_event(this, "listening", NULL, 0, NULL, NULL);

    CHECK(net_context_accept(handle->tcp_sock, tcp_accepted, 0, handle));

    DBG_PRINT("listening for connection to %s:%u\n", hostname, port);

    return ZJS_UNDEFINED;
}


static jerry_value_t zjs_net_create_server(const jerry_value_t function_obj,
                                           const jerry_value_t this,
                                           const jerry_value_t argv[],
                                           const jerry_length_t argc)
{
    int ret;
    jerry_value_t connection_cb = 0;
    if (argc > 0) {
        if (!jerry_value_is_function(argv[0])) {
            return zjs_error("invalid arguments");
        } else {
            connection_cb = argv[0];
        }
    }
    jerry_value_t server = jerry_create_object();

    zjs_obj_add_boolean(server, false, "listening");
    zjs_obj_add_number(server, NET_DEFAULT_MAX_CONNECTIONS, "maxConnections");

    zjs_obj_add_function(server, server_listen, "listen");
    zjs_obj_add_function(server, server_close, "close");
    zjs_obj_add_function(server, server_get_connections, "getConnections");


    zjs_make_event(server, ZJS_UNDEFINED);

    if (connection_cb) {
        zjs_add_event_listener(server, "connection", connection_cb);
    }

    net_handle_t *handle = zjs_malloc(sizeof(net_handle_t));

    CHECK(net_context_get(AF_INET6, SOCK_STREAM, IPPROTO_TCP,
            &handle->tcp_sock))

    jerry_set_object_native_handle(server, (uintptr_t)handle, NULL);

    handle->server = server;
    handle->listening = 0;

    DBG_PRINT("creating server: context=%p\n", handle->tcp_sock);

    return server;
}

jerry_value_t zjs_net_init()
{
#if defined(CONFIG_NET_L2_BLUETOOTH)
    if (bt_enable(NULL)) {
        NET_ERR("Bluetooth init failed");
        return ZJS_UNDEFINED;
    }
    ipss_init();
    ipss_advertise();
#endif

    // TODO: Interface address initialization doesn't belong to this module
    static struct in_addr in4addr_my = { { { 192, 0, 2, 1 } } };
    // 2001:db8::1
    static struct in6_addr in6addr_my = { { { 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0,
                                              0, 0, 0, 0, 0, 0, 0, 0x1 } } };

    net_if_ipv4_addr_add(net_if_get_default(), &in4addr_my, NET_ADDR_MANUAL, 0);
    net_if_ipv6_addr_add(net_if_get_default(), &in6addr_my, NET_ADDR_MANUAL, 0);

    jerry_value_t net_obj = jerry_create_object();
    zjs_obj_add_function(net_obj, zjs_net_create_server, "createServer");

    return net_obj;
}

void zjs_net_cleanup()
{
    jerry_release_value(zjs_net_socket_prototype);
}

#endif // BUILD_MODULE_NET
