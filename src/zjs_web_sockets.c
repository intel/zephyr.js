// Copyright (c) 2017-2018, Intel Corporation.

// enable to use function tracing for debug purposes
#if 0
#define USE_FTRACE
static char FTRACE_PREFIX[] = "ws";
#endif

// C includes
#include <errno.h>

// Zephyr includes
#include <zephyr.h>

#include "mbedtls/base64.h"
#include "mbedtls/sha1.h"
#include <net/net_context.h>
#include <net/net_core.h>
#include <net/net_if.h>
#include <net/net_mgmt.h>
#include <net/net_pkt.h>

#ifdef CONFIG_NET_L2_BT
#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#endif

// ZJS includes
#include "zjs_buffer.h"
#include "zjs_callbacks.h"
#include "zjs_event.h"
#include "zjs_modules.h"
#include "zjs_net_config.h"
#include "zjs_util.h"
#include "zjs_zephyr_port.h"

// JerryScript includes
#include "jerryscript.h"

/*
 * TODO:
 *   - Implement WebSocket client side
 */

#define DEFAULT_WS_BUFFER_SIZE 256
#define CHECK(x)                                 \
    ret = (x);                                   \
    if (ret < 0) {                               \
        ERR_PRINT("Error in " #x ": %d\n", ret); \
        return zjs_error(#x);                    \
    }

typedef enum {
    UNCONNECTED = 0,
    AWAITING_ACCEPT,
    CONNECTED
} ws_state;

typedef enum {
    WS_PACKET_CONTINUATION = 0x00,
    WS_PACKET_TEXT_DATA = 0x01,
    WS_PACKET_BINARY_DATA = 0x02,
    WS_PACKET_CLOSE = 0x08,
    WS_PACKET_PING = 0x09,
    WS_PACKET_PONG = 0x0a
} ws_packet_type;

// server handle
typedef struct server_handle {
    struct net_context *server_ctx;
    jerry_value_t accept_handler;
    jerry_value_t server;
    struct ws_connection *connections;
    u16_t max_payload;
    bool track;
} server_handle_t;

// WS connection handle, unique per server connection
typedef struct ws_connection {
    struct net_context *tcp_sock;
    server_handle_t *server_h;
    u8_t *rbuf;
    u8_t *wptr;
    u8_t *rptr;
    struct ws_connection *next;
    char *accept_key;
    jerry_value_t server;
    jerry_value_t conn;
    zjs_callback_id accept_handler_id;
    ws_state state;
} ws_connection_t;

// structure to hold a decoded WS packet
typedef struct ws_packet {
    u8_t *payload;
    u32_t payload_len;
    u32_t payload_offset;
    u8_t mask[4];
    u8_t fin;
    u8_t rsv1;
    u8_t rsv2;
    u8_t rsv3;
    u8_t opcode;
    u8_t mask_bit;
} ws_packet_t;

// start of header preceding accept key
static char accept_header[] = "HTTP/1.1 101 Switching Protocols\r\n"
                              "Upgrade: websocket\r\n"
                              "Connection: Upgrade\r\n"
                              "Sec-WebSocket-Accept: ";

// magic string for computing accept key
static char magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// get the socket handle or return a JS error
#define GET_WS_HANDLE_JS(obj, var)                                            \
    ws_connection_t *var = (ws_connection_t *)zjs_event_get_user_handle(obj); \
    if (!var) {                                                               \
        return zjs_error("no websocket handle");                              \
    }

static inline ws_connection_t *find_connection(server_handle_t *server_h,
                                               struct net_context *context)
{
    FTRACE("server_h = %p, context = %p\n", server_h, context);
    return ZJS_LIST_FIND(ws_connection_t, server_h->connections, tcp_sock,
                         context);
}

enum {
    ERROR_CONN_FAIL
};

static const char *error_messages[] = {
    "connection failed",
};

typedef struct error_desc {
    u32_t error_id;
    jerry_value_t this;
    jerry_value_t function_obj;
} error_desc_t;

// FIXME: create_error_desc is only used once now so all this could probably be
//   removed

static error_desc_t create_error_desc(u32_t error_id, jerry_value_t this,
                                      jerry_value_t function_obj)
{
    FTRACE("error_id = %d, this = %p, func = %p\n\n", error_id, (void *)this,
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

static void emit_error(jerry_value_t obj, const char *format, ...)
{
    // effects: emits an error event on obj w/ formatted string message
    FTRACE("obj = %p\n", (void *)obj);
    const int LEN = 128;
    char buf[LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, LEN, format, args);
    va_end(args);

    jerry_value_t error = zjs_error_context(buf, 0, 0);
    jerry_value_clear_error_flag(&error);
    zjs_emit_event(obj, "error", &error, 1);
}

// a zjs_event_free callback
static void free_server(void *native)
{
    FTRACE("native = %p\n", native);
    // effects: free up the server handle when it goes out of scope in JS
    server_handle_t *handle = (server_handle_t *)native;
    if (handle) {
        jerry_release_value(handle->accept_handler);
        net_context_put(handle->server_ctx);
        zjs_free(handle);
    }
}

#ifdef DEBUG_BUILD
static inline void pkt_sent(struct net_context *context, int status,
                            void *token, void *user_data)
{
    FTRACE("context = %p, status = %d, token = %p, user_data = %p\n",
           context, status, token, user_data);
    static int first = 1;
    if (first) {
        DBG_PRINT("TX Thread ID: %p\n", (void *)k_current_get());
        first = 0;
    }
}
#endif

static void tcp_send(struct net_context *context, void *data, u32_t len)
{
    FTRACE("context = %p, data = %p, len = %d\n", context, data, len);
    struct net_pkt *send_pkt;
    send_pkt = net_pkt_get_tx(context, K_NO_WAIT);
    if (!send_pkt) {
        ERR_PRINT("cannot acquire send_pkt\n");
        return;
    }

    bool status = net_pkt_append(send_pkt, len, data, K_NO_WAIT);
    if (!status) {
        net_pkt_unref(send_pkt);
        ERR_PRINT("cannot populate send_pkt\n");
        return;
    }

    void *func = NULL;
#ifdef DEBUG_BUILD
    // pass receipt function just to make a note of the TX thread ID
    func = pkt_sent;
#endif
    int ret = net_context_send(send_pkt, func, K_NO_WAIT,
                               UINT_TO_POINTER(net_pkt_get_len(send_pkt)),
                               NULL);
    if (ret < 0) {
        ERR_PRINT("Cannot send data to peer (%d)\n", ret);
        net_pkt_unref(send_pkt);
        return;
    }
}

// generate an accept key given an input key
static void generate_key(char *key, u32_t len, char *output, u32_t olen)
{
    FTRACE("key = '%s', len = %d, olen = %d\n", key, len, olen);
    u32_t concat_size = strlen(key) + strlen(magic) + 1;
    char *concat = zjs_malloc(concat_size);
    if (!concat) {
        ERR_PRINT("could not allocate key\n");
        return;
    }
    strcpy(concat, key);
    strcat(concat, magic);
    char sha_out[20];
    // compute sha1 hash of concatenated key + magic
    mbedtls_sha1(concat, strnlen(concat, 64), sha_out);
    size_t out_len;
    // base64 encode the sha1 hash
    mbedtls_base64_encode(output, olen, &out_len, sha_out, 20);
    zjs_free(concat);
}

#ifdef DEBUG_BUILD
// dump/print a WS packets binary information
static void dump_packet(ws_packet_t *packet)
{
    FTRACE("packet = %p\n", packet);
    ZJS_PRINT("FIN=         0x%02x\n", packet->fin);
    ZJS_PRINT("RSV1=        0x%02x\n", packet->rsv1);
    ZJS_PRINT("RSV2=        0x%02x\n", packet->rsv2);
    ZJS_PRINT("RSV3=        0x%02x\n", packet->rsv3);
    ZJS_PRINT("OPCODE=      0x%02x\n", packet->opcode);
    ZJS_PRINT("MASK=        0x%02x\n", packet->mask_bit);
    ZJS_PRINT("PayloadLen=  %d\n",  packet->payload_len);
    ZJS_PRINT("PayloadMask= [%02x%02x%02x%02x]\n",
              packet->mask[0],
              packet->mask[1],
              packet->mask[2],
              packet->mask[3]);
    ZJS_PRINT("Payload=     %s\n", (char*)packet->payload);
}

// dump an array of bytes, formatted (hex on left, ascii on right)
static void dump_bytes(const char *tag, u8_t *data, u32_t len)
{
    FTRACE("tag = '%s', data = %p, len = %d\n", tag, data, len);
    if (tag) {
        ZJS_PRINT("%s:\n", tag);
    } else {
        ZJS_PRINT("DUMP_BYTES:\n");
    }
    u8_t offset = 43;
    char line[81];
    memset(line, ' ', sizeof(line));
    line[80] = '\0';
    u8_t idx = 0;
    u32_t i = 0;
    while (i < len) {
        sprintf(line + (idx * 3), "%02x ", data[i]);
        line[(idx * 3) + 3] = ' ';
        if ((data[i] >= 32) && (data[i] <= 126)) {
            line[idx + offset] = data[i];
        } else {
            line[idx + offset] = '.';
        }
        idx++;
        i++;
        if ((((idx * 3) + offset) >= 80) || (i >= len)) {
            line[80] = '\0';
            ZJS_PRINT("%s\n", line);
            idx = 0;
            memset(line, ' ', sizeof(line));
        }
    }
}
#endif

// encode data into a WS packet
static int encode_packet(ws_packet_type type, u8_t mask, void *payload,
                         u16_t len, u8_t *out)
{
    FTRACE("type = %d, mask = %d, payload = %p, len = %d\n", (u32_t)type,
           (u32_t)mask, payload, (u32_t)len);
    u8_t mask_offset = 2;
    u8_t byte1 = 0;
    // FIN bit
    byte1 |= (1 << 7);
    // rsv[1-3] are unused
    // opcode is the bottom 4 bits of the first byte
    byte1 |= (type & 0xf);
    u8_t byte2 = 0;
    if (len <= 125) {
        // use encoded byte for length
        byte2 |= ((mask & 0x1) << 7);
        byte2 |= (len & 0x7f);
    } else if (len <= UINT16_MAX) {
        // use 16 bits for length
        memcpy(out + 2, &len, 2);
        mask_offset = 4;
    } else {
        ERR_PRINT("max length of data is %u\n", UINT16_MAX);
        return -1;
    }
    out[0] = byte1;
    out[1] = byte2;
    if (mask) {
        // data should be masked
        u32_t m = sys_rand32_get();
        u8_t mask_arr[4];
        // a uint8 array makes things easier for encoding
        memcpy(&mask_arr, &m, 4);
        // copy random mask to out buffer
        memcpy(out + mask_offset, &m, 4);
        int i;
        int j = 0;
        for (i = mask_offset + 4; i < len + mask_offset + 4; i++) {
            // mask each byte and copy to out buffer
            out[j + mask_offset + 4] = ((u8_t *)payload)[j] ^ mask_arr[j % 4];
            j++;
        }
        return mask_offset + 4 + len;
    } else {
        // no need to mask data, just copy payload as is
        memcpy(out + mask_offset, payload, len);
        return mask_offset + len;
    }
}

// decode WS TCP data into a ws_packet_t type
// Note: packet->payload must be freed after use
static int decode_packet(ws_packet_t *packet, u8_t *data, u32_t len)
{
    FTRACE("packet = %p, data = %p, len = %d\n", packet, data, len);
    u32_t mask_offset = 2;
    u8_t byte1 = data[0];
    u8_t byte2 = data[1];
    packet->fin = (byte1 & 0x1);           // FIN      bytes: 0 bits: 0
    packet->rsv1 = (byte1 & 0x2);          // RSV1     bytes: 0 bits: 1
    packet->rsv2 = (byte1 & 0x4);          // RSV2     bytes: 0 bits: 2
    packet->rsv3 = (byte1 & 0x8);          // RSV3     bytes: 0 bits: 3
    packet->opcode = (byte1 & 0xf);        // OPCODE   bytes: 0 bits: 4-7
    packet->mask_bit = (byte2 & 0x1);      // MASK_BIT bytes: 1 bits: 0
    packet->payload_len = (byte2 & 0x7f);  // PAY LEN  bytes: 1 bits: 1-7
    // payload length is less than 125 bytes
    if (packet->payload_len <= 125) {
        packet->payload_offset = 6;
    } else if (packet->payload_len == 126) {
        // payload length is greater than 125 bytes
        u8_t byte3 = data[2];              // PAY LEN  bytes: 2-3  bits: all
        u8_t byte4 = data[3];
        // decode payload length
        packet->payload_len = (byte3 << 8) | byte4;
        packet->payload_offset = 8;
        mask_offset = 4;
    } else if (packet->payload_len == 127) {
        /*
         * TODO: is this something we should support?
         */
        ERR_PRINT("only support payload <= 65,535 bytes\n");
        return -1;
    }
    // payload mask, 4 bytes
    packet->mask[0] = data[mask_offset];
    packet->mask[1] = data[mask_offset + 1];
    packet->mask[2] = data[mask_offset + 2];
    packet->mask[3] = data[mask_offset + 3];
    packet->payload = zjs_malloc(packet->payload_len + 1);
    if (!packet->payload) {
        ERR_PRINT("could not allocate packet payload\n");
        return -1;
    }
    memset(packet->payload, 0, packet->payload_len);
    int j = 0;
    int i;
    // decode payload using payload mask
    for (i = packet->payload_offset;
         i < packet->payload_len + packet->payload_offset; i++) {
        packet->payload[j] = data[i] ^ packet->mask[j % 4];
        j++;
    }
    packet->payload[packet->payload_len] = '\0';
    DBG_PRINT("packet decoded: %s\n", packet->payload);
    return 0;
}

static void consume_data(ws_connection_t *con, u16_t len)
{
    // effects: mark len bytes of data as read in the connection
    FTRACE("con = %p, len = %d\n", con, (u32_t)len);
    con->rptr += len;
    if (con->rptr == con->wptr) {
        DBG_PRINT("all data consumed, reseting read buffer\n");
        con->rptr = con->wptr = con->rbuf;
    }
}

// a zjs_pre_emit_callback
static bool trigger_data(void *h, jerry_value_t argv[], u32_t *argc,
                         const char *buffer, u32_t bytes)
{
    // requires: buffer contains 2-byte length
    FTRACE("h = %p, buffer = %p, bytes = %d\n", h, buffer, bytes);
    ZJS_ASSERT(bytes == sizeof(u16_t), "invalid data received");

    ws_connection_t *con = (ws_connection_t *)h;
    u16_t len = *(u16_t *)buffer;
    zjs_buffer_t *buf;
    jerry_value_t buf_obj = zjs_buffer_create(len, &buf);
    if (buf) {
        memcpy(buf->buffer, con->rptr, len);
        consume_data(con, len);
        argv[0] = buf_obj;
        *argc = 1;
        return true;
    }
    return false;
}

// a zjs_pre_emit callback
static bool pre_close_connection(void *handle, jerry_value_t argv[],
                                 u32_t *argc, const char *buffer, u32_t length)
{
    FTRACE("handle = %p, buffer = %p, length = %d\n", handle, buffer, length);

    jerry_value_t code = jerry_create_number(*((u16_t *)buffer));
    jerry_value_t reason = jerry_create_string("socket closed");

    argv[0] = code;
    argv[1] = reason;
    *argc = 2;
    return true;
}

// a zjs_post_emit callback
static void close_connection(void *h, jerry_value_t argv[], u32_t argc)
{
    FTRACE("h = %p, argc = %d\n", h, argc);
    ws_connection_t *con = (ws_connection_t *)h;

    ZJS_LIST_REMOVE(ws_connection_t, con->server_h->connections, con);

    net_context_put(con->tcp_sock);
    zjs_free(con->rbuf);
    zjs_free(con->accept_key);
    zjs_remove_callback(con->accept_handler_id);
    if (con->conn) {
        zjs_destroy_emitter(con->conn);
        jerry_release_value(con->conn);
    }
    zjs_free(con);
    DBG_PRINT("Freed socket: server %p, connections %p\n", con->server_h,
              con->server_h->connections);
}

// process a TCP WS packet
static void process_packet(ws_connection_t *con, u8_t *data, u32_t len)
{
    // requires: expects to be called from main thread; emits events directly
    FTRACE("con = %p, data = %p, len = %d\n", con, data, len);
    ws_packet_t *packet = zjs_malloc(sizeof(ws_packet_t));
    if (!packet) {
        ERR_PRINT("allocation failed\n");
        emit_error(con->conn, "out of memory");
        return;
    }
    memset(packet, 0, sizeof(ws_packet_t));
    if (decode_packet(packet, data, len) < 0) {
        ERR_PRINT("error decoding packet\n");
        zjs_free(packet->payload);
        zjs_free(packet);
        emit_error(con->conn, "decoding packet");
        return;
    }

    if (con->server_h->max_payload &&
        (packet->payload_len > con->server_h->max_payload)) {
        emit_error(con->server, "payload too large: %d > %d",
                   packet->payload_len, con->server_h->max_payload);
        zjs_free(packet->payload);
        zjs_free(packet);
        return;
    }

    memcpy(con->wptr, packet->payload, packet->payload_len);
    con->wptr += packet->payload_len;

    u16_t plen = packet->payload_len;

    // FIXME: deferred emit calls below can be changed to direct

    switch (packet->opcode) {
    case WS_PACKET_CONTINUATION:
        // continuation frame
    case WS_PACKET_TEXT_DATA:
        // text data
        zjs_defer_emit_event(con->conn, "message", &plen, sizeof(plen),
                             trigger_data, zjs_release_args);
        break;
    case WS_PACKET_BINARY_DATA:
        // binary data (TODO: why is this ignored?)
        DBG_PRINT("binary data - ignored\n");
        consume_data(con, packet->payload_len);
        break;
    case WS_PACKET_PING:
        zjs_defer_emit_event(con->conn, "ping", &plen, sizeof(plen),
                             trigger_data, zjs_release_args);
        break;
    case WS_PACKET_PONG:
        zjs_defer_emit_event(con->conn, "pong", &plen, sizeof(plen),
                             trigger_data, zjs_release_args);
        break;
    case WS_PACKET_CLOSE:
        zjs_defer_emit_event(con->conn, "close", packet->payload,
                             sizeof(packet->payload), pre_close_connection,
                             close_connection);
        break;
    default:
        DBG_PRINT("opcode 0x%02x not recognized\n", packet->opcode);
        break;
    }

#ifdef DEBUG_BUILD
    dump_packet(packet);
#endif
    zjs_free(packet->payload);
    zjs_free(packet);
}

static ZJS_DECL_FUNC_ARGS(ws_send_data, ws_packet_type type)
{
    ZJS_VALIDATE_ARGS_OPTCOUNT(optcount, Z_OBJECT, Z_OPTIONAL Z_BOOL);
    bool mask = 0;
    GET_WS_HANDLE_JS(this, con);
    zjs_buffer_t *buf = zjs_buffer_find(argv[0]);
    if (!buf) {
        ERR_PRINT("buffer not found\n");
        return TYPE_ERROR("invalid arguments");
    }
    if (optcount) {
        mask = jerry_get_boolean_value(argv[1]);
    }
    if (con->server_h->max_payload &&
        (buf->bufsize > con->server_h->max_payload)) {
        char msg[60];
        sprintf(msg, "payload too large: %d > %d", buf->bufsize,
                con->server_h->max_payload);
        return zjs_error(msg);
    }
    u8_t out[buf->bufsize + 10];
    u32_t out_len = encode_packet(type, mask, buf->buffer, buf->bufsize, out);
#ifdef DEBUG_BUILD
    dump_bytes("SEND DATA", out, out_len);
#endif
    DBG_PRINT("Sending %u bytes\n", out_len);
    tcp_send(con->tcp_sock, out, out_len);

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(ws_ping)
{
    FTRACE_JSAPI;
    return ws_send_data(function_obj, this, argv, argc, WS_PACKET_PING);
}

static ZJS_DECL_FUNC(ws_pong)
{
    FTRACE_JSAPI;
    return ws_send_data(function_obj, this, argv, argc, WS_PACKET_PONG);
}

static ZJS_DECL_FUNC(ws_send)
{
    FTRACE_JSAPI;
    return ws_send_data(function_obj, this, argv, argc, WS_PACKET_TEXT_DATA);
}

static ZJS_DECL_FUNC(ws_terminate)
{
    FTRACE_JSAPI;
    GET_WS_HANDLE_JS(this, con);
    DBG_PRINT("closing connection\n");
    close_connection(con, NULL, 0);
    return ZJS_UNDEFINED;
}

static jerry_value_t create_ws_connection(ws_connection_t *con)
{
    FTRACE("con = %p\n", con);
    // FIXME: this should be using a prototype
    jerry_value_t conn = zjs_create_object();
    zjs_obj_add_function(conn, "send", ws_send);
    zjs_obj_add_function(conn, "ping", ws_ping);
    zjs_obj_add_function(conn, "pong", ws_pong);
    zjs_obj_add_function(conn, "terminate", ws_terminate);
    zjs_make_emitter(conn, ZJS_UNDEFINED, con, NULL);
    if (con->server_h->track) {
        ZVAL clients = zjs_get_property(con->server_h->server, "clients");
        ZVAL new = zjs_push_array(clients, conn);
        zjs_set_property(con->server_h->server, "clients", new);
    }
    con->conn = jerry_acquire_value(conn);
    return conn;
}

typedef struct {
    struct net_context *context;
    server_handle_t *server_h;
    ws_connection_t *con;
    struct net_pkt *pkt;
} receive_packet_t;

// a zjs_deferred_work callback
static void receive_packet(const void *buffer, u32_t length)
{
    FTRACE("buffer = %p, length = %d\n", buffer, length);
    if (length != sizeof(receive_packet_t)) {
        // shouldn't happen so make it a debug print
        DBG_PRINT("Error: Unexpected data!\n");
        return;
    }

    receive_packet_t *receive = (receive_packet_t *)buffer;
    ws_connection_t *con = receive->con;
    struct net_pkt *pkt = receive->pkt;

    if (!con) {
        con = find_connection(receive->server_h, receive->context);
    }

    // if data has been received, after the accept packet, it's safe to assume
    //   the client connection was successful
    if (con && con->state == AWAITING_ACCEPT) {
        con->state = CONNECTED;
    }

    if (con && pkt) {
        u32_t len = net_pkt_appdatalen(pkt);

        DBG_PRINT("data received on context %p: len=%u\n", con->tcp_sock, len);

        struct net_buf *tmp = pkt->frags;
        u32_t header_len = net_pkt_appdata(pkt) - tmp->data;
        // move past IP header
        net_buf_pull(tmp, header_len);

        u8_t *data = zjs_malloc(len);
        if (!data) {
            DBG_PRINT("not enough memory to allocate data\n");
            emit_error(con->server, "out of memory");
            net_pkt_unref(pkt);
            return;
        }
        u8_t *wptr = data;
        while (tmp) {
            memcpy(wptr, tmp->data, tmp->len);
            wptr += tmp->len;
            tmp = tmp->frags;
        }

#ifdef DEBUG_BUILD
        dump_bytes("DATA", data, len);
#endif
        if (con->state == UNCONNECTED) {
            ZVAL_MUTABLE protocol_list = ZJS_UNDEFINED;
            con->accept_key = zjs_malloc(64);
            if (!con->accept_key) {
                DBG_PRINT("could not allocate accept key\n");
                emit_error(con->server, "out of memory");
                net_pkt_unref(pkt);
                zjs_free(data);
                return;
            }
            char *i = data;
            while ((i - (char *)data) < len) {
                char *tmp1 = i;
                while (*i != ':' && ((i - (char *)data) < len)) {
                    i++;
                }
                if ((i - (char *)data) >= len) {
                    break;
                }
                char field[(i - tmp1) + 1];
                memcpy(field, tmp1, i - tmp1);
                field[i - tmp1] = '\0';

                char *tmp2 = i;
                while (*i != '\n') {
                    i++;
                }
                char value[(i - tmp2) - 1];
                memcpy(value, tmp2 + 2, i - tmp2 - 2);
                value[i - tmp2 - 3] = '\0';

                if (strequal(field, "Sec-WebSocket-Key")) {
                    // compute and return accept key
                    DBG_PRINT("new connection key: %s\n", value);
                    memset(con->accept_key, 0, 64);
                    generate_key(value, strlen(value), con->accept_key, 64);
                    DBG_PRINT("accept key: %s\n", con->accept_key);
                } else if (strequal(field, "Sec-WebSocket-Protocol")) {
                    int protocols = 1;
                    char *i = value;
                    while (*i != '\0') {
                        if (*i == ',') {
                            i++;
                            *(i - 1) = '\0';
                            protocols++;
                        } else {
                            i++;
                        }
                    }
                    protocol_list = jerry_create_array(protocols);
                    char *iter = value;
                    for (int j = 0; j < protocols; j++) {
                        int len = strlen(iter);
                        ZVAL val = jerry_create_string(iter);
                        jerry_set_property_by_index(protocol_list, j, val);
                        DBG_PRINT("New protocol requested: %s\n", iter);
                        iter += len + 1;
                    }
                }
                i++;
            }

            // no handler registered, automatically accept connection
            if (con->accept_handler_id == -1) {
                // create the accept response
                char *send_data = zjs_malloc(strlen(accept_header) +
                                             strlen(con->accept_key) + 6);
                if (!send_data) {
                    DBG_PRINT("could not allocate accept message\n");
                    emit_error(con->server, "out of memory");
                    net_pkt_unref(pkt);
                    zjs_free(data);
                    return;
                }
                sprintf(send_data, "%s%s\r\n\r\n", accept_header,
                        con->accept_key);

                DBG_PRINT("Sending accept packet\n");
                tcp_send(con->tcp_sock, send_data, strlen(send_data));
                con->state = AWAITING_ACCEPT;
                zjs_free(send_data);
                zjs_free(con->accept_key);
                con->accept_key = NULL;

                jerry_value_t conn = create_ws_connection(con);
                zjs_emit_event(con->server, "connection", &conn, 1);
            } else {
                if (jerry_value_is_undefined(protocol_list)) {
                    // create empty array so handler can check length
                    protocol_list = jerry_create_array(0);
                }
                // handler registered, accepted based on return of handler
                zjs_signal_callback(con->accept_handler_id, &protocol_list,
                                    sizeof(jerry_value_t));
            }
        } else {
            process_packet(con, data, len);
        }
        zjs_free(data);
    }
    net_pkt_unref(pkt);
}

static void tcp_received(struct net_context *context,
                         struct net_pkt *pkt,
                         int status,
                         void *user_data)
{
    // requires: called from RX thread, use caution
    FTRACE("context = %p, pkt = %p, status = %d, user_data = %p\n", context,
           pkt, status, user_data);
    server_handle_t *server_h = (server_handle_t *)user_data;
    ws_connection_t *con = find_connection(server_h, context);

    if (status == 0 && pkt == NULL) {
        DBG_PRINT("socket closed\n");
        net_pkt_unref(pkt);
        if (con && con->state == CONNECTED) {
            // close the socket
            u16_t stat = (u16_t)status;
            zjs_defer_emit_event(con->conn, "close", &stat, sizeof(stat),
                                 pre_close_connection, close_connection);
            return;
        } else {
            DBG_PRINT("socket closed before connection was opened\n");
            if (con) {
                close_connection(con, NULL, 0);
            }
            return;
        }
    }

    receive_packet_t receive;
    receive.context = context;
    receive.server_h = server_h;
    receive.con = con;
    receive.pkt = pkt;

    zjs_defer_work(receive_packet, &receive, sizeof(receive));
}

static void post_accept_handler(void *handle, jerry_value_t ret_val)
{
    // requires: expects to be called from main thread; emits events directly
    FTRACE("handle = %p, ret_val = %p\n", handle, (void *)ret_val);
    ws_connection_t *con = (ws_connection_t *)handle;
    if (!jerry_value_is_string(ret_val)) {
        DBG_PRINT("no protocol returned\n");
        char send_data[] = "HTTP/1.1 401 Unauthorized\r\n\r\n";
        tcp_send(con->tcp_sock, send_data, strlen(send_data));
        return;
    }
    jerry_size_t size = 32;
    char proto[size];
    zjs_copy_jstring(ret_val, proto, &size);
    if (!size) {
        ERR_PRINT("acceptHandler() protocol string too large\n");
        return;
    }

    // create the accept response
    u32_t sdata_size = strlen(accept_header) +
                       strlen(con->accept_key) +
                       strlen(proto) + 32;
    char *send_data = zjs_malloc(sdata_size);
    if (!send_data) {
        ERR_PRINT("could not allocate accept message\n");
        emit_error(con->server, "out of memory");
        return;
    }
    snprintf(send_data, sdata_size,
             "%s%s\r\nSec-WebSocket-Protocol: %s\r\n\r\n", accept_header,
             con->accept_key, proto);

    DBG_PRINT("Sending accept packet\n");
    tcp_send(con->tcp_sock, send_data, strlen(send_data));
    con->state = AWAITING_ACCEPT;
    zjs_free(send_data);
    zjs_free(con->accept_key);
    con->accept_key = NULL;

    jerry_value_t conn = create_ws_connection(con);
    zjs_emit_event(con->server, "connection", &conn, 1);
}

typedef struct {
    struct net_context *context;
    server_handle_t *server_h;
} accept_connection_t;

// a zjs_deferred_work callback
static void accept_connection(const void *buffer, u32_t length)
{
    FTRACE("buffer = %p, length = %d\n", buffer, length);
    // FIXME: replace deferred calls below; no longer needed
    if (length != sizeof(accept_connection_t)) {
        // shouldn't happen so make it a debug print
        DBG_PRINT("Error: Unexpected data!\n");
        return;
    }

    accept_connection_t *accept = (accept_connection_t *)buffer;
    server_handle_t *server_h = accept->server_h;

    ws_connection_t *con = zjs_malloc(sizeof(ws_connection_t));
    if (!con) {
        ERR_PRINT("could not allocate connection handle\n");
        emit_error(server_h->server, "out of memory");
        return;
    }
    memset(con, 0, sizeof(ws_connection_t));
    con->tcp_sock = accept->context;
    con->rbuf = zjs_malloc(DEFAULT_WS_BUFFER_SIZE);
    if (!con->rbuf) {
        ERR_PRINT("could not allocate read buffer\n");
        emit_error(server_h->server, "out of memory");
        zjs_free(con);
        return;
    }
    memset(con->rbuf, 0, sizeof(DEFAULT_WS_BUFFER_SIZE));
    con->rptr = con->wptr = con->rbuf;
    con->server = server_h->server;
    con->server_h = server_h;
    if (jerry_value_is_function(server_h->accept_handler)) {
        con->accept_handler_id = zjs_add_callback(server_h->accept_handler,
                                                  server_h->server,
                                                  con, post_accept_handler);
    } else {
        con->accept_handler_id = -1;
    }

    ZJS_LIST_PREPEND(ws_connection_t, server_h->connections, con);
}

static void tcp_accepted(struct net_context *context,
                         struct sockaddr *addr,
                         socklen_t addrlen,
                         int error,
                         void *user_data)
{
    // requires: called from RX thread, use caution
    FTRACE("context = %p, addr = %p, addrlen = %d, error = %d, userdata = %p\n",
           context, addr, addrlen, error, user_data);
#ifdef DEBUG_BUILD
    static int first = 1;
    if (first) {
        DBG_PRINT("RX Thread ID: %p\n", (void *)k_current_get());
        first = 0;
    }
#endif
    DBG_PRINT("connection made, context %p error %d\n", context, error);

    server_handle_t *server_h = (server_handle_t *)user_data;

    accept_connection_t accept;
    accept.context = context;
    accept.server_h = server_h;

    // start listening for packets immediately so we don't miss any
    if (net_context_recv(context, tcp_received, K_NO_WAIT, server_h) < 0) {
        ERR_PRINT("Cannot receive TCP packet (family %d)\n",
                  net_context_get_family(context));
        // this seems to mean the remote exists but the connection was not made
        error_desc_t desc = create_error_desc(ERROR_CONN_FAIL, 0, 0);
        zjs_defer_emit_event(server_h->server, "error", &desc, sizeof(desc),
                             handle_error_arg, zjs_release_args);
        return;
    }

    zjs_defer_work(accept_connection, &accept, sizeof(accept));
}

static ZJS_DECL_FUNC(ws_server)
{
    ZJS_VALIDATE_ARGS_OPTCOUNT(optcount, Z_OBJECT, Z_OPTIONAL Z_FUNCTION);

    int ret;
    double port = 0;
    bool backlog = 0;
    double max_payload = 0;
    char host[64];

    server_handle_t *handle = zjs_malloc(sizeof(server_handle_t));
    if (!handle) {
        return zjs_error("out of memory");
    }
    memset(handle, 0, sizeof(server_handle_t));

    jerry_value_t server = zjs_create_object();

    zjs_obj_get_double(argv[0], "port", &port);
    zjs_obj_get_boolean(argv[0], "backlog", &backlog);
    zjs_obj_get_boolean(argv[0], "clientTracking", &handle->track);
    zjs_obj_get_double(argv[0], "maxPayload", &max_payload);
    if (!zjs_obj_get_string(argv[0], "host", host, 64)) {
        // no host given, default to 0.0.0.0 or ::
#ifdef CONFIG_NET_IPV6
        strcpy(host, "::");
#else
        strcpy(host, "0.0.0.0");
#endif
    }
    jerry_value_t accept_handler = zjs_get_property(argv[0], "acceptHandler");
    if (jerry_value_is_function(accept_handler)) {
        handle->accept_handler = jerry_acquire_value(accept_handler);
    }

    struct sockaddr addr;
    memset(&addr, 0, sizeof(struct sockaddr));

    if (zjs_is_ip(host) == 4) {
        CHECK(net_context_get(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                              &handle->server_ctx));

        struct sockaddr_in *my_addr = (struct sockaddr_in *)&addr;

        net_addr_pton(AF_INET, host, &my_addr->sin_addr);

        my_addr->sin_family = AF_INET;
        my_addr->sin_port = htons((int)port);
    } else {
        CHECK(net_context_get(AF_INET6, SOCK_STREAM, IPPROTO_TCP,
                              &handle->server_ctx));

        struct sockaddr_in6 *my_addr6 = (struct sockaddr_in6 *)&addr;

        net_addr_pton(AF_INET6, host, &my_addr6->sin6_addr);

        my_addr6->sin6_family = AF_INET6;
        my_addr6->sin6_port = htons((int)port);
    }

    CHECK(net_context_bind(handle->server_ctx, (struct sockaddr *)&addr,
                           sizeof(struct sockaddr)));
    CHECK(net_context_listen(handle->server_ctx, (int)backlog));
    CHECK(net_context_accept(handle->server_ctx, tcp_accepted, 0, handle));

    zjs_make_emitter(server, ZJS_UNDEFINED, handle, free_server);

    // FIXME: add a close method to this server; for now, we're just going
    //   keep the object around forever
    handle->server = jerry_acquire_value(server);
    handle->max_payload = (u16_t)max_payload;

    if (optcount) {
        zjs_add_event_listener(handle->server, "listening", argv[1]);
        zjs_defer_emit_event(handle->server, "listening", NULL, 0, NULL, NULL);
    }

    return server;
}

static void zjs_ws_cleanup(void *native)
{
    FTRACE("\n");
}

static const jerry_object_native_info_t ws_module_type_info = {
    .free_cb = zjs_ws_cleanup
};

static jerry_value_t zjs_ws_init()
{
    FTRACE("\n");
    zjs_net_config_default();

    jerry_value_t ws = zjs_create_object();
    zjs_obj_add_function(ws, "Server", ws_server);
    // Set up cleanup function for when the object gets freed
    jerry_set_object_native_pointer(ws, NULL, &ws_module_type_info);
    return ws;
}

JERRYX_NATIVE_MODULE(ws, zjs_ws_init)
