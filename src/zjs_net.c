// Copyright (c) 2017-2018, Intel Corporation.

#ifdef BUILD_MODULE_NET

// enable to use function tracing for debug purposes
#if 0
#define USE_FTRACE
static char FTRACE_PREFIX[] = "net";
#endif

// enable for verbose lock detail
#if 0
#define DEBUG_LOCKS
#endif

// C includes
#include <errno.h>

// Zephyr includes
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

// represents a server socket (e.g. listening on a port)
typedef struct server_handle {
    struct net_context *server_ctx;
    jerry_value_t server;
    struct sock_handle *connections;
    struct server_handle *next;
    struct sockaddr local;
    struct net_context *early_closed;
    u16_t port;
    u8_t listening;
    u8_t closed;
} server_handle_t;

// represents a server connection socket or client socket
typedef struct sock_handle {
    // set to &no_server for client connection sockets
    server_handle_t *server_h;

    // only used for client sockets
    jerry_value_t connect_listener;

    struct sock_handle *next;
    struct net_context *tcp_sock;
    struct sockaddr remote;
    jerry_value_t socket;
    void *rptr;
    void *wptr;
    struct k_timer timer;
    u32_t timeout;
    u8_t bound;
    u8_t paused;
    u8_t *rbuf;
    u8_t timer_started;
    u8_t closing;
    u8_t closed;
} sock_handle_t;

// a stub server handle representing client connections with no server
static server_handle_t no_server;

// the server list will always contain "no_server"
static server_handle_t *servers = &no_server;

// mutex to ensure only one thread uses handle lists at time
static struct k_mutex socket_mutex;

#define S_LOCK()                            \
    LPRINT("Sockets lock...");              \
    k_mutex_lock(&socket_mutex, K_FOREVER); \
    LPRINT("Sockets locked.")
#define S_UNLOCK()                 \
    LPRINT("Sockets unlock...");   \
    k_mutex_unlock(&socket_mutex); \
    LPRINT("Sockets unlocked.")

// get the socket handle from the object or NULL
#define GET_SOCK_HANDLE(obj, var) \
    sock_handle_t *var = (sock_handle_t *)zjs_event_get_user_handle(obj);

// get the socket handle or return a JS error
#define GET_SOCK_HANDLE_JS(obj, var)                                      \
    sock_handle_t *var = (sock_handle_t *)zjs_event_get_user_handle(obj); \
    if (!var) {                                                           \
        return zjs_error("no socket handle");                             \
    }

// get the net handle or return a JS error
#define GET_SERVER_HANDLE_JS(obj, var)                                        \
    server_handle_t *var = (server_handle_t *)zjs_event_get_user_handle(obj); \
    if (!var) {                                                               \
        return zjs_error("no socket handle");                                 \
    }

