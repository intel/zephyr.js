// Copyright (c) 2017, Intel Corporation.
#ifdef BUILD_MODULE_NET

// C includes
#include <errno.h>

// Zephyr includes
#include <sections.h>
#include <zephyr.h>

#include <net/net_context.h>
#include <net/net_core.h>
#include <net/net_if.h>
#include <net/net_pkt.h>

// ZJS includes
#include "zjs_buffer.h"
#include "zjs_callbacks.h"
#include "zjs_event.h"
#include "zjs_modules.h"
#include "zjs_net_config.h"
#include "zjs_util.h"

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
    u16_t port;
    u8_t listening;
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
    u32_t timeout;
    u8_t bound;
    u8_t paused;
    u8_t *rbuf;
    u8_t timer_started;
} sock_handle_t;

static sock_handle_t *opened_sockets = NULL;

// get the socket handle from the object or NULL
#define GET_SOCK_HANDLE(obj, var)  \
    sock_handle_t *var = (sock_handle_t *)zjs_event_get_user_handle(obj);

// get the socket handle or return a JS error
#define GET_SOCK_HANDLE_JS(obj, var)                                       \
    sock_handle_t *var = (sock_handle_t *)zjs_event_get_user_handle(obj);  \
    if (!var) { return zjs_error("no socket handle"); }

// get the net handle or return a JS error
#define GET_NET_HANDLE_JS(obj, var)                                      \
    net_handle_t *var = (net_handle_t *)zjs_event_get_user_handle(obj);  \
    if (!var) { return zjs_error("no socket handle"); }

