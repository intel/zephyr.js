// Copyright (c) 2017, Intel Corporation.
#ifdef BUILD_MODULE_NET

// ZJS includes
#include "zjs_util.h"
#include "zjs_buffer.h"
#include "zjs_callbacks.h"
#include "zjs_event.h"
#include "zjs_modules.h"
#include "zjs_net_config.h"

#include <zephyr.h>
#include <sections.h>
#include <errno.h>

#include <net/nbuf.h>
#include <net/net_if.h>
#include <net/net_core.h>
#include <net/net_context.h>
#include <net/net_mgmt.h>

#if defined(CONFIG_NET_L2_BLUETOOTH)
#include <bluetooth/bluetooth.h>
#include <gatt/ipss.h>
#include <bluetooth/conn.h>
#endif

/**
 * Net module
 * @module net
 * @namespace Net
 */
/**
 * Address object
 * @name AddressObject
 * @typedef {Object} AddressObject
 * @property {string} address - IP address
 * @property {number} port - Port
 * @property {string} family - IP address family
 */

/**
 * Close event. Triggered when a server has closed
 *
 * @memberof Net.Server
 * @event close
 */
/**
 * Connection event. Triggered when the server has a new connection
 *
 * @memberof Net.Server
 * @event connection
 * @param {Socket} socket - New socket connection
 */
/**
 * Error event. Triggered when there was an error on the server
 *
 * @memberof Net.Server
 * @event error
 */
/**
 * Listening event. Triggered when the sever has started listening
 *
 * @memberof Net.Server
 * @event listening
 */

/**
 * Close event. Triggered when the socket has closed
 *
 * @memberof Net.Socket
 * @event close
 */
/**
 * Connect event. Triggered when the socket has connected to a remote
 *
 * @memberof Net.Socket
 * @event connect
 */
/**
 * Data event. Triggered when there is data available on the socket
 *
 * @memberof Net.Socket
 * @event data
 *
 * @param {Buffer} data
 */
/**
 * Timeout event. Triggered when the socket has timed out
 *
 * @memberof Net.Socket
 * @event timeout
 */

#define MAX_DBG_PRINT 64
static jerry_value_t zjs_net_prototype;
static jerry_value_t zjs_net_socket_prototype;
static jerry_value_t zjs_net_server_prototype;

typedef struct net_handle {
    struct net_context *tcp_sock;
    jerry_value_t server;
    struct sockaddr local;
    uint16_t port;
    uint8_t listening;
} net_handle_t;

typedef struct sock_handle {
    net_handle_t *handle;
    struct net_context *tcp_sock;
    struct sockaddr remote;
    jerry_value_t socket;
    jerry_value_t connect_listener;
    void *rptr;
    void *wptr;
    struct sock_handle *next;
    struct k_timer timer;
    uint32_t timeout;
    zjs_callback_id tcp_read_id;
    zjs_callback_id tcp_connect_id;
    zjs_callback_id tcp_timeout_id;
    uint8_t bound;
    uint8_t paused;
    uint8_t *rbuf;
    uint8_t timer_started;
} sock_handle_t;

static sock_handle_t *opened_sockets = NULL;

static const jerry_object_native_info_t socket_type_info =
{
   .free_cb = free_handle_nop
};

static const jerry_object_native_info_t net_type_info =
{
   .free_cb = free_handle_nop
};