#define CHECK(x)                                 \
    ret = (x);                                   \
    if (ret < 0) {                               \
        ERR_PRINT("Error in " #x ": %d\n", ret); \
        return zjs_error(#x);                    \
    }

#define NET_DEFAULT_MAX_CONNECTIONS 5
#define NET_HOSTNAME_MAX            32
#define SOCK_READ_BUF_SIZE          128

// TODO: this could perhaps be reused in dgram/ws etc
static void zjs_copy_sockaddr(struct sockaddr *dst, struct sockaddr *src,
                              socklen_t len)
{
    // requires: dst contains at least sizeof(sockaddr) bytes, src points to
    //             a valid sockaddr struct (sockaddr_in or sockaddr_in6),
    //             len is the address length if known, or 0
    //  effects: copies src to dest, but only the number of bytes required by
    //             the underlying address family; if len is given, asserts
    //             that it matches the expected size
    if (src->sa_family == AF_INET) {
        ZJS_ASSERT(!len || len == sizeof(struct sockaddr_in),
                   "expected IPv4 length");
        *(struct sockaddr_in *)dst = *(struct sockaddr_in *)src;
    } else if (src->sa_family == AF_INET6) {
        ZJS_ASSERT(!len || len == sizeof(struct sockaddr_in6),
                   "expected IPv6 length");
        *(struct sockaddr_in6 *)dst = *(struct sockaddr_in6 *)src;
    } else {
        ZJS_ASSERT(false, "invalid sockaddr struct");
    }
}

static void *zjs_get_inaddr(struct sockaddr *addr)
{
    // requires: addr is a pointer to a valid sockaddr_in or sockaddr_in6 struct
    //  effects: returns a void pointer to the in_addr or in6_addr within
    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
        return &addr4->sin_addr;
    }
    else if (addr->sa_family == AF_INET6) {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
        return &addr6->sin6_addr;
    }
    else {
        ZJS_ASSERT(false, "invalid sockaddr struct");
    }
}

static int match_timer(sock_handle_t *sock, struct k_timer *timer)
{
    FTRACE("sock = %p, timer = %p\n", sock, timer);
    return (&sock->timer == timer) ? 0 : 1;
}

static void socket_timeout_callback(struct k_timer *timer)
{
    FTRACE("timer = %p\n", timer);
    sock_handle_t *sock = NULL;
    server_handle_t *server = servers;

    S_LOCK();
    while (server && !sock) {
        // search in server and "no_server" connections
        sock = ZJS_LIST_FIND_CMP(sock_handle_t, server->connections,
                                 match_timer, timer);
        server = server->next;
    }
    S_UNLOCK();

    if (sock) {
        zjs_defer_emit_event(sock->socket, "timeout", NULL, 0, NULL, NULL);
        k_timer_stop(timer);
        // TODO: This may not be correct, but if we don't set it, then more
        //       timeouts will get added, potentially after the socket has been
        //       closed
        sock->timeout = 0;
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
    FTRACE("handle = %p\n", handle);
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
static void close_server(server_handle_t *server_h)
{
    FTRACE("server_h = %p\n", server_h);
    DBG_PRINT("closing server: %p\n", server_h);
    zjs_emit_event(server_h->server, "close", NULL, 0);
    zjs_destroy_emitter(server_h->server);
    jerry_release_value(server_h->server);

    S_LOCK();
    ZJS_LIST_REMOVE(server_handle_t, servers, server_h);
    S_UNLOCK();
}

// a zjs_event_free callback
static void server_free_cb(void *native)
{
    FTRACE("native = %p\n", native);
    server_handle_t *server_h = (server_handle_t *)native;
    ZJS_ASSERT(server_h != &no_server, "attempt to free stub server");
    net_context_put(server_h->server_ctx);
    zjs_free(server_h);
}

// a zjs_post_emit callback
static void release_close(void *handle, jerry_value_t argv[], u32_t argc)
{
    FTRACE("handle = %p, argc = %d\n", handle, argc);
    sock_handle_t *h = (sock_handle_t *)handle;
    if (!h) {
        DBG_PRINT("warning: null pointer for socket handle\n");
        return;
    }

    server_handle_t *server_h = h->server_h;
    S_LOCK();
    u8_t removed = ZJS_LIST_REMOVE(sock_handle_t, server_h->connections, h);
    S_UNLOCK();
    ZJS_ASSERT(removed, "connection not found in list");

    // check if this is a server connection socket
    if (server_h != &no_server) {
        if (removed) {
            DBG_PRINT("Freeing socket %p: connections=%p\n", h,
                      server_h->connections);
            if (!h->closed) {
                // if the context hasn't been dropped yet, do so now
                net_context_unref(h->tcp_sock);
                h->closed = 1;
            }
            zjs_destroy_emitter(h->socket);
            jerry_release_value(h->socket);
            // FIXME: this part should maybe move into an emitter free cb
            zjs_free(h->rbuf);
            zjs_free(h);
        }

        if (server_h->closed && !server_h->connections) {
            // no more sockets open and not listening, close server
            close_server(server_h);
        }
    } else {
        // for client sockets, we did get and need to do put
        net_context_put(h->tcp_sock);
    }

    if (argc) {
        zjs_release_args(h, argv, argc);
    }
}

static inline sock_handle_t *find_connection(server_handle_t *server_h,
                                             struct net_context *context)
{
    // effects: looks through server_h's connections list to find one with a
    //            matching context
    FTRACE("server_h = %p, context = %p\n", server_h, context);
    S_LOCK();
    sock_handle_t *sock = ZJS_LIST_FIND(sock_handle_t, server_h->connections,
                                        tcp_sock, context);
    S_UNLOCK();
    return sock;
}

enum {
    ERROR_WRITE_SOCKET,
    ERROR_ACCEPT_SERVER,
    ERROR_CONNECT_SOCKET,
};

static const char *error_messages[] = {
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
    FTRACE("error_id = %d, this = %p, func = %p\n", error_id, (void *)this,
           (void *)function_obj);
    error_desc_t desc;
    desc.error_id = error_id;
    desc.this = this;
    desc.function_obj = function_obj;
    return desc;
}

// a zjs_pre_emit callback
static bool handle_error_arg(void *unused, jerry_value_t argv[], u32_t *argc,
                             const char *buffer, u32_t bytes)
{
    // requires: buffer contains a u32_t with an error constant
    //  effects: creates an error object with the corresponding text, clears
    //             the error flag, and sets this as the first arg; the error
    //             value must be released later (e.g. zjs_release_args)
    FTRACE("buffer = %p, bytes = %d\n", buffer, bytes);
    ZJS_ASSERT(bytes == sizeof(error_desc_t), "invalid data received");

    error_desc_t *desc = (error_desc_t *)buffer;
    const char *message = error_messages[desc->error_id];

    jerry_value_t error = zjs_error_context(message, desc->this,
                                            desc->function_obj);
    jerry_value_clear_error_flag(&error);
    argv[0] = error;
    *argc = 1;
    return true;
}

typedef struct {
    struct net_context *context;
    server_handle_t *server_h;
    sock_handle_t *handle;
    struct net_pkt *pkt;
} receive_packet_t;

// a zjs_deferred_work callback
static void receive_packet(const void *buffer, u32_t length)
{
    // effects: handle an incoming packet on the main thread that we first
    //            received on the RX thread
    FTRACE("buffer = %p, length = %d\n", buffer, length);
    ZJS_ASSERT(length == sizeof(receive_packet_t), "invalid data received");

    receive_packet_t *receive = (receive_packet_t *)buffer;
    sock_handle_t *handle = receive->handle;
    struct net_pkt *pkt = receive->pkt;
    net_pkt_unref(pkt);

    if (!handle) {
        handle = find_connection(receive->server_h, receive->context);
    }
    ZJS_ASSERT(handle, "no handle found");
    ZJS_ASSERT(handle, "no packet found");

    if (handle && pkt) {
        start_socket_timeout(handle);

        u32_t len = net_pkt_appdatalen(pkt);
        u8_t *data = net_pkt_appdata(pkt);

        if (len && data) {
            DBG_PRINT("received data, context=%p, data=%p, len=%u\n",
                      receive->context, data, len);

            memcpy(handle->wptr, data, len);
            handle->wptr += len;

            // if not paused, call the callback to get JS the data
            if (!handle->paused) {
                // find length of unconsumed data in read buffer
                u32_t len = handle->wptr - handle->rptr;
                zjs_buffer_t *zbuf;
                ZVAL data_buf = zjs_buffer_create(len, &zbuf);
                if (!zbuf) {
                    // out of memory
                    DBG_PRINT("out of memory\n");
                    net_pkt_unref(pkt);
                    return;
                }

                // copy data from read buffer
                memcpy(zbuf->buffer, handle->rptr, len);
                handle->rptr = handle->wptr = handle->rbuf;

                zjs_emit_event(handle->socket, "data", &data_buf, 1);
            }
        }
    }
    net_pkt_unref(pkt);
}

typedef struct {
    server_handle_t *server_h;
    struct net_context *context;
} clear_closed_t;

// a zjs_deferred_work callback
static void clear_closed(const void *buffer, u32_t length)
{
    // requires: only called for server connection sockets
    //  effects: this callback should be stuffed in the queue as soon as the
    //             socket is closed, so the idea is incoming packets before this
    //             will still be passed on to the socket, but at this point we
    //             clean up and will no longer refer to the associated context
    //           this is particularly because the same context ID might get used
    //             again right away and we don't want to be confused thinking
    FTRACE("buffer = %p, length = %d\n", buffer, length);
    ZJS_ASSERT(length == sizeof(clear_closed_t), "invalid data received");

    clear_closed_t *clear = (clear_closed_t *)buffer;
    ZJS_ASSERT(clear->server_h != &no_server, "called with client socket");
    ZJS_ASSERT(clear->server_h->early_closed == clear->context,
               "unexpected early closed socket");

    DBG_PRINT("cleared early closed for server %p\n", clear->server_h);
    clear->server_h->early_closed = NULL;

    sock_handle_t *handle = find_connection(clear->server_h, clear->context);
    ZJS_ASSERT(handle, "handle not found");
    if (handle) {
        // clear context reference out of the handle so it no longer shows
        //   up as a match with find_connection
        net_context_unref(handle->tcp_sock);
        handle->closed = 1;
        zjs_emit_event(handle->socket, "close", NULL, 0);
        release_close(handle, NULL, 0);
    }
}

static void tcp_received(struct net_context *context,
                         struct net_pkt *pkt,
                         int status,
                         void *user_data)
{
    // requires: user_data is the server handle the packet is associated with
    //             (or &no_server in the case of client connection sockets)
    FTRACE("context = %p, pkt = %p, status = %d, user_data = %p\n", context,
           pkt, status, user_data);
#ifdef DEBUG_BUILD
    static int first = 1;
    if (first) {
        DBG_PRINT("RX Thread ID: %p\n", (void *)k_current_get());
        first = 0;
    }
#endif
    server_handle_t *server_h = (server_handle_t *)user_data;
    sock_handle_t *handle = find_connection(server_h, context);
    if (context == server_h->early_closed) {
        // we got a close event on this same context earlier, and haven't
        //   finished dealing with it, so this must be a new socket w/ the
        //   same context
        handle = NULL;
    }

    if (status == 0 && pkt == NULL) {
        // this means the socket closed properly
        DBG_PRINT("closing socket, context=%p, handle=%p\n", context, handle);
        net_pkt_unref(pkt);

        if (handle) {
            DBG_PRINT("socket=%p\n", (void *)handle->socket);
            // NOTE: we're not really releasing anything but release_close will
            //   just ignore the 0 args, so we can reuse the function
            handle->closing = 1;
            zjs_defer_emit_event(handle->socket, "close", NULL, 0, NULL,
                                 release_close);
        } else {
            ZJS_ASSERT(server_h != &no_server,
                       "client connections shouldn't get here");
            if (server_h->early_closed) {
                // this is bad because we only allocated space in the server
                //   handle to remember one early-closed socket; could be
                //   increased to an array of them if need be
                ERR_PRINT("Socket closed early with another in process\n");
            } else {
                DBG_PRINT("socket closed before data received\n");
                DBG_PRINT("marking server %p with early closed %p\n",
                          server_h, context);
                server_h->early_closed = context;

                clear_closed_t clear;
                clear.server_h = server_h;
                clear.context = context;
                zjs_defer_work(clear_closed, &clear, sizeof(clear));
            }
        }
        return;
    }

    receive_packet_t receive;
    receive.context = context;
    receive.server_h = server_h;
    receive.handle = handle;
    receive.pkt = pkt;

    zjs_defer_work(receive_packet, &receive, sizeof(receive));
}

static inline void pkt_sent(struct net_context *context, int status,
                            void *token, void *user_data)
{
    FTRACE("context = %p, status = %d, token = %p, user_data = %p\n", context,
           status, token, user_data);
#ifdef DEBUG_BUILD
    static int first = 1;
    if (first) {
        DBG_PRINT("TX Thread ID: %p\n", (void *)k_current_get());
        first = 0;
    }
#endif
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

    // FIXME: these other error cases should maybe call "error" event too
    if (handle->closing || handle->closed) {
        ERR_PRINT("socket already closed\n");
        return jerry_create_boolean(false);
    }

    start_socket_timeout(handle);

    zjs_buffer_t *buf = zjs_buffer_find(argv[0]);
    struct net_pkt *send_pkt;
    send_pkt = net_pkt_get_tx(handle->tcp_sock, K_NO_WAIT);
    if (!send_pkt) {
        ERR_PRINT("cannot acquire send_pkt\n");
        return jerry_create_boolean(false);
    }

    bool status = net_pkt_append(send_pkt, buf->bufsize, buf->buffer,
                                 K_NO_WAIT);
    if (!status) {
        net_pkt_unref(send_pkt);
        ERR_PRINT("cannot populate send_pkt\n");
        return jerry_create_boolean(false);
    }

    zjs_callback_id id = -1;
    if (optcount) {
        id = zjs_add_callback_once(argv[1], this, NULL, NULL);
    }

    int ret = net_context_send(send_pkt, pkt_sent, K_NO_WAIT,
                               UINT_TO_POINTER(net_pkt_get_len(send_pkt)),
                               INT_TO_POINTER((s32_t)id));
    if (ret < 0) {
        ERR_PRINT("Cannot send data to peer (%d)\n", ret);
        net_pkt_unref(send_pkt);
        zjs_remove_callback(id);
        // TODO: may need to check the specific error to determine action
        DBG_PRINT("write failed, context=%p, socket=%p\n", handle->tcp_sock,
                  (void *)handle->socket);
        error_desc_t desc = create_error_desc(ERROR_WRITE_SOCKET, this,
                                              function_obj);
        zjs_defer_emit_event(handle->socket, "error", &desc, sizeof(desc),
                             handle_error_arg, zjs_release_args);
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
    FTRACE_JSAPI;
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
    FTRACE_JSAPI;
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
    FTRACE_JSAPI;
    GET_SOCK_HANDLE_JS(this, handle);
    jerry_value_t ret = zjs_create_object();
    ZVAL port = zjs_get_property(this, "localPort");
    ZVAL addr = zjs_get_property(this, "localAddress");
    sa_family_t family = net_context_get_family(handle->tcp_sock);

    zjs_set_property(ret, "port", port);
    zjs_set_property(ret, "address", addr);
    if (family == AF_INET6) {
        zjs_obj_add_string(ret, "family", "IPv6");
    } else {
        zjs_obj_add_string(ret, "family", "IPv4");
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
    // returns: a socket object that the caller owns
    FTRACE("client = %d\n", (u32_t)client);
    sock_handle_t *sock_handle = zjs_malloc(sizeof(sock_handle_t));
    if (!sock_handle) {
        return ZJS_UNDEFINED;
    }
    memset(sock_handle, 0, sizeof(sock_handle_t));

    sock_handle->rbuf = zjs_malloc(SOCK_READ_BUF_SIZE);
    if (!sock_handle->rbuf) {
        zjs_free(sock_handle);
        return ZJS_UNDEFINED;
    }

    jerry_value_t socket = zjs_create_object();

    if (client) {
        sock_handle->server_h = &no_server;

        // only a new client socket has connect method
        zjs_obj_add_function(socket, "connect", socket_connect);
    }

    sock_handle->connect_listener = ZJS_UNDEFINED;
    sock_handle->socket = jerry_acquire_value(socket);
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
                                  server_handle_t *server_h,
                                  struct net_context *new,
                                  struct sockaddr *remote)
{
    FTRACE("socket = %p, server_h = %p, new = %p, remote = %p\n",
           (void *)socket, server_h, new, remote);
    GET_SOCK_HANDLE(socket, handle);
    if (!handle) {
        ERR_PRINT("could not get socket handle\n");
        return;
    }

    handle->remote = *remote;
    handle->server_h = server_h;
    handle->tcp_sock = new;

    if (server_h->early_closed == new) {
        net_context_unref(handle->tcp_sock);
        handle->closed = 1;
    }

    sa_family_t family = net_context_get_family(new);
    char remote_ip[INET6_ADDRSTRLEN];
    net_addr_ntop(family, zjs_get_inaddr(remote), remote_ip, INET6_ADDRSTRLEN);

    zjs_obj_add_string(socket, "remoteAddress", remote_ip);
    zjs_obj_add_number(socket, "remotePort", server_h->port);

    char local_ip[INET6_ADDRSTRLEN];
    net_addr_ntop(family, zjs_get_inaddr(&server_h->local), local_ip,
                  INET6_ADDRSTRLEN);

    zjs_obj_add_string(socket, "localAddress", local_ip);
    zjs_obj_add_number(socket, "localPort", server_h->port);
    if (family == AF_INET6) {
        zjs_obj_add_string(socket, "family", "IPv6");
        zjs_obj_add_string(socket, "remoteFamily", "IPv6");
    } else {
        zjs_obj_add_string(socket, "family", "IPv4");
        zjs_obj_add_string(socket, "remoteFamily", "IPv4");
    }
}

typedef struct {
    struct net_context *context;
    server_handle_t *server_h;
    struct sockaddr addr;
} accept_connection_t;

// a zjs_deferred_work callback
static void accept_connection(const void *buffer, u32_t length)
{
    FTRACE("buffer = %p, length = %d\n", buffer, length);
    ZJS_ASSERT(length == sizeof(accept_connection_t), "invalid data received");

    accept_connection_t *accept = (accept_connection_t *)buffer;

    sock_handle_t *sock_handle = NULL;
    ZVAL sock = create_socket(false, &sock_handle);
    if (!sock_handle) {
        ERR_PRINT("could not allocate socket handle\n");
        net_context_unref(accept->context);
        return;
    }

    add_socket_connection(sock, accept->server_h, accept->context,
                          &accept->addr);

    // add new socket to list
    S_LOCK();
    ZJS_LIST_PREPEND(sock_handle_t, accept->server_h->connections, sock_handle);
    S_UNLOCK();

    zjs_emit_event(accept->server_h->server, "connection", &sock, 1);
}

static void tcp_accepted(struct net_context *context,
                         struct sockaddr *addr,
                         socklen_t addrlen,
                         int error,
                         void *user_data)
{
    FTRACE("context = %p, addr = %p, addrlen = %d, error = %d, userdata = %p\n",
           context, addr, addrlen, error, user_data);
#ifdef DEBUG_BUILD
    static int first = 1;
    if (first) {
        DBG_PRINT("RX Thread ID: %p\n", (void *)k_current_get());
        first = 0;
    }
#endif
    // FIXME: handle error < 0
    DBG_PRINT("connection made, context %p error %d\n", context, error);

    server_handle_t *server_h = (server_handle_t *)user_data;
    if (!server_h->listening || server_h->closed) {
        DBG_PRINT("Warning: received connection on closed socket\n");
        return;
    }

    int rval = net_context_recv(context, tcp_received, K_NO_WAIT, server_h);
    if (rval < 0) {
        ERR_PRINT("Cannot receive TCP packet (family %d) rval=%d\n",
                  net_context_get_family(context), rval);
        // this seems to mean the remote exists but the connection was not made
        error_desc_t desc = create_error_desc(ERROR_ACCEPT_SERVER, 0, 0);
        zjs_defer_emit_event(server_h->server, "error", &desc, sizeof(desc),
                             handle_error_arg, zjs_release_args);
        return;
    }

    // under some conditions, we see Zephyr do an extra unref on the context
    //   so at one point we had a second ref here to work around it (ZEP-2598)
    net_context_ref(context);

    accept_connection_t accept;
    accept.context = context;
    accept.server_h = server_h;
    memset(&accept.addr, 0, sizeof(struct sockaddr));
    zjs_copy_sockaddr(&accept.addr, addr, addrlen);

    zjs_defer_work(accept_connection, &accept, sizeof(accept));
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
    FTRACE_JSAPI;
    GET_SERVER_HANDLE_JS(this, server_h);

    jerry_value_t info = zjs_create_object();
    zjs_obj_add_number(info, "port", server_h->port);

    sa_family_t family = net_context_get_family(server_h->server_ctx);
    char ipstr[INET6_ADDRSTRLEN];

    zjs_obj_add_string(info, "family", family == AF_INET6 ? "IPv6" : "IPv4");
    net_addr_ntop(family, zjs_get_inaddr(&server_h->local), ipstr,
                  INET6_ADDRSTRLEN);

    zjs_obj_add_string(info, "address", ipstr);
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

    GET_SERVER_HANDLE_JS(this, server_h);

    // make sure we accept no more connections
    server_h->listening = 0;
    server_h->closed = 1;
    zjs_obj_add_boolean(this, "listening", false);

    if (optcount) {
        zjs_add_event_listener(server_h->server, "close", argv[0]);
    }

    // if there are no connections, the server can be closed now
    S_LOCK();
    if (!server_h->connections) {
        close_server(server_h);
    }
    S_UNLOCK();

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

    GET_SERVER_HANDLE_JS(this, server_h);

    S_LOCK();
    int count = ZJS_LIST_LENGTH(sock_handle_t, server_h->connections);
    S_UNLOCK();

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

    GET_SERVER_HANDLE_JS(this, server_h);

    if (server_h->listening || server_h->closed) {
        return zjs_error("server not listening or closed");
    }

    int ret;
    double port = 0;
    double backlog = 0;
    u32_t size = NET_HOSTNAME_MAX;
    char hostname[size];
    u32_t family = 0;

    zjs_obj_get_double(argv[0], "port", &port);
    zjs_obj_get_double(argv[0], "backlog", &backlog);
    zjs_obj_get_string(argv[0], "host", hostname, size);
    zjs_obj_get_uint32(argv[0], "family", &family);

    // FIXME: validate or fix input, e.g. family

    if (optcount) {
        zjs_add_event_listener(this, "listening", argv[1]);
    }

    struct sockaddr addr;
    memset(&addr, 0, sizeof(struct sockaddr));
    addr.sa_family = (family == 6) ? AF_INET6 : AF_INET;  // default to IPv4

    CHECK(net_context_get(addr.sa_family, SOCK_STREAM, IPPROTO_TCP,
                          &server_h->server_ctx));

    u32_t addrlen;
    if (addr.sa_family == AF_INET) {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)&addr;
        addr4->sin_port = htons((int)port);
        net_addr_pton(AF_INET, hostname, &addr4->sin_addr);
        addrlen = sizeof(struct sockaddr_in);
    } else {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&addr;
        addr6->sin6_port = htons((int)port);
        net_addr_pton(AF_INET6, hostname, &addr6->sin6_addr);
        addrlen = sizeof(struct sockaddr_in6);
    }

    CHECK(net_context_bind(server_h->server_ctx, &addr, addrlen));
    CHECK(net_context_listen(server_h->server_ctx, (int)backlog));

    server_h->listening = 1;
    server_h->port = (u16_t)port;

    zjs_copy_sockaddr(&server_h->local,
                      zjs_net_config_get_ip(server_h->server_ctx), 0);
    zjs_obj_add_boolean(this, "listening", true);

    // Here we defer just to keep the call stack short; this is unless we
    //   determine that the listeners should be called before this function
    //   returns. Note, since this is a JS API function we can be sure we're in
    //   the main thread and can call JerryScript functions as above. Normally
    //   when we're calling defer_emit we put off those kinds of calls until
    //   we're sure to be back in the main thread during the pre_emit callback
    zjs_defer_emit_event(this, "listening", NULL, 0, NULL, NULL);

    CHECK(net_context_accept(server_h->server_ctx, tcp_accepted, 0, server_h));

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

    zjs_obj_add_boolean(server, "listening", false);
    zjs_obj_add_number(server, "maxConnections", NET_DEFAULT_MAX_CONNECTIONS);

    server_handle_t *server_h = zjs_malloc(sizeof(server_handle_t));
    if (!server_h) {
        jerry_release_value(server);
        return zjs_error("could not alloc server handle");
    }

    memset(server_h, 0, sizeof(server_handle_t));

    // hold a reference to the server object; we will have to release it
    //   before it can ever be freed, which we can only do when it has been
    //   explicitly closed and all its connections have closed
    server_h->server = jerry_acquire_value(server);

    zjs_make_emitter(server, zjs_net_server_prototype, server_h,
                     server_free_cb);

    if (optcount) {
        zjs_add_event_listener(server, "connection", argv[0]);
    }

    DBG_PRINT("creating server: context=%p\n", server_h->server_ctx);

    S_LOCK();
    ZJS_LIST_PREPEND(server_handle_t, servers, server_h);
    S_UNLOCK();

    return server;
}

// a zjs_pre_emit_callback
static bool connect_callback(void *h, jerry_value_t argv[], u32_t *argc,
                             const char *buffer, u32_t bytes)
{
    FTRACE("h = %p, buffer = %p, bytes = %d\n", h, buffer, bytes);
    sock_handle_t *handle = (sock_handle_t *)h;
    zjs_obj_add_boolean(handle->socket, "connecting", false);
    zjs_add_event_listener(handle->socket, "connect", handle->connect_listener);
    return true;
}

static void tcp_connected(struct net_context *context, int status,
                          void *user_data)
{
    FTRACE("context = %p, status = %d, user_data = %p\n", context, status,
           user_data);
    if (status == 0) {
        sock_handle_t *sock_handle = (sock_handle_t *)user_data;

        if (sock_handle) {
            int ret = net_context_recv(context, tcp_received, K_NO_WAIT,
                                       &no_server);
            if (ret < 0) {
                ERR_PRINT("Cannot receive TCP packets (%d)\n", ret);
            }
            // activity, restart timeout
            start_socket_timeout(sock_handle);

            // here we supply a pre callback to manipulate JerryScript objects
            //   from the main thread; but don't actually have args to pass
            zjs_defer_emit_event(sock_handle->socket, "connect", NULL, 0,
                                 connect_callback, NULL);

            DBG_PRINT("connection success, context=%p, socket=%p\n", context,
                      (void *)sock_handle->socket);
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

    DBG_PRINT("port=%u, host=%s, localPort=%u, localAddress=%s, socket=%p\n",
              (u32_t)port, host, (u32_t)localPort, localAddress, (void *)this);

    int ret;
    if (!handle->tcp_sock) {
        sa_family_t inet = (fam == 6) ? AF_INET6 : AF_INET;
        CHECK(net_context_get(inet, SOCK_STREAM, IPPROTO_TCP,
                              &handle->tcp_sock));
    }
    if (!handle->tcp_sock) {
        DBG_PRINT("failed to get context\n");
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
        zjs_obj_add_boolean(this, "connecting", true);
        // connect to remote
        int rval = net_context_connect(handle->tcp_sock,
                                       (struct sockaddr *)&peer_addr6,
                                       sizeof(peer_addr6), tcp_connected,
                                       1, handle);
        if (rval < 0) {
            DBG_PRINT("IPv6 connect failed: %d\n", rval);
            zjs_obj_add_boolean(this, "connecting", false);
            error_desc_t desc = create_error_desc(ERROR_CONNECT_SOCKET, this,
                                                  function_obj);
            zjs_defer_emit_event(this, "error", &desc, sizeof(desc),
                                 handle_error_arg, zjs_release_args);
            net_context_put(handle->tcp_sock);
            handle->tcp_sock = NULL;
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
        zjs_obj_add_boolean(this, "connecting", true);
        // connect to remote
        int rval  = net_context_connect(handle->tcp_sock,
                                        (struct sockaddr *)&peer_addr4,
                                        sizeof(peer_addr4), tcp_connected,
                                        1, handle);
        if (rval < 0) {
            DBG_PRINT("IPv4 connect failed: %d\n", rval);
            error_desc_t desc = create_error_desc(ERROR_CONNECT_SOCKET, this,
                                                  function_obj);
            zjs_defer_emit_event(this, "error", &desc, sizeof(desc),
                                 handle_error_arg, zjs_release_args);
            net_context_put(handle->tcp_sock);
            handle->tcp_sock = NULL;
            return ZJS_UNDEFINED;
        }
    }

    // add all the socket address information
    zjs_obj_add_string(this, "remoteAddress", host);
    zjs_obj_add_string(this, "remoteFamily", "IPv6");
    zjs_obj_add_number(this, "remotePort", port);

    zjs_obj_add_string(this, "localAddress", localAddress);
    zjs_obj_add_number(this, "localPort", localPort);
    sa_family_t family = net_context_get_family(handle->tcp_sock);
    if (family == AF_INET6) {
        zjs_obj_add_string(this, "family", "IPv6");
    } else {
        zjs_obj_add_string(this, "family", "IPv4");
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
    FTRACE_JSAPI;
    sock_handle_t *sock_handle = NULL;
    jerry_value_t socket = create_socket(true, &sock_handle);
    if (!sock_handle) {
        return zjs_error("could not alloc socket handle");
    }

    // add new socket to client list
    S_LOCK();
    ZJS_LIST_PREPEND(sock_handle_t, no_server.connections, sock_handle);
    S_UNLOCK();

    DBG_PRINT("socket created, context=%p, sock=%p\n", sock_handle->tcp_sock,
              (void *)socket);

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
    FTRACE_JSAPI;
    if (!jerry_value_is_string(argv[0]) || argc < 1) {
        return jerry_create_number(0);
    }
    jerry_size_t size = INET6_ADDRSTRLEN;
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
    FTRACE_JSAPI;
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
    FTRACE_JSAPI;
    ZVAL ret = net_is_ip(function_obj, this, argv, argc);
    double v = jerry_get_number_value(ret);
    if (v == 6) {
        return jerry_create_boolean(true);
    }
    return jerry_create_boolean(false);
}

static jerry_value_t net_obj;

static void zjs_net_cleanup(void *native)
{
    FTRACE("\n");
    jerry_release_value(zjs_net_prototype);
    jerry_release_value(zjs_net_socket_prototype);
    jerry_release_value(zjs_net_server_prototype);
}

static const jerry_object_native_info_t net_module_type_info = {
    .free_cb = zjs_net_cleanup
};

static jerry_value_t zjs_net_init()
{
    FTRACE("\n");
    zjs_net_config_default();

    k_mutex_init(&socket_mutex);

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
    // Set up cleanup function for when the object gets freed
    jerry_set_object_native_pointer(net_obj, NULL, &net_module_type_info);
    return jerry_acquire_value(net_obj);
}

JERRYX_NATIVE_MODULE(net, zjs_net_init)
#endif  // BUILD_MODULE_NET