#define CHECK(x)                                 \
    ret = (x);                                   \
    if (ret < 0) {                               \
        ERR_PRINT("Error in " #x ": %d\n", ret); \
        return zjs_error(#x);                    \
    }

#define NET_DEFAULT_MAX_CONNECTIONS 5
#define NET_HOSTNAME_MAX            32
#define SOCK_READ_BUF_SIZE          128

static void socket_timeout_callback(struct k_timer *timer)
{
    sock_handle_t *cur = opened_sockets;
    while (cur) {
        if (&cur->timer == timer) {
            break;
        }
    }
    if (cur) {
        zjs_defer_emit_event(cur->socket, "timeout", NULL, 0, NULL, NULL);
        k_timer_stop(&cur->timer);
        // TODO: This may not be correct, but if we don't set it, then more
        //       timeouts will get added, potentially after the socket has been
        //       closed
        cur->timeout = 0;
        DBG_PRINT("socket timed out\n");
    }
}

/*
 * initialize, start, re-start or stop a socket timeout. 'time' is the timeout
 * for the socket:
 *
 * time = 0     If a timeout has not been started this has no effect
 *              If a timeout has been started this will stop it
 * time > 0     Will start a timeout for the socket
 */
static void start_socket_timeout(sock_handle_t *handle)
{
    if (handle->timeout) {
        if (!handle->timer_started) {
            // time has not been started
            k_timer_init(&handle->timer, socket_timeout_callback, NULL);
            handle->timer_started = 1;
        }
        k_timer_start(&handle->timer, handle->timeout, handle->timeout);
        DBG_PRINT("starting socket timeout: %u\n", handle->timeout);
    } else if (handle->timer_started) {
        DBG_PRINT("stoping socket timeout\n");
        k_timer_stop(&handle->timer);
    }
}

// a zjs_post_emit callback
static void close_server(void *handle, jerry_value_t argv[], u32_t argc)
{
    sock_handle_t *h = (sock_handle_t *)handle;
    if (h->handle) {
        DBG_PRINT("closing server\n");
        net_context_put(h->handle->tcp_sock);
        zjs_free(h->handle);
    }
}

static void post_closed(void *handle)
{
    sock_handle_t *h = (sock_handle_t *)handle;
    if (h) {
        net_handle_t *net = h->handle;
        if (ZJS_LIST_REMOVE(sock_handle_t, opened_sockets, h)) {
            DBG_PRINT("Freeing socket %p: opened_sockets=%p\n", h, opened_sockets);

            net_context_put(h->tcp_sock);
            jerry_release_value(h->socket);
            zjs_free(h->rbuf);
            zjs_free(h);
        }

        if (net) {
            if (net->listening == 0 && opened_sockets == NULL) {
                // no more sockets open and not listening, close server
                zjs_defer_emit_event(net->server, "close", NULL, 0, NULL,
                                     close_server);
                DBG_PRINT("server signaled to close\n");
            }
        }
    }
}

// a zjs_post_emit callback
static void release_close(void *h, jerry_value_t argv[], u32_t argc)
{
    post_closed(h);
    if (argc) {
        zjs_release_args(h, argv, argc);
    }
}

// a zjs_pre_emit_callback
static void handle_wbuf_arg(void *h, jerry_value_t argv[], u32_t *argc,
                            const char *buffer, u32_t bytes)
{
    sock_handle_t *handle = (sock_handle_t *)h;

    // find length of unconsumed data in read buffer
    u32_t len = handle->wptr - handle->rptr;
    zjs_buffer_t *zbuf;
    jerry_value_t data_buf = zjs_buffer_create(len, &zbuf);
    if (!zbuf) {
        // out of memory
        DBG_PRINT("out of memory in handle_wbuf_arg\n");
        return;
    }

    // copy data from read buffer
    memcpy(zbuf->buffer, handle->rptr, len);
    handle->rptr = handle->wptr = handle->rbuf;

    argv[0] = data_buf;
    *argc = 1;
}

enum {
    ERROR_READ_SOCKET_CLOSED,
    ERROR_WRITE_SOCKET,
    ERROR_ACCEPT_SERVER,
    ERROR_CONNECT_SOCKET,
};

static const char *error_messages[] = {
    "socket has been closed",
    "error writing to socket",
    "error listening to accepted connection",
    "failed to make connection",
};

typedef struct error_desc {
    u32_t error_id;
    jerry_value_t this;
    jerry_value_t function_obj;
} error_desc_t;

static error_desc_t create_error_desc(u32_t error_id, jerry_value_t this,
                                      jerry_value_t function_obj)
{
    error_desc_t desc;
    desc.error_id = error_id;
    desc.this = this;
    desc.function_obj = function_obj;
    return desc;
}

// a zjs_pre_emit callback
static void handle_error_arg(void *unused, jerry_value_t argv[], u32_t *argc,
                             const char *buffer, u32_t bytes)
{
    // requires: buffer contains a u32_t with an error constant
    //  effects: creates an error object with the corresponding text, clears
    //             the error flag, and sets this as the first arg; the error
    //             value must be released later (e.g. zjs_release_args)
    if (bytes != sizeof(error_desc_t)) {
        DBG_PRINT("invalid data in handle_error_arg");
        return;
    }

    error_desc_t *desc = (error_desc_t *)buffer;
    const char *message = error_messages[desc->error_id];

    jerry_value_t error = zjs_error_context(message, desc->this,
                                            desc->function_obj);
    jerry_value_clear_error_flag(&error);
    argv[0] = error;
    *argc = 1;
}

static void tcp_received(struct net_context *context,
                         struct net_pkt *buf,
                         int status,
                         void *user_data)
{
    sock_handle_t *handle = (sock_handle_t *)user_data;

    if (status == 0 && buf == NULL) {
        // socket close
        DBG_PRINT("closing socket, context=%p, socket=%u\n", context,
                handle->socket);
        error_desc_t desc = create_error_desc(ERROR_READ_SOCKET_CLOSED, 0, 0);
        zjs_defer_emit_event(handle->socket, "error", &desc, sizeof(desc),
                             handle_error_arg, zjs_release_args);

        // note: we're not really releasing anything but release_close will
        //   just ignore the 0 args and this way we don't need a new function
        zjs_defer_emit_event(handle->socket, "close", NULL, 0, NULL,
                             release_close);
        net_pkt_unref(buf);
        return;
    }

    if (handle && buf) {
        start_socket_timeout(handle);

        u32_t len = net_pkt_appdatalen(buf);
        u8_t *data = net_pkt_appdata(buf);

        if (len && data) {
            DBG_PRINT("received data, context=%p, data=%p, len=%u\n", context,
                      data, len);

            memcpy(handle->wptr, data, len);
            handle->wptr += len;

            // if not paused, call the callback to get JS the data
            if (!handle->paused) {
                zjs_defer_emit_event(handle->socket, "data", NULL, 0,
                                     handle_wbuf_arg, zjs_release_args);

                DBG_PRINT("data received on context %p: data=%p, len=%u\n",
                          context, data, len);
            }
        }
    }
    net_pkt_unref(buf);
}

static inline void pkt_sent(struct net_context *context, int status,
                            void *token, void *user_data)
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
static ZJS_DECL_FUNC(socket_write)
{
    ZJS_VALIDATE_ARGS_OPTCOUNT(optcount, Z_OBJECT, Z_OPTIONAL Z_FUNCTION);

    GET_SOCK_HANDLE_JS(this, handle);

    start_socket_timeout(handle);

    zjs_buffer_t *buf = zjs_buffer_find(argv[0]);
    struct net_pkt *send_buf;
    send_buf = net_pkt_get_tx(handle->tcp_sock, K_NO_WAIT);
    if (!send_buf) {
        ERR_PRINT("cannot acquire send_buf\n");
        return jerry_create_boolean(false);
    }

    bool status = net_pkt_append(send_buf, buf->bufsize, buf->buffer,
                                 K_NO_WAIT);
    if (!status) {
        net_pkt_unref(send_buf);
        ERR_PRINT("cannot populate send_buf\n");
        return jerry_create_boolean(false);
    }

    zjs_callback_id id = -1;
    if (optcount) {
        id = zjs_add_callback_once(argv[1], this, NULL, NULL);
    }
    int ret = net_context_send(send_buf, pkt_sent, K_NO_WAIT,
                               UINT_TO_POINTER(net_pkt_get_len(send_buf)),
                               INT_TO_POINTER((s32_t)id));
    if (ret < 0) {
        ERR_PRINT("Cannot send data to peer (%d)\n", ret);
        net_pkt_unref(send_buf);
        zjs_remove_callback(id);
        // TODO: may need to check the specific error to determine action
        DBG_PRINT("write failed, context=%p, socket=%u\n", handle->tcp_sock,
                  handle->socket);
        error_desc_t desc = create_error_desc(ERROR_WRITE_SOCKET, this,
                                              function_obj);
        zjs_defer_emit_event(handle->socket, "error", &desc, sizeof(desc),
                             handle_error_arg, release_close);
        return jerry_create_boolean(false);
    }

    return jerry_create_boolean(true);
}

/**
 * Pause/throttle 'data' callback on the socket. Calling this will prevent
 * the 'data' callback from getting called until Socket.resume() is called.
 *
 * @name pause
 * @memberof Net.Socket
 */
static ZJS_DECL_FUNC(socket_pause)
{
    GET_SOCK_HANDLE_JS(this, handle);
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
static ZJS_DECL_FUNC(socket_resume)
{
    GET_SOCK_HANDLE_JS(this, handle);
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
static ZJS_DECL_FUNC(socket_address)
{
    GET_SOCK_HANDLE_JS(this, handle);
    jerry_value_t ret = zjs_create_object();
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
static ZJS_DECL_FUNC(socket_set_timeout)
{
    ZJS_VALIDATE_ARGS_OPTCOUNT(optcount, Z_NUMBER, Z_OPTIONAL Z_FUNCTION);

    GET_SOCK_HANDLE_JS(this, handle);

    u32_t time = (u32_t)jerry_get_number_value(argv[0]);
    handle->timeout = time;

    start_socket_timeout(handle);

    if (optcount) {
        zjs_add_event_listener(this, "timeout", argv[1]);
    }

    return jerry_acquire_value(this);
}

static ZJS_DECL_FUNC(socket_connect);

/*
 * Create a new socket object with needed methods. If 'client' is true,
 * a 'connect()' method will be added (client mode). The socket native handle
 * is returned as an out parameter.
 */
static jerry_value_t create_socket(u8_t client, sock_handle_t **handle_out)
{
    sock_handle_t *sock_handle = zjs_malloc(sizeof(sock_handle_t));
    if (!sock_handle) {
        return zjs_error_context("could not alloc socket handle", 0, 0);
    }
    memset(sock_handle, 0, sizeof(sock_handle_t));

    sock_handle->rbuf = zjs_malloc(SOCK_READ_BUF_SIZE);
    if (!sock_handle->rbuf) {
        zjs_free(sock_handle);
        return zjs_error_context("out of memory", 0, 0);
    }

    jerry_value_t socket = zjs_create_object();

    if (client) {
        // only a new client socket has connect method
        zjs_obj_add_function(socket, socket_connect, "connect");
    }

    sock_handle->connect_listener = ZJS_UNDEFINED;
    sock_handle->socket = socket;
    sock_handle->rptr = sock_handle->wptr = sock_handle->rbuf;

    zjs_make_emitter(socket, zjs_net_socket_prototype, sock_handle, NULL);

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
    GET_SOCK_HANDLE(socket, handle);
    if (!handle) {
        ERR_PRINT("could not get socket handle\n");
        return;
    }

    handle->remote = *remote;
    handle->handle = net;
    handle->tcp_sock = new;

    sa_family_t family = net_context_get_family(new);
    char remote_ip[64];
    net_addr_ntop(family, (const void *)remote, remote_ip, 64);

    zjs_obj_add_string(socket, remote_ip, "remoteAddress");
    zjs_obj_add_number(socket, net->port, "remotePort");

    char local_ip[64];
    net_addr_ntop(family, (const void *)&net->local, local_ip, 64);

    zjs_obj_add_string(socket, local_ip, "localAddress");
    zjs_obj_add_number(socket, net->port, "localPort");
    if (family == AF_INET6) {
        zjs_obj_add_string(socket, "IPv6", "family");
        zjs_obj_add_string(socket, "IPv6", "remoteFamily");
    } else {
        zjs_obj_add_string(socket, "IPv4", "family");
        zjs_obj_add_string(socket, "IPv4", "remoteFamily");
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

    // FIXME: this shouldn't really be getting called here because it runs in
    //   a networking thread but is doing malloc and JerryScript calls
    jerry_value_t sock = create_socket(false, &sock_handle);
    if (!sock_handle) {
        ERR_PRINT("could not allocate socket handle\n");
        return;
    }

    add_socket_connection(sock, handle, context, addr);

    // add new socket to list
    sock_handle->next = opened_sockets;
    opened_sockets = sock_handle;

    int ret = net_context_recv(context, tcp_received, 0, sock_handle);

    if (ret < 0) {
        ERR_PRINT("Cannot receive TCP packet (family %d), ret=%d\n",
                net_context_get_family(sock_handle->tcp_sock), ret);
        // this seems to mean the remote exists but the connection was not made
        error_desc_t desc = create_error_desc(ERROR_ACCEPT_SERVER, 0, 0);
        zjs_defer_emit_event(sock_handle->handle->server, "error", &desc,
                             sizeof(desc), handle_error_arg, zjs_release_args);
        jerry_release_value(sock);
        return;
    }

    zjs_defer_emit_event(handle->server, "connection", &sock, sizeof(sock),
                         zjs_copy_arg, zjs_release_args);
}

/**
 * Retrieve address information from the bound server socket
 *
 * @name address
 * @memberof Net.Server
 * @return {AddressObject}
 */
static ZJS_DECL_FUNC(server_address)
{
    GET_NET_HANDLE_JS(this, handle);

    jerry_value_t info = zjs_create_object();
    zjs_obj_add_number(info, handle->port, "port");

    sa_family_t family = net_context_get_family(handle->tcp_sock);
    char ipstr[INET6_ADDRSTRLEN];

    if (family == AF_INET6) {
        zjs_obj_add_string(info, "IPv6", "family");
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&handle->local;
        net_addr_ntop(family, &addr6->sin6_addr, ipstr, INET6_ADDRSTRLEN);
        zjs_obj_add_string(info, ipstr, "address");

    } else {
        zjs_obj_add_string(info, "IPv4", "family");
        struct sockaddr_in *addr = (struct sockaddr_in *)&handle->local;
        net_addr_ntop(family, &addr->sin_addr, ipstr, INET6_ADDRSTRLEN);
        zjs_obj_add_string(info, ipstr, "address");
    }


    return info;
}

/**
 * Signal the server to close. Any opened sockets will remain open, and the
 * 'close' event will be called when these remaining sockets are closed.
 *
 * @name close
 * @memberof Net.Server
 * @param {function?} Callback function. Called when server is closed
 */
static ZJS_DECL_FUNC(server_close)
{
    ZJS_VALIDATE_ARGS_OPTCOUNT(optcount, Z_OPTIONAL Z_FUNCTION);

    GET_NET_HANDLE_JS(this, handle);

    handle->listening = 0;
    zjs_obj_add_boolean(this, false, "listening");

    if (optcount) {
        zjs_add_event_listener(handle->server, "close", argv[0]);
    }
    // If there are no connections the server can be closed
    if (opened_sockets == NULL) {
        // NOTE: could emit immediately but safer for call stack to defer
        zjs_defer_emit_event(handle->server, "close", NULL, 0, NULL,
                             close_server);
        DBG_PRINT("server signaled to close\n");
    }
    return ZJS_UNDEFINED;
}

/**
 * Get the number of connections on this server
 *
 * @name getConnections
 * @memberof Net.Server
 * @param {function} Callback function. Called with the number of opened
 *                    connections
 */
static ZJS_DECL_FUNC(server_get_connections)
{
    ZJS_VALIDATE_ARGS(Z_FUNCTION);

    GET_NET_HANDLE_JS(this, handle);

    int count = 0;
    sock_handle_t *cur = opened_sockets;
    while (cur) {
        if (cur->handle == handle) {
            count++;
        }
        cur = cur->next;
    }

    ZVAL err = jerry_create_number(0);
    ZVAL num = jerry_create_number(count);
    jerry_value_t args[2] = { err, num };

    zjs_callback_id id = zjs_add_callback_once(argv[0], this, NULL, NULL);
    zjs_signal_callback(id, args, sizeof(args));

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
static ZJS_DECL_FUNC(server_listen)
{
    // options object, optional function
    ZJS_VALIDATE_ARGS_OPTCOUNT(optcount, Z_OBJECT, Z_OPTIONAL Z_FUNCTION);

    GET_NET_HANDLE_JS(this, handle);

    int ret;
    double port = 0;
    double backlog = 0;
    u32_t size = NET_HOSTNAME_MAX;
    char hostname[size];
    double family = 0;

    zjs_obj_get_double(argv[0], "port", &port);
    zjs_obj_get_double(argv[0], "backlog", &backlog);
    zjs_obj_get_string(argv[0], "host", hostname, size);
    zjs_obj_get_double(argv[0], "family", &family);

    if (optcount) {
        zjs_add_event_listener(this, "listening", argv[1]);
    }

    struct sockaddr addr;
    memset(&addr, 0, sizeof(struct sockaddr));

    // default to IPv4
    if (family == 0 || family == 4) {
        family = 4;
        CHECK(net_context_get(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                              &handle->tcp_sock))

        struct sockaddr_in *addr4 = (struct sockaddr_in *)&addr;

        addr4->sin_family = AF_INET;
        addr4->sin_port = htons((int)port);

        net_addr_pton(AF_INET, hostname, &addr4->sin_addr);
    } else {
        CHECK(net_context_get(AF_INET6, SOCK_STREAM, IPPROTO_TCP,
                              &handle->tcp_sock))

        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&addr;

        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons((int)port);

        net_addr_pton(AF_INET6, hostname, &addr6->sin6_addr);
    }

    CHECK(net_context_bind(handle->tcp_sock, &addr, sizeof(struct sockaddr)));
    CHECK(net_context_listen(handle->tcp_sock, (int)backlog));

    handle->listening = 1;
    handle->port = (u16_t)port;

    memcpy(&handle->local, zjs_net_config_get_ip(handle->tcp_sock),
           sizeof(struct sockaddr));
    handle->local = *zjs_net_config_get_ip(handle->tcp_sock);
    zjs_obj_add_boolean(this, true, "listening");

    // Here we defer just to keep the call stack short; this is unless we
    //   determine that the listeners should be called before this function
    //   returns. Note, since this is a JS API function we can be sure we're in
    //   the main thread and can call JerryScript functions as above. Normally
    //   when we're calling defer_emit we put off those kinds of calls until
    //   we're sure to be back in the main thread during the pre_emit callback
    zjs_defer_emit_event(this, "listening", NULL, 0, NULL, NULL);

    CHECK(net_context_accept(handle->tcp_sock, tcp_accepted, 0, handle));

    DBG_PRINT("listening for connection to %s:%u\n", hostname, (u32_t)port);

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
static ZJS_DECL_FUNC(net_create_server)
{
    ZJS_VALIDATE_ARGS_OPTCOUNT(optcount, Z_OPTIONAL Z_FUNCTION);

    jerry_value_t server = zjs_create_object();

    zjs_obj_add_boolean(server, false, "listening");
    zjs_obj_add_number(server, NET_DEFAULT_MAX_CONNECTIONS, "maxConnections");

    net_handle_t *handle = zjs_malloc(sizeof(net_handle_t));
    if (!handle) {
        jerry_release_value(server);
        return zjs_error("could not alloc server handle");
    }

    handle->server = server;
    handle->listening = 0;

    zjs_make_emitter(server, zjs_net_server_prototype, handle, NULL);

    if (optcount) {
        zjs_add_event_listener(server, "connection", argv[0]);
    }

    DBG_PRINT("creating server: context=%p\n", handle->tcp_sock);

    return server;
}

// a zjs_pre_emit_callback
static void connect_callback(void *h, jerry_value_t argv[], u32_t *argc,
                             const char *buffer, u32_t bytes)
{
    sock_handle_t *handle = (sock_handle_t *)h;
    zjs_obj_add_boolean(handle->socket, false, "connecting");
    zjs_add_event_listener(handle->socket, "connect",
                           handle->connect_listener);
}

static void tcp_connected(struct net_context *context, int status,
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
            start_socket_timeout(sock_handle);

            // here we supply a pre callback to manipulate JerryScript objects
            //   from the main thread; but don't actually have args to pass
            zjs_defer_emit_event(sock_handle->socket, "connect", NULL, 0,
                                 connect_callback, NULL);

            DBG_PRINT("connection success, context=%p, socket=%u\n", context,
                      sock_handle->socket);
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
static ZJS_DECL_FUNC(socket_connect)
{
    ZJS_VALIDATE_ARGS(Z_OBJECT, Z_OPTIONAL Z_FUNCTION);

    GET_SOCK_HANDLE_JS(this, handle);

    int ret;
    if (!handle->tcp_sock) {
        CHECK(net_context_get(AF_INET6, SOCK_STREAM, IPPROTO_TCP,
                              &handle->tcp_sock));
    }
    if (!handle->tcp_sock) {
        DBG_PRINT("connect failed\n");
        error_desc_t desc = create_error_desc(ERROR_CONNECT_SOCKET, this,
                                              function_obj);
        zjs_defer_emit_event(this, "error", &desc, sizeof(desc),
                             handle_error_arg, zjs_release_args);
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

    DBG_PRINT("port=%u, host=%s, localPort=%u, localAddress=%s, socket=%u\n",
              (u32_t)port, host, (u32_t)localPort, localAddress, this);

    if (fam == 6) {
        if (!handle->bound) {
            struct sockaddr_in6 my_addr6 = { 0 };

            my_addr6.sin6_family = AF_INET6;
            my_addr6.sin6_port = htons((u32_t)localPort);

            CHECK(net_addr_pton(AF_INET6, localAddress, &my_addr6.sin6_addr));
            // bind to our local address
            CHECK(net_context_bind(handle->tcp_sock,
                                   (struct sockaddr *)&my_addr6,
                                   sizeof(struct sockaddr_in6)));
            handle->bound = 1;
        }
        struct sockaddr_in6 peer_addr6 = { 0 };
        peer_addr6.sin6_family = AF_INET6;
        peer_addr6.sin6_port = htons((u32_t)port);

        CHECK(net_addr_pton(AF_INET6, host, &peer_addr6.sin6_addr));
        // set socket.connecting property == true
        zjs_obj_add_boolean(this, true, "connecting");
        // connect to remote
        if (net_context_connect(
                handle->tcp_sock, (struct sockaddr *)&peer_addr6,
                sizeof(peer_addr6), tcp_connected, 1, handle) < 0) {
            DBG_PRINT("connect failed\n");
            zjs_obj_add_boolean(this, false, "connecting");
            error_desc_t desc = create_error_desc(ERROR_CONNECT_SOCKET, this,
                                                  function_obj);
            zjs_defer_emit_event(this, "error", &desc, sizeof(desc),
                                 handle_error_arg, zjs_release_args);
            return ZJS_UNDEFINED;
        }
    } else {
        if (!handle->bound) {
            struct sockaddr_in my_addr4 = { 0 };

            my_addr4.sin_family = AF_INET;
            my_addr4.sin_port = htons((u32_t)localPort);
            CHECK(net_addr_pton(AF_INET, localAddress, &my_addr4.sin_addr));
            // bind to our local address
            CHECK(net_context_bind(handle->tcp_sock,
                                   (struct sockaddr *)&my_addr4,
                                   sizeof(struct sockaddr_in)));
            handle->bound = 1;
        }
        struct sockaddr_in peer_addr4 = { 0 };
        peer_addr4.sin_family = AF_INET;
        peer_addr4.sin_port = htons((u32_t)port);

        CHECK(net_addr_pton(AF_INET, host, &peer_addr4.sin_addr));
        // set socket.connecting property == true
        zjs_obj_add_boolean(this, true, "connecting");
        // connect to remote
        if (net_context_connect(handle->tcp_sock,
                                (struct sockaddr *)&peer_addr4,
                                sizeof(peer_addr4), tcp_connected,
                                1, handle) < 0) {
            DBG_PRINT("connect failed\n");
            error_desc_t desc = create_error_desc(ERROR_CONNECT_SOCKET, this,
                                                  function_obj);
            zjs_defer_emit_event(this, "error", &desc, sizeof(desc),
                                 handle_error_arg, zjs_release_args);
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
static ZJS_DECL_FUNC(net_socket)
{
    sock_handle_t *sock_handle = NULL;
    jerry_value_t socket = create_socket(true, &sock_handle);
    if (!sock_handle) {
        return zjs_error("could not alloc socket handle");
    }

    DBG_PRINT("socket created, context=%p, sock=%u\n", sock_handle->tcp_sock,
              socket);

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
static ZJS_DECL_FUNC(net_is_ip)
{
    if (!jerry_value_is_string(argv[0]) || argc < 1) {
        return jerry_create_number(0);
    }
    jerry_size_t size = 64;
    char ip[size];
    zjs_copy_jstring(argv[0], ip, &size);
    if (!size) {
        return jerry_create_number(0);
    }
    struct sockaddr_in6 tmp = { 0 };

    // check if v6
    if (net_addr_pton(AF_INET6, ip, &tmp.sin6_addr) < 0) {
        // check if v4
        struct sockaddr_in tmp1 = { 0 };
        if (net_addr_pton(AF_INET, ip, &tmp1.sin_addr) < 0) {
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
static ZJS_DECL_FUNC(net_is_ip4)
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
static ZJS_DECL_FUNC(net_is_ip6)
{
    ZVAL ret = net_is_ip(function_obj, this, argv, argc);
    double v = jerry_get_number_value(ret);
    if (v == 6) {
        return jerry_create_boolean(true);
    }
    return jerry_create_boolean(false);
}

static jerry_value_t net_obj;

jerry_value_t zjs_net_init()
{
    zjs_net_config_default();

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
            { server_address, "address" },
            { server_listen, "listen" },
            { server_close, "close" },
            { server_get_connections, "getConnections" },
            { NULL, NULL }
    };
    // Net object prototype
    zjs_net_prototype = zjs_create_object();
    zjs_obj_add_functions(zjs_net_prototype, net_array);

    // Socket object prototype
    zjs_net_socket_prototype = zjs_create_object();
    zjs_obj_add_functions(zjs_net_socket_prototype, sock_array);

    // Server object prototype
    zjs_net_server_prototype = zjs_create_object();
    zjs_obj_add_functions(zjs_net_server_prototype, server_array);

    net_obj = zjs_create_object();
    jerry_set_prototype(net_obj, zjs_net_prototype);

    return jerry_acquire_value(net_obj);
}

void zjs_net_cleanup()
{
    jerry_release_value(zjs_net_prototype);
    jerry_release_value(zjs_net_socket_prototype);
    jerry_release_value(zjs_net_server_prototype);
}

#endif  // BUILD_MODULE_NET