#define CHECK(x) \
    ret = (x); if (ret < 0) { ERR_PRINT("Error in " #x ": %d\n", ret); return zjs_error(#x); }

#define NET_DEFAULT_MAX_CONNECTIONS 5
#define NET_HOSTNAME_MAX            32
#define SOCK_READ_BUF_SIZE          128

static void tcp_c_timeout_callback(void *h, const void *args)
{
    sock_handle_t *sock_handle = (sock_handle_t *)h;
    if (sock_handle && opened_sockets) {
        zjs_trigger_event(sock_handle->socket, "timeout", NULL, 0, NULL, NULL);
        k_timer_stop(&sock_handle->timer);
        // TODO: This may not be correct, but if we don't set it, then more
        //       timeouts will get added, potentially after the socket has been
        //       closed
        sock_handle->timeout = 0;
        DBG_PRINT("socket timed out\n");
    }
}

static void socket_timeout_callback(struct k_timer *timer)
{
    sock_handle_t *cur = opened_sockets;
    while (cur) {
        if (&cur->timer == timer) {
            break;
        }
    }
    zjs_signal_callback(cur->tcp_timeout_id, NULL, 0);
}

/*
 * initialize, start, re-start or stop a socket timeout. 'time' is the timeout
 * for the socket:
 *
 * time = 0     If a timeout has not been started this has no effect
 *              If a timeout has been started this will stop it
 * time > 0     Will start a timeout for the socket
 */
static void start_socket_timeout(sock_handle_t *handle, uint32_t time)
{
    if (time) {
        if (!handle->timer_started) {
            // time has not been started
            k_timer_init(&handle->timer, socket_timeout_callback, NULL);
            handle->timer_started = 1;
        }
        k_timer_start(&handle->timer, time, time);
        if (handle->tcp_timeout_id == -1) {
            handle->tcp_timeout_id = zjs_add_c_callback(handle, tcp_c_timeout_callback);
        }
        DBG_PRINT("starting socket timeout: %u\n", time);
    } else if (handle->timer_started) {
        DBG_PRINT("stoping socket timeout\n");
        k_timer_stop(&handle->timer);
    }
    handle->timeout = time;
}

static void tcp_c_callback(void *h, const void *args)
{
    sock_handle_t *handle = (sock_handle_t *)h;
    if (handle) {
        // find length of unconsumed data in read buffer
        uint32_t len = handle->wptr - handle->rptr;
        zjs_buffer_t *zbuf;
        ZVAL data_buf = zjs_buffer_create(len, &zbuf);
        // copy data from read buffer
        memcpy(zbuf->buffer, handle->rptr, len);
        handle->rptr = handle->wptr = handle->rbuf;

        zjs_trigger_event(handle->socket, "data", &data_buf, 1, NULL, NULL);

        zjs_remove_callback(handle->tcp_read_id);
    } else {
        ERR_PRINT("handle is NULL\n");
    }
}

static void post_server_closed(void *handle)
{
    net_handle_t *h = (net_handle_t *)handle;
    if (h) {
        DBG_PRINT("closing server\n");
        net_context_put(h->tcp_sock);
        zjs_free(h);
    }
}

static void post_closed(void *handle)
{
    sock_handle_t *h = (sock_handle_t *)handle;
    if (h) {
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
        if (net) {
            if (net->listening == 0 && opened_sockets == NULL) {
                // no more sockets open and not listening, close server
                zjs_trigger_event(net->server, "close", NULL, 0, post_server_closed, net);
                DBG_PRINT("server signaled to close\n");
            }
        }
    }
}

static void tcp_received(struct net_context *context,
                         struct net_buf *buf,
                         int status,
                         void *user_data)
{
    sock_handle_t *handle = (sock_handle_t *)user_data;

    if (handle && buf) {
        start_socket_timeout(handle, handle->timeout);

        uint32_t len = net_nbuf_appdatalen(buf);
        uint8_t *data = net_nbuf_appdata(buf);

        // TODO: Probably not the right way to check if a socket is closed
        if (len == 0 && data == NULL) {
            // socket close
            DBG_PRINT("closing socket, context=%p, socket=%u\n", context, handle->socket);
            ZVAL_MUTABLE error = zjs_custom_error("ReadError",  "socket has been closed");
            jerry_value_clear_error_flag(&error);
            zjs_trigger_event(handle->socket, "error", &error, 1, NULL, NULL);
            zjs_trigger_event(handle->socket, "close", NULL, 0, post_closed, handle);
            net_buf_unref(buf);
            return;
        }

        if (len && data) {
            DBG_PRINT("received data, context=%p, data=%p, len=%u\n", context, data, len);

            memcpy(handle->wptr, data, len);
            handle->wptr += len;

            // if not paused, call the callback to get JS the data
            if (!handle->paused) {
                handle->tcp_read_id = zjs_add_c_callback(handle, tcp_c_callback);
                zjs_signal_callback(handle->tcp_read_id, NULL, 0);

                DBG_PRINT("data recieved on context %p: data=%p, len=%u\n", context, data, len);
            }
        }

        net_buf_unref(buf);
    }
}

static inline void pkt_sent(struct net_context *context,
                            int status,
                            void *token,
                            void *user_data)
{
    if (!status) {
        int sent = POINTER_TO_UINT(token);
        DBG_PRINT("Sent %d bytes\n", sent);
        if (sent) {
            zjs_callback_id id = POINTER_TO_INT(user_data);
            if (id != -1) {
                zjs_signal_callback(id, NULL, 0);
            }
        }
    }
}

/**
 * Write data to a socket
 *
 * @name write
 * @memberof Net.Socket
 * @param {Buffer} buf - Buffer being written to the socket
 * @param {function=} func - Callback called when write has completed
 */
static jerry_value_t socket_write(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    ZJS_VALIDATE_ARGS_OPTCOUNT(optcount, Z_OBJECT, Z_OPTIONAL Z_FUNCTION);

    ZJS_GET_HANDLE(this, sock_handle_t, handle, socket_type_info);

    start_socket_timeout(handle, handle->timeout);

    zjs_buffer_t *buf = zjs_buffer_find(argv[0]);
    struct net_buf *send_buf;
    send_buf = net_nbuf_get_tx(handle->tcp_sock, K_NO_WAIT);
    if (!send_buf) {
        ERR_PRINT("cannot acquire send_buf\n");
        return jerry_create_boolean(false);
    }

    bool status = net_nbuf_append(send_buf, buf->bufsize, buf->buffer, K_NO_WAIT);
    if (!status) {
        net_nbuf_unref(send_buf);
        ERR_PRINT("cannot populate send_buf\n");
        return jerry_create_boolean(false);
    }

    zjs_callback_id id = -1;
    if (optcount) {
        id = zjs_add_callback_once(argv[1], this, NULL, NULL);
    }
    int ret = net_context_send(send_buf, pkt_sent,
                               K_NO_WAIT,
                               UINT_TO_POINTER(net_buf_frags_len(send_buf)),
                               INT_TO_POINTER((int32_t)id));
    if (ret < 0) {
        ERR_PRINT("Cannot send data to peer (%d)", ret);
        net_nbuf_unref(send_buf);
        zjs_remove_callback(id);
        // TODO: may need to check the specific error to determine action
        DBG_PRINT("write failed, context=%p, socket=%u\n", handle->tcp_sock, handle->socket);
        ZVAL_MUTABLE error = zjs_custom_error("WriteError", "error writing to socket");
        jerry_value_clear_error_flag(&error);
        zjs_trigger_event(handle->socket, "error", &error, 1, post_closed, handle);
        return jerry_create_boolean(false);
    }

    net_nbuf_unref(send_buf);

    return jerry_create_boolean(true);
}

/**
 * Pause/throttle 'data' callback on the socket. Calling this will prevent
 * the 'data' callback from getting called until Socket.resume() is called.
 *
 * @name pause
 * @memberof Net.Socket
 */
static jerry_value_t socket_pause(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    ZJS_GET_HANDLE(this, sock_handle_t, handle, socket_type_info);
    handle->paused = 1;
    return ZJS_UNDEFINED;
}

/**
 * Resume the 'data' callback. Calling this will un-pause the socket and
 * allow the 'data' callback to resume being called.
 *
 * @name resume
 * @memberof Net.Socket
 */
static jerry_value_t socket_resume(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc)
{
    ZJS_GET_HANDLE(this, sock_handle_t, handle, socket_type_info);
    handle->paused = 0;
    return ZJS_UNDEFINED;
}

/**
 * Retrieve address information from the socket
 *
 * @name address
 * @memberof Net.Socket
 * @return {AddressObject}
 */
static jerry_value_t socket_address(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc)
{
    ZJS_GET_HANDLE(this, sock_handle_t, handle, socket_type_info);
    jerry_value_t ret = jerry_create_object();
    ZVAL port = zjs_get_property(this, "localPort");
    ZVAL addr = zjs_get_property(this, "localAddress");
    sa_family_t family = net_context_get_family(handle->tcp_sock);

    zjs_set_property(ret, "port", port);
    zjs_set_property(ret, "address", addr);
    if (family == AF_INET6) {
        zjs_obj_add_string(ret, "IPv6", "family");
    } else {
        zjs_obj_add_string(ret, "IPv4", "family");
    }

    return ret;
}

/**
 * Set a timeout on a socket. The timeout expires when there has been no
 * activity on the socket for the set number of milliseconds.
 *
 * @name setTimeout
 * @memberof Net.Socket
 * @param {number} time - The timeout in milliseconds
 * @param {function=} callback - Callback function when the timeout expires.
 *                               If supplied, this will be set as the listener
 *                               for the 'timeout' event
 * @return {Socket} socket
 */
static jerry_value_t socket_set_timeout(const jerry_value_t function_obj,
                                        const jerry_value_t this,
                                        const jerry_value_t argv[],
                                        const jerry_length_t argc)
{
    ZJS_VALIDATE_ARGS_OPTCOUNT(optcount, Z_NUMBER, Z_OPTIONAL Z_FUNCTION);

    ZJS_GET_HANDLE(this, sock_handle_t, handle, socket_type_info);

    uint32_t time = (uint32_t)jerry_get_number_value(argv[0]);

    start_socket_timeout(handle, time);

    if (optcount) {
        zjs_add_event_listener(this, "timeout", argv[1]);
    }

    return jerry_acquire_value(this);
}

static jerry_value_t socket_connect(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc);

/*
 * Create a new socket object with needed methods. If 'client' is true,
 * a 'connect()' method will be added (client mode). The socket native handle
 * is returned as an out parameter.
 */
static jerry_value_t create_socket(uint8_t client, sock_handle_t **handle_out)
{
    sock_handle_t *sock_handle = zjs_malloc(sizeof(sock_handle_t));
    if (!sock_handle) {
        return zjs_error("could not alloc socket handle");
    }
    memset(sock_handle, 0, sizeof(sock_handle_t));

    jerry_value_t socket = jerry_create_object();

    if (client) {
        // only a new client socket has connect method
        zjs_obj_add_function(socket, socket_connect, "connect");
    }

    jerry_set_object_native_pointer(socket, sock_handle, &socket_type_info);
    sock_handle->connect_listener = ZJS_UNDEFINED;
    sock_handle->socket = socket;
    sock_handle->tcp_connect_id = -1;
    sock_handle->tcp_read_id = -1;
    sock_handle->tcp_timeout_id = -1;
    sock_handle->rbuf = zjs_malloc(SOCK_READ_BUF_SIZE);
    sock_handle->rptr = sock_handle->wptr = sock_handle->rbuf;

    zjs_make_event(socket, zjs_net_socket_prototype);

    *handle_out = sock_handle;

    return socket;
}

/*
 * Add extra connection information to a socket object. This should be called
 * once a new connection is accepted to the server.
 */
static void add_socket_connection(jerry_value_t socket,
                                  net_handle_t *net,
                                  struct net_context *new,
                                  struct sockaddr *remote)
{
    sock_handle_t *handle = NULL;
    const jerry_object_native_info_t *tmp;
    if (!jerry_get_object_native_pointer(socket, (void **)&handle, &tmp)) {
        ERR_PRINT("could not get socket handle\n");
        return;
    }
    if (tmp != &socket_type_info) {
        ERR_PRINT("handle was incorrect type");
        return;
    }

    memcpy(&handle->remote, remote, sizeof(struct sockaddr));
    handle->handle = net;
    handle->tcp_sock = new;

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
    if (family == AF_INET6) {
        zjs_obj_add_string(socket, "IPv6", "family");
    } else {
        zjs_obj_add_string(socket, "IPv4", "family");
    }
}

static void post_connection(void *handle)
{
    sock_handle_t *sock_handle = (sock_handle_t *)handle;
    int ret = net_context_recv(sock_handle->tcp_sock, tcp_received, 0, sock_handle);
    if (ret < 0) {
        ERR_PRINT("Cannot receive TCP packet (family %d)\n",
                net_context_get_family(sock_handle->tcp_sock));
        // this seems to mean the remote exists but the connection was not made
        zjs_trigger_event(sock_handle->handle->server, "error", NULL, 0, NULL, NULL);
        return;
    }
}

static void tcp_accepted(struct net_context *context,
                         struct sockaddr *addr,
                         socklen_t addrlen,
                         int error,
                         void *user_data)
{
    net_handle_t *handle = (net_handle_t *)user_data;
    sock_handle_t *sock_handle = NULL;

    DBG_PRINT("connection made, context %p error %d\n", context, error);

    ZVAL sock = create_socket(false, &sock_handle);
    if (!sock_handle) {
        ERR_PRINT("could not allocate socket handle\n");
        return;
    }

    add_socket_connection(sock, handle, context, addr);

    // add new socket to list
    sock_handle->next = opened_sockets;
    opened_sockets = sock_handle;

    zjs_trigger_event(handle->server, "connection", &sock, 1, post_connection, sock_handle);
}

/**
 * Signal the server to close. Any opened sockets will remain open, and the
 * 'close' event will be called when these remaining sockets are closed.
 *
 * @name close
 * @memberof Net.Server
 * @param {function?} Callback function. Called when server is closed
 */
static jerry_value_t server_close(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    ZJS_VALIDATE_ARGS_OPTCOUNT(optcount, Z_OPTIONAL Z_FUNCTION);

    ZJS_GET_HANDLE(this, net_handle_t, handle, net_type_info);

    handle->listening = 0;
    zjs_obj_add_boolean(this, false, "listening");

    if (optcount) {
        zjs_add_event_listener(handle->server, "close", argv[0]);
    }
    // If there are no connections the server can be closed
    if (opened_sockets == NULL) {
        zjs_trigger_event(handle->server, "close", NULL, 0, post_server_closed, handle);
        DBG_PRINT("server signaled to close\n");
    }
    return ZJS_UNDEFINED;
}

/**
 * Get the number of connections on this server
 *
 * @name getConnections
 * @memberof Net.Server
 * @param {function} Callback function. Called with the number of opened connections
 */
static jerry_value_t server_get_connections(const jerry_value_t function_obj,
                                            const jerry_value_t this,
                                            const jerry_value_t argv[],
                                            const jerry_length_t argc)
{
    ZJS_VALIDATE_ARGS(Z_FUNCTION);

    int count = 0;
    ZJS_GET_HANDLE(this, net_handle_t, handle, net_type_info);

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

/**
 * Start listening for connections
 *
 * @name listen
 * @memberof Net.Server
 *
 * @param {ListenOptions} options - Options for listening
 * @param {function?} listener - Listener for 'listening' event
 */
static jerry_value_t server_listen(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc)
{
    // options object, optional function
    ZJS_VALIDATE_ARGS_OPTCOUNT(optcount, Z_OBJECT, Z_OPTIONAL Z_FUNCTION);

    ZJS_GET_HANDLE(this, net_handle_t, handle, net_type_info);

    int ret;
    double port = 0;
    double backlog = 0;
    uint32_t size = NET_HOSTNAME_MAX;
    char hostname[size];

    zjs_obj_get_double(argv[0], "port", &port);
    zjs_obj_get_double(argv[0], "backlog", &backlog);
    zjs_obj_get_string(argv[0], "host", hostname, size);

    if (optcount) {
        zjs_add_event_listener(this, "listening", argv[1]);
    }

    struct sockaddr_in6 my_addr6 = { 0 };

    my_addr6.sin6_family = AF_INET6;
    my_addr6.sin6_port = htons((int)port);

    CHECK(net_context_bind(handle->tcp_sock,
                           (struct sockaddr *)&my_addr6,
                           sizeof(struct sockaddr_in6)));
    CHECK(net_context_listen(handle->tcp_sock, (int)backlog));

    handle->listening = 1;
    handle->port = (uint16_t)port;
    memcpy(&handle->local, &my_addr6, sizeof(struct sockaddr *));
    zjs_obj_add_boolean(this, true, "listening");

    zjs_trigger_event(this, "listening", NULL, 0, NULL, NULL);

    CHECK(net_context_accept(handle->tcp_sock, tcp_accepted, 0, handle));

    DBG_PRINT("listening for connection to %s:%u\n", hostname, port);

    return ZJS_UNDEFINED;
}

/**
 * Create a TCP server
 *
 * @memberof Net
 * @name Server
 * @fires close
 * @fires connection
 * @fires error
 * @fires listening
 *
 * @param {function?} listener - Connection listener
 *
 * @return {Server} server - Newly created server
 */
static jerry_value_t net_create_server(const jerry_value_t function_obj,
                                       const jerry_value_t this,
                                       const jerry_value_t argv[],
                                       const jerry_length_t argc)
{
    ZJS_VALIDATE_ARGS_OPTCOUNT(optcount, Z_OPTIONAL Z_FUNCTION);

    int ret;
    jerry_value_t server = jerry_create_object();

    zjs_obj_add_boolean(server, false, "listening");
    zjs_obj_add_number(server, NET_DEFAULT_MAX_CONNECTIONS, "maxConnections");

    zjs_make_event(server, zjs_net_server_prototype);

    if (optcount) {
        zjs_add_event_listener(server, "connection", argv[0]);
    }

    net_handle_t *handle = zjs_malloc(sizeof(net_handle_t));
    if (!handle) {
        jerry_release_value(server);
        return zjs_error("could not alloc server handle");
    }

    CHECK(net_context_get(AF_INET6, SOCK_STREAM, IPPROTO_TCP,
            &handle->tcp_sock))

    jerry_set_object_native_pointer(server, handle, &net_type_info);

    handle->server = server;
    handle->listening = 0;

    DBG_PRINT("creating server: context=%p\n", handle->tcp_sock);

    return server;
}

static void tcp_connected_c_callback(void *handle, const void *args)
{
    sock_handle_t *sock_handle = (sock_handle_t *)handle;
    if (sock_handle) {
        // set socket.connecting property == false
        zjs_obj_add_boolean(sock_handle->socket, false, "connecting");
        zjs_add_event_listener(sock_handle->socket, "connect", sock_handle->connect_listener);
        zjs_trigger_event(sock_handle->socket, "connect", NULL, 0, NULL, NULL);
        zjs_remove_callback(sock_handle->tcp_connect_id);
    }
}

static void tcp_connected(struct net_context *context,
                          int status,
                          void *user_data)
{
    if (status == 0) {
        sock_handle_t *sock_handle = (sock_handle_t *)user_data;

        if (sock_handle) {
            int ret;
            ret = net_context_recv(context, tcp_received, 0, sock_handle);
            if (ret < 0) {
                ERR_PRINT("Cannot receive TCP packets (%d)\n", ret);
            }
            // activity, restart timeout
            start_socket_timeout(sock_handle, sock_handle->timeout);

            sock_handle->tcp_connect_id = zjs_add_c_callback(sock_handle, tcp_connected_c_callback);
            zjs_signal_callback(sock_handle->tcp_connect_id, NULL, 0);

            DBG_PRINT("connection success, context=%p, socket=%u\n", context, sock_handle->socket);
        }
    } else {
        DBG_PRINT("connect failed, status=%d\n", status);
    }
}

/**
 * Connect to a remote server
 *
 * @name connect
 * @memberof Net.Socket
 *
 * @param {ConnectOptions} options
 * @param {function?} listener - Connect listener callback
 */
static jerry_value_t socket_connect(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc)
{
    ZJS_VALIDATE_ARGS(Z_OBJECT, Z_OPTIONAL Z_FUNCTION);

    int ret;
    ZJS_GET_HANDLE(this, sock_handle_t, handle, socket_type_info);
    if (!handle->tcp_sock) {
        CHECK(net_context_get(AF_INET6, SOCK_STREAM, IPPROTO_TCP,
                    &handle->tcp_sock));
    }
    if (!handle->tcp_sock) {
        DBG_PRINT("connect failed\n");
        ZVAL_MUTABLE error = zjs_custom_error("NotFoundError", "Connection could not be made");
        jerry_value_clear_error_flag(&error);
        zjs_trigger_event(this, "error", &error, 1, NULL, NULL);
        return ZJS_UNDEFINED;
    }

    if (argc > 1) {
        jerry_release_value(handle->connect_listener);
        handle->connect_listener = jerry_acquire_value(argv[1]);
    }

    double port = 0;
    double localPort = 0;
    double fam = 0;
    char host[128];
    char localAddress[128];

    zjs_obj_get_double(argv[0], "port", &port);
    zjs_obj_get_string(argv[0], "host", host, 128);
    zjs_obj_get_double(argv[0], "localPort", &localPort);
    zjs_obj_get_string(argv[0], "localAddress", localAddress, 128);
    zjs_obj_get_double(argv[0], "family", &fam);
    if (fam == 0) {
        fam = 4;
    }
    // TODO: get: .hints, .lookup

    DBG_PRINT("port: %u, host: %s, localPort: %u, localAddress: %s, socket=%u\n", (uint32_t)port, host, (uint32_t)localPort, localAddress, this);

    if (fam == 6) {
        if (!handle->bound) {
            struct sockaddr_in6 my_addr6 = { 0 };

            my_addr6.sin6_family = AF_INET6;
            my_addr6.sin6_port = htons((uint32_t)localPort);

            CHECK(net_addr_pton(AF_INET6,
                                localAddress,
                                &my_addr6.sin6_addr));
            // bind to our local address
            CHECK(net_context_bind(handle->tcp_sock,
                                   (struct sockaddr *)&my_addr6,
                                   sizeof(struct sockaddr_in6)));
            handle->bound = 1;
        }
        struct sockaddr_in6 peer_addr6 = { 0 };
        peer_addr6.sin6_family = AF_INET6;
        peer_addr6.sin6_port = htons((uint32_t)port);

        CHECK(net_addr_pton(AF_INET6,
                            host,
                            &peer_addr6.sin6_addr));
        // set socket.connecting property == true
        zjs_obj_add_boolean(this, true, "connecting");
        // connect to remote
        if (net_context_connect(handle->tcp_sock,
                                (struct sockaddr *)&peer_addr6,
                                sizeof(peer_addr6),
                                tcp_connected,
                                1,
                                handle) < 0) {
            DBG_PRINT("connect failed\n");
            zjs_obj_add_boolean(this, false, "connecting");
            ZVAL_MUTABLE error = zjs_custom_error("NotFoundError", "Connection could not be made");
            jerry_value_clear_error_flag(&error);
            zjs_trigger_event(this, "error", &error, 1, NULL, NULL);
            return ZJS_UNDEFINED;
        }
    } else {
        if (!handle->bound) {
            struct sockaddr_in my_addr4 = { 0 };

            my_addr4.sin_family = AF_INET;
            my_addr4.sin_port = htons((uint32_t)localPort);
            CHECK(net_addr_pton(AF_INET,
                                localAddress,
                                &my_addr4.sin_addr));
            // bind to our local address
            CHECK(net_context_bind(handle->tcp_sock,
                                   (struct sockaddr *)&my_addr4,
                                   sizeof(struct sockaddr_in)));
            handle->bound = 1;
        }
        struct sockaddr_in peer_addr4 = { 0 };
        peer_addr4.sin_family = AF_INET;
        peer_addr4.sin_port = htons((uint32_t)port);

        CHECK(net_addr_pton(AF_INET,
                            host,
                            &peer_addr4.sin_addr));
        // set socket.connecting property == true
        zjs_obj_add_boolean(this, true, "connecting");
        // connect to remote
        if (net_context_connect(handle->tcp_sock,
                                (struct sockaddr *)&peer_addr4,
                                sizeof(peer_addr4),
                                tcp_connected,
                                1,
                                handle) < 0) {
            DBG_PRINT("connect failed\n");
            zjs_obj_add_boolean(this, false, "connecting");
            ZVAL_MUTABLE error = zjs_custom_error("NotFoundError", "Connection could not be made");
            jerry_value_clear_error_flag(&error);
            zjs_trigger_event(this, "error", &error, 1, NULL, NULL);
            return ZJS_UNDEFINED;
        }
    }

    // add all the socket address information
    zjs_obj_add_string(this, host, "remoteAddress");
    zjs_obj_add_string(this, "IPv6", "remoteFamily");
    zjs_obj_add_number(this, port, "remotePort");

    zjs_obj_add_string(this, localAddress, "localAddress");
    zjs_obj_add_number(this, localPort, "localPort");
    sa_family_t family = net_context_get_family(handle->tcp_sock);
    if (family == AF_INET6) {
        zjs_obj_add_string(this, "IPv6", "family");
    } else {
        zjs_obj_add_string(this, "IPv4", "family");
    }

    return ZJS_UNDEFINED;
}

/**
 * Create a new socket object
 *
 * @namespace Net.Socket
 * @memberof Net
 * @name Socket
 * @fires close
 * @fires connect
 * @fires data
 * @fires timeout
 * @returns {Socket} New socket object created
 */
static jerry_value_t net_socket(const jerry_value_t function_obj,
                                const jerry_value_t this,
                                const jerry_value_t argv[],
                                const jerry_length_t argc)
{
    sock_handle_t *sock_handle = NULL;
    jerry_value_t socket = create_socket(true, &sock_handle);
    if (!sock_handle) {
        return zjs_error("could not alloc socket handle");
    }

    DBG_PRINT("socket created, context=%p, sock=%u\n", sock_handle->tcp_sock, socket);

    return socket;
}

/**
 * Check if input is an IP address
 *
 * @name isIP
 * @memberof Net
 *
 * @param {string} input - Input string
 * @return {number} 0 for invalid strings, 4 for IPv4, 6 for IPv6
 */
static jerry_value_t net_is_ip(const jerry_value_t function_obj,
                               const jerry_value_t this,
                               const jerry_value_t argv[],
                               const jerry_length_t argc)
{
    if (!jerry_value_is_string(argv[0]) || argc < 1) {
        return jerry_create_number(0);
    }
    uint32_t size = 64;
    char ip[size];
    zjs_copy_jstring(argv[0], ip, &size);
    if (!size) {
        return jerry_create_number(0);
    }
    struct sockaddr_in6 tmp = { 0 };

    // check if v6
    if(net_addr_pton(AF_INET6,
                     ip,
                     &tmp.sin6_addr) < 0) {
        // check if v4
        struct sockaddr_in tmp1 = { 0 };
        if(net_addr_pton(AF_INET,
                         ip,
                         &tmp1.sin_addr) < 0) {
            return jerry_create_number(0);
        } else {
            return jerry_create_number(4);
        }
    } else {
        return jerry_create_number(6);
    }
}

/**
 * Check if input is an IPv4 address
 *
 * @name isIPv4
 * @memberof Net
 *
 * @param {string} input - Input string
 * @return {boolean} true if input was IPv4
 */
static jerry_value_t net_is_ip4(const jerry_value_t function_obj,
                                const jerry_value_t this,
                                const jerry_value_t argv[],
                                const jerry_length_t argc)
{
    ZVAL ret = net_is_ip(function_obj, this, argv, argc);
    double v = jerry_get_number_value(ret);
    if (v == 4) {
        return jerry_create_boolean(true);
    }
    return jerry_create_boolean(false);
}

/**
 * Check if input is an IPv6 address
 *
 * @name isIPv6
 * @memberof Net
 *
 * @param {string} input - Input string
 * @return {boolean} true if input was IPv4
 */
static jerry_value_t net_is_ip6(const jerry_value_t function_obj,
                               const jerry_value_t this,
                               const jerry_value_t argv[],
                               const jerry_length_t argc)
{
    ZVAL ret = net_is_ip(function_obj, this, argv, argc);
    double v = jerry_get_number_value(ret);
    if (v == 6) {
        return jerry_create_boolean(true);
    }
    return jerry_create_boolean(false);
}

static jerry_value_t net_obj;

#if defined(CONFIG_NET_L2_BLUETOOTH)
static zjs_callback_id up_id = -1;

static void up_callback(void *handle, const void *args)
{
    zjs_trigger_event(net_obj, "up", NULL, 0, NULL, NULL);
    zjs_remove_callback(up_id);
}

static struct net_mgmt_event_callback cb;
static void event_iface_up(struct net_mgmt_event_callback *cb,
               uint32_t mgmt_event, struct net_if *iface)
{
    zjs_signal_callback(up_id, NULL, 0);
}
#endif

jerry_value_t zjs_net_init()
{
    zjs_net_config();

    zjs_native_func_t net_array[] = {
            { net_create_server, "createServer" },
            { net_socket, "Socket" },
            { net_is_ip, "isIP" },
            { net_is_ip4, "isIPv4" },
            { net_is_ip6, "isIPv6" },
            { NULL, NULL }
    };
    zjs_native_func_t sock_array[] = {
            { socket_address, "address" },
            { socket_write, "write" },
            { socket_pause, "pause" },
            { socket_resume, "resume" },
            { socket_set_timeout, "setTimeout" },
            { NULL, NULL }
    };
    zjs_native_func_t server_array[] = {
            { server_listen, "listen" },
            { server_close, "close" },
            { server_get_connections, "getConnections" },
            { NULL, NULL }
    };
    // Net object prototype
    zjs_net_prototype = jerry_create_object();
    zjs_obj_add_functions(zjs_net_prototype, net_array);

    // Socket object prototype
    zjs_net_socket_prototype = jerry_create_object();
    zjs_obj_add_functions(zjs_net_socket_prototype, sock_array);

    // Server object prototype
    zjs_net_server_prototype = jerry_create_object();
    zjs_obj_add_functions(zjs_net_server_prototype, server_array);

    net_obj = jerry_create_object();
    //jerry_set_prototype(net_obj, zjs_net_prototype);
    zjs_make_event(net_obj, zjs_net_prototype);

    /*
     * TODO: move to net config?
     */
#if defined(CONFIG_NET_L2_BLUETOOTH)
    struct net_if *iface = net_if_get_default();
    if (!atomic_test_bit(iface->flags, NET_IF_UP)) {
        net_mgmt_init_event_callback(&cb, event_iface_up,
                NET_EVENT_IF_UP);
        net_mgmt_add_event_callback(&cb);
    }
    up_id = zjs_add_c_callback(NULL, up_callback);
#endif

    return jerry_acquire_value(net_obj);
}

void zjs_net_cleanup()
{
    jerry_release_value(zjs_net_prototype);
    jerry_release_value(zjs_net_socket_prototype);
    jerry_release_value(zjs_net_server_prototype);
}

#endif // BUILD_MODULE_NET
