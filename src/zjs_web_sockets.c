// Copyright (c) 2017, Intel Corporation.

#include <zephyr.h>
#include <random.h>
#include <sections.h>
#include <errno.h>

#include <net/nbuf.h>
#include <net/net_if.h>
#include <net/net_core.h>
#include <net/net_context.h>
#include <net/net_mgmt.h>
#include "mbedtls/sha1.h"
#include "mbedtls/base64.h"

#ifdef CONFIG_NET_L2_BLUETOOTH
#include <bluetooth/bluetooth.h>
#include <gatt/ipss.h>
#include <bluetooth/conn.h>
#endif

// ZJS includes
#include "zjs_util.h"
#include "zjs_buffer.h"
#include "zjs_callbacks.h"
#include "zjs_event.h"
#include "zjs_modules.h"
#include "zjs_net_config.h"
#include "zjs_zephyr_port.h"

#include "jerryscript.h"

/*
 * TODO:
 *   - Implement WebSocket client side
 */

#define DEFAULT_WS_BUFFER_SIZE  256
#define CHECK(x) \
    ret = (x); if (ret < 0) { ERR_PRINT("Error in " #x ": %d\n", ret); return zjs_error(#x); }

typedef enum {
    UNCONNECTED = 0,
    AWAITING_ACCEPT,
    CONNECTED
} ws_state;

typedef enum {
    WS_PACKET_CONTINUATION = 0x00,
    WS_PACKET_TEXT_DATA = 0x01,
    WS_PACKET_BINARY_DATA = 0x02,
    WS_PACKET_PING = 0x09,
    WS_PACKET_PONG = 0x0a
} ws_packet_type;

// server handle
typedef struct server_handle {
    struct net_context *tcp_sock;
    jerry_value_t accept_handler;
    jerry_value_t server;
    uint16_t max_payload;
    bool track;
} server_handle_t;

// WS connection handle, unique per server connection
typedef struct ws_connection {
    struct net_context *tcp_sock;
    server_handle_t *server_handle;
    uint8_t *rbuf;
    uint8_t *wptr;
    uint8_t *rptr;
    struct ws_connection *next;
    char *accept_key;
    jerry_value_t server;
    jerry_value_t conn;
    zjs_callback_id trigger_msg_id;
    zjs_callback_id accept_handler_id;
    ws_state state;
} ws_connection_t;

// structure to hold a decoded WS packet
typedef struct ws_packet {
    uint8_t *payload;
    uint32_t payload_len;
    uint32_t payload_offset;
    uint8_t mask[4];
    uint8_t fin;
    uint8_t rsv1;
    uint8_t rsv2;
    uint8_t rsv3;
    uint8_t opcode;
    uint8_t mask_bit;
} ws_packet_t;

static const jerry_object_native_info_t ws_type_info =
{
   .free_cb = free_handle_nop
};

static void free_server(void *native)
{
    server_handle_t *handle = (server_handle_t *)native;
    if (handle) {
        jerry_release_value(handle->accept_handler);
        net_context_put(handle->tcp_sock);
        zjs_free(handle);
    }
}

static const jerry_object_native_info_t server_type_info =
{
   .free_cb = free_server
};


// start of header preceding accept key
static char accept_header[] = "HTTP/1.1 101 Switching Protocols\r\n"
                              "Upgrade: websocket\r\n"
                              "Connection: Upgrade\r\n"
                              "Sec-WebSocket-Accept: ";

// magic string for computing accept key
static char magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// list of opened connections
static ws_connection_t *connections = NULL;

/*
 * TODO: could probably move this to zjs_util
 */
static jerry_value_t push_array(jerry_value_t array, jerry_value_t val)
{
    if (!jerry_value_is_array(array)) {
        jerry_value_t new = jerry_create_array(1);
        jerry_set_property_by_index(new, 0, val);
        return new;
    } else {
        uint32_t size = jerry_get_array_length(array);
        jerry_value_t new = jerry_create_array(size + 1);
        for (int i = 0; i < size; i++) {
            ZVAL v = jerry_get_property_by_index(array, i);
            jerry_set_property_by_index(new, i, v);
        }
        jerry_set_property_by_index(new, size, val);
        return new;
    }
}

static void tcp_send(struct net_context *context, void *data, uint32_t len)
{
    struct net_buf *send_buf;
    send_buf = net_nbuf_get_tx(context, K_NO_WAIT);
    if (!send_buf) {
        ERR_PRINT("cannot acquire send_buf\n");
        return;
    }

    bool status = net_nbuf_append(send_buf, len, data, K_NO_WAIT);
    if (!status) {
        net_nbuf_unref(send_buf);
        ERR_PRINT("cannot populate send_buf\n");
        return;
    }

    int ret = net_context_send(send_buf,
                               NULL,
                               K_NO_WAIT,
                               UINT_TO_POINTER(net_buf_frags_len(send_buf)),
                               NULL);
    if (ret < 0) {
        ERR_PRINT("Cannot send data to peer (%d)", ret);
        net_nbuf_unref(send_buf);
        return;
    }

    net_nbuf_unref(send_buf);
}

// generate an accept key given an input key
static void generate_key(char *key, uint32_t len, char *output, uint32_t olen)
{
    uint32_t concat_size = strlen(key) + strlen(magic) + 1;
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
    ZJS_PRINT("FIN=         0x%02x\n", packet->fin);
    ZJS_PRINT("RSV1=        0x%02x\n",packet->rsv1);
    ZJS_PRINT("RSV2=        0x%02x\n",packet->rsv2);
    ZJS_PRINT("RSV3=        0x%02x\n",packet->rsv3);
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
static void dump_bytes(const char *tag, uint8_t *data, uint32_t len)
{
    if (tag) {
        ZJS_PRINT("%s:\n", tag);
    } else {
        ZJS_PRINT("DUMP_BYTES:\n");
    }
    uint8_t offset = 43;
    char line[81];
    memset(line, ' ', sizeof(line));
    line[80] = '\0';
    uint8_t idx = 0;
    uint32_t i = 0;
    while (i < len) {
        sprintf(line + (idx*3), "%02x ", data[i]);
        line[(idx*3) + 3] = ' ';
        if ((data[i] >= 32) && (data[i] <= 126)) {
            line[idx + offset] = data[i];
        } else {
            line[idx + offset] = '.';
        }
        idx++;
        i++;
        if ((((idx*3) + offset) >= 80) || (i >= len)) {
            line[80] = '\0';
            ZJS_PRINT("%s\n", line);
            idx = 0;
            memset(line, ' ', sizeof(line));
        }
    }
}
#endif

// encode data into a WS packet.
static int encode_packet(ws_packet_type type,
                         uint8_t mask,
                         void *payload,
                         uint16_t len,
                         uint8_t *out)
{
    uint8_t mask_offset = 2;
    uint8_t byte1 = 0;
    // FIN bit
    byte1 |= (1 << 7);
    // rsv[1-3] are unused
    // opcode is the bottom 4 bits of the first byte
    byte1 |= (type & 0xf);
    uint8_t byte2 = 0;
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
        uint32_t m = sys_rand32_get();
        uint8_t mask_arr[4];
        // a uint8 array makes things easier for encoding
        memcpy(&mask_arr, &m, 4);
        // copy random mask to out buffer
        memcpy(out + mask_offset, &m, 4);
        int i;
        int j = 0;
        for (i = mask_offset + 4; i < len + mask_offset + 4; i++) {
            // mask each byte and copy to out buffer
            out[j + mask_offset + 4] = ((uint8_t*)payload)[j] ^ mask_arr[j % 4];
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
static int decode_packet(ws_packet_t *packet, uint8_t *data, uint32_t len)
{
    uint32_t mask_offset = 2;
    uint8_t byte1 = data[0];
    uint8_t byte2 = data[1];
    packet->fin = (byte1 & 0x1);            // FIN      bytes: 0 bits: 0
    packet->rsv1 = (byte1 & 0x2);           // RSV1     bytes: 0 bits: 1
    packet->rsv2 = (byte1 & 0x4);           // RSV2     bytes: 0 bits: 2
    packet->rsv3 = (byte1 & 0x8);           // RSV3     bytes: 0 bits: 3
    packet->opcode = (byte1 & 0xf);         // OPCODE   bytes: 0 bits: 4-7
    packet->mask_bit = (byte2 & 0x1);       // MASK_BIT bytes: 1 bits: 0
    packet->payload_len = (byte2 & 0x7f);   // PAY LEN  bytes: 1 bits: 1-7
    // payload length is less than 125 bytes
    if (packet->payload_len <= 125) {
        packet->payload_offset = 6;
    } else if (packet->payload_len == 126) {
        // payload length is greater than 125 bytes
        uint8_t byte3 = data[2];            // PAY LEN  bytes: 2-3  bits: all
        uint8_t byte4 = data[3];
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
    for (i = packet->payload_offset; i < packet->payload_len + packet->payload_offset; i++) {
        packet->payload[j] = data[i] ^ packet->mask[j % 4];
        j++;
    }
    packet->payload[packet->payload_len] = '\0';
    DBG_PRINT("packet decoded: %s\n", packet->payload);
    return 0;
}

static void trigger_data(void *h, const void *args)
{
    ws_connection_t *con = (ws_connection_t *)h;
    uint32_t len_encode = *((uint32_t*)args);
    // decode length and opcode from void* handle
    uint16_t len = (len_encode & 0xffff);
    uint16_t opcode = ((len_encode & 0xffff0000) >> 16);
    zjs_buffer_t *zbuf;
    ZVAL data_buf = zjs_buffer_create(len, &zbuf);
    // copy data from read buffer
    memcpy(zbuf->buffer, con->rptr, len);
    con->rptr += len;
    if (con->rptr == con->wptr) {
        con->rptr = con->wptr = con->rbuf;
        DBG_PRINT("all data consumed, reseting read buffer\n");
    }
    switch (opcode) {
    case WS_PACKET_CONTINUATION:
        // continuation frame
    case WS_PACKET_TEXT_DATA:
        // text data
        zjs_trigger_event(con->conn, "message", &data_buf, 1, NULL, NULL);
        break;
    case WS_PACKET_BINARY_DATA:
        // binary data
        break;
    case WS_PACKET_PING:
        zjs_trigger_event(con->conn, "ping", &data_buf, 1, NULL, NULL);
        break;
    case WS_PACKET_PONG:
        zjs_trigger_event(con->conn, "pong", &data_buf, 1, NULL, NULL);
        break;
    default:
        ERR_PRINT("opcode 0x%02x not recognized\n", opcode);
        break;
    }
}

static void close_connection(ws_connection_t *con)
{
    ws_connection_t *cur = connections;
    if (cur->next == NULL) {
        if (cur != con) {
            ERR_PRINT("connection handle %p does not exist\n", con);
            return;
        }
        connections = NULL;
        net_context_put(cur->tcp_sock);
        zjs_free(cur->rbuf);
        if (cur->accept_key) {
            zjs_free(cur->accept_key);
        }
        zjs_remove_callback(cur->trigger_msg_id);
        zjs_remove_callback(cur->accept_handler_id);
        zjs_free(cur);
        DBG_PRINT("Freed socket: opened_sockets=%p\n", connections);
    } else {
        while (cur->next) {
            if (cur->next == con) {
                cur->next = cur->next->next;
                net_context_put(cur->tcp_sock);
                zjs_free(cur->rbuf);
                if (cur->accept_key) {
                    zjs_free(cur->accept_key);
                }
                zjs_remove_callback(cur->trigger_msg_id);
                zjs_remove_callback(cur->accept_handler_id);
                zjs_free(cur);
                DBG_PRINT("Freed socket: opened_sockets=%p\n", connections);
            }
        }
    }
}

// process a TCP WS packet
static void process_packet(ws_connection_t *con, uint8_t *data, uint32_t len)
{
    ws_packet_t *packet = zjs_malloc(sizeof(ws_packet_t));
    if (!packet) {
        ERR_PRINT("allocation failed\n");
        ZVAL_MUTABLE error = zjs_custom_error("OutOfMemoryError", "could not allocate packet");
        jerry_value_clear_error_flag(&error);
        zjs_trigger_event(con->conn, "error", &error, 1, NULL, NULL);
        return;
    }
    memset(packet, 0, sizeof(ws_packet_t));
    if (decode_packet(packet, data, len) < 0) {
        ERR_PRINT("error decoding packet\n");
        zjs_free(packet->payload);
        zjs_free(packet);
        ZVAL_MUTABLE error = zjs_custom_error("PacketDecodeError", "error decoding packet");
        jerry_value_clear_error_flag(&error);
        zjs_trigger_event(con->conn, "error", &error, 1, NULL, NULL);
        return;
    }

    uint32_t len_encode = 0;
    // encode packet length and packet type in the void* handle
    len_encode |= packet->payload_len;
    len_encode |= (packet->opcode << 16);
    memcpy(con->wptr, packet->payload, packet->payload_len);
    con->wptr += packet->payload_len;
    zjs_signal_callback(con->trigger_msg_id, (void*)&len_encode, 4);

#ifdef DEBUG_BUILD
    dump_packet(packet);
#endif
    zjs_free(packet->payload);
    zjs_free(packet);
}

static jerry_value_t ws_send_data(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc,
                                  ws_packet_type type)
{
    ZJS_VALIDATE_ARGS_OPTCOUNT(optcount, Z_OBJECT, Z_OPTIONAL Z_BOOL);
    bool mask = 0;
    ZJS_GET_HANDLE(this, ws_connection_t, con, ws_type_info);
    zjs_buffer_t *buf = zjs_buffer_find(argv[0]);
    if (!buf) {
        ERR_PRINT("buffer not found\n");
        return TYPE_ERROR("invalid arguments");
    }
    if (optcount) {
        mask = jerry_get_boolean_value(argv[1]);
    }
    if (con->server_handle->max_payload &&
       (buf->bufsize > con->server_handle->max_payload)) {
        return zjs_error("payload too large");
    }
    uint8_t out[buf->bufsize + 10];
    uint32_t out_len = encode_packet(type, mask, buf->buffer, buf->bufsize, out);
#ifdef DEBUG_BUILD
    dump_bytes("SEND DATA", out, out_len);
#endif
    DBG_PRINT("Sending %u bytes\n", out_len);
    tcp_send(con->tcp_sock, out, out_len);

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(ws_ping)
{
    return ws_send_data(function_obj, this, argv, argc, WS_PACKET_PING);
}

static ZJS_DECL_FUNC(ws_pong)
{
    return ws_send_data(function_obj, this, argv, argc, WS_PACKET_PONG);
}

static ZJS_DECL_FUNC(ws_send)
{
    return ws_send_data(function_obj, this, argv, argc, WS_PACKET_TEXT_DATA);
}

static ZJS_DECL_FUNC(ws_terminate)
{
    ZJS_GET_HANDLE(this, ws_connection_t, con, ws_type_info);
    DBG_PRINT("closing connection\n");
    close_connection(con);
    return ZJS_UNDEFINED;
}

static jerry_value_t create_ws_connection(ws_connection_t *con)
{
    jerry_value_t conn = jerry_create_object();
    zjs_obj_add_function(conn, ws_send, "send");
    zjs_obj_add_function(conn, ws_ping, "ping");
    zjs_obj_add_function(conn, ws_pong, "pong");
    zjs_obj_add_function(conn, ws_terminate, "terminate");
    jerry_set_object_native_pointer(conn, con, &ws_type_info);
    zjs_make_event(conn, ZJS_UNDEFINED);
    if (con->server_handle->track) {
        ZVAL clients = zjs_get_property(con->server_handle->server, "clients");
        ZVAL new = push_array(clients, conn);
        zjs_set_property(con->server_handle->server, "clients", new);
    }
    return conn;
}

static void tcp_received(struct net_context *context,
                         struct net_buf *buf,
                         int status,
                         void *user_data)
{
    ws_connection_t *con = (ws_connection_t *)user_data;
    uint32_t len = net_nbuf_appdatalen(buf);
    uint8_t *buffer = net_nbuf_appdata(buf);

    DBG_PRINT("data recieved on context %p: data=%p, len=%u\n", con->tcp_sock, buffer, len);

    if (buffer && len == 0) {
        // ack from accept
        /*
         * TODO: there may be other cases where this is true but not an ACK.
         *       it would be better to figure out how to process the IP header
         *       and find if it was indeed an ACK, possibly storing the SEQ
         *       number of the accept response to match it.
         */
        if (con->state == AWAITING_ACCEPT) {
            con->state = CONNECTED;
            con->conn = create_ws_connection(con);
            zjs_trigger_event(con->server, "connection", &con->conn, 1, NULL, NULL);
        }
    }

    if (len && buffer) {
        struct net_buf *tmp = buf->frags;
        uint32_t header_len = net_nbuf_appdata(buf) - tmp->data;
        // move past IP header
        net_buf_pull(tmp, header_len);

        uint8_t *data = zjs_malloc(len);
        if (!data) {
            ERR_PRINT("not enough memory to allocate data\n");
            ZVAL_MUTABLE error = zjs_error("out of memory");
            jerry_value_clear_error_flag(&error);
            zjs_trigger_event(con->server, "error", &error, 1, NULL, NULL);
            net_nbuf_unref(buf);
            return;
        }
        uint8_t *wptr = data;
        while (tmp) {
            memcpy(wptr, tmp->data, tmp->len);
            wptr += tmp->len;
            tmp = tmp->frags;
        }

#ifdef DEBUG_BUILD
        dump_bytes("DATA", data, len);
#endif
        if (con->server_handle->max_payload &&
            (len > con->server_handle->max_payload)) {
            ZVAL_MUTABLE error = zjs_error("payload too large");
            jerry_value_clear_error_flag(&error);
            zjs_trigger_event(con->server, "error", &error, 1, NULL, NULL);
            net_nbuf_unref(buf);
            zjs_free(data);
            return;
        }
        if (con->state == UNCONNECTED) {
            ZVAL_MUTABLE protocol_list;
            con->accept_key = zjs_malloc(64);
            if (!con->accept_key) {
                ERR_PRINT("could not allocate accept key\n");
                ZVAL_MUTABLE error = zjs_error("out of memory");
                jerry_value_clear_error_flag(&error);
                zjs_trigger_event(con->server, "error", &error, 1, NULL, NULL);
                net_nbuf_unref(buf);
                zjs_free(data);
                return;
            }
            char *i = data;
            while ((i - (char*)data) < len) {
                char *tmp1 = i;
                while (*i != ':' && ((i - (char*)data) < len )) {
                    i++;
                }
                if ((i - (char*)data) >= len) {
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

                if (strcmp(field, "Sec-WebSocket-Key") == 0) {
                    // compute and return accept key
                    DBG_PRINT("new connection key: %s\n", value);
                    memset(con->accept_key, 0, 64);
                    generate_key(value, strlen(value), con->accept_key, 64);
                    DBG_PRINT("accept key: %s\n", con->accept_key);
                } else if (strcmp(field, "Sec-WebSocket-Protocol") == 0) {
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
                    ERR_PRINT("could not allocate accept message\n");
                    ZVAL_MUTABLE error = zjs_error("out of memory");
                    jerry_value_clear_error_flag(&error);
                    zjs_trigger_event(con->server, "error", &error, 1, NULL, NULL);
                    net_nbuf_unref(buf);
                    zjs_free(data);
                    return;
                }
                memset(send_data, 0, strlen(accept_header) +
                                     strlen(con->accept_key) + 6);
                memcpy(send_data, accept_header, strlen(accept_header));
                strcat(send_data, con->accept_key);
                strcat(send_data, "\r\n\r\n\0");

                DBG_PRINT("Sending accept packet\n");
                tcp_send(con->tcp_sock, send_data, strlen(send_data));
                con->state = AWAITING_ACCEPT;
                zjs_free(send_data);
                zjs_free(con->accept_key);
                con->accept_key = NULL;
            } else {
                // handler registered, accepted based on return of handler
                zjs_signal_callback(con->accept_handler_id,
                                    &protocol_list,
                                    sizeof(jerry_value_t));
            }
        } else {
            process_packet(con, data, len);
        }
        zjs_free(data);
    } else if (buffer == NULL && len == 0) {
        DBG_PRINT("socket closed\n");
        // close the socket
        ZVAL code = jerry_create_number(0);
        ZVAL reason = jerry_create_string("socket was closed");
        jerry_value_t args[] = {code, reason};
        zjs_trigger_event(con->conn, "close", args, 2, NULL, NULL);
        close_connection(con);
    }
    net_nbuf_unref(buf);
}

static void post_accept_handler(void *handle, jerry_value_t ret_val)
{
    ws_connection_t *con = (ws_connection_t *)handle;
    if (!jerry_value_is_string(ret_val)) {
        DBG_PRINT("no protocol returned\n");
        char send_data[] = "HTTP/1.1 401 Unauthorized\r\n\r\n";
        tcp_send(con->tcp_sock, send_data, strlen(send_data));
        return;
    }
    uint32_t size = 32;
    char proto[size];
    zjs_copy_jstring(ret_val, proto, &size);
    if (!size) {
        ERR_PRINT("acceptHandler() protocol string too large\n");
        return;
    }

    // create the accept response
    uint32_t sdata_size = strlen(accept_header) +
                          strlen(con->accept_key) +
                          strlen(proto) + 32;
    char *send_data = zjs_malloc(sdata_size);
    if (!send_data) {
        ERR_PRINT("could not allocate accept message\n");
        ZVAL_MUTABLE error = zjs_error("out of memory");
        jerry_value_clear_error_flag(&error);
        zjs_trigger_event(con->server, "error", &error, 1, NULL, NULL);
        return;
    }
    memset(send_data, 0, sdata_size);
    memcpy(send_data, accept_header, strlen(accept_header));
    strcat(send_data, con->accept_key);
    strcat(send_data, "\r\n");
    strcat(send_data, "Sec-WebSocket-Protocol: ");
    strcat(send_data, proto);
    strcat(send_data, "\r\n\r\n\0");

    DBG_PRINT("Sending accept packet\n");
    tcp_send(con->tcp_sock, send_data, strlen(send_data));
    con->state = AWAITING_ACCEPT;
    zjs_free(send_data);
    zjs_free(con->accept_key);
    con->accept_key = NULL;
}

static void tcp_accepted(struct net_context *context,
                         struct sockaddr *addr,
                         socklen_t addrlen,
                         int error,
                         void *user_data)
{
    int ret;
    server_handle_t *handle = (server_handle_t *)user_data;
    DBG_PRINT("connection made, context %p error %d\n", context, error);
    ws_connection_t *new = zjs_malloc(sizeof(ws_connection_t));
    if (!new) {
        ERR_PRINT("could not allocate connection handle\n");
        ZVAL_MUTABLE error = zjs_error("out of memory");
        jerry_value_clear_error_flag(&error);
        zjs_trigger_event(handle->server, "error", &error, 1, NULL, NULL);
        return;
    }
    memset(new, 0, sizeof(ws_connection_t));
    new->tcp_sock = context;
    new->rbuf = zjs_malloc(DEFAULT_WS_BUFFER_SIZE);
    if (!new->rbuf) {
        ERR_PRINT("could not allocate read buffer\n");
        ZVAL_MUTABLE error = zjs_error("out of memory");
        jerry_value_clear_error_flag(&error);
        zjs_trigger_event(handle->server, "error", &error, 1, NULL, NULL);
        zjs_free(new);
        return;
    }
    memset(new->rbuf, 0, sizeof(DEFAULT_WS_BUFFER_SIZE));
    new->rptr = new->wptr = new->rbuf;
    new->server = handle->server;
    new->trigger_msg_id = zjs_add_c_callback(new, trigger_data);
    if (jerry_value_is_function(handle->accept_handler)) {
        new->accept_handler_id = zjs_add_callback(handle->accept_handler,
                                                  new->server_handle->server,
                                                  new,
                                                  post_accept_handler);
    } else {
        new->accept_handler_id = -1;
    }
    new->next = connections;
    connections = new;

    ret = net_context_recv(context, tcp_received, 0, new);
    if (ret < 0) {
        ERR_PRINT("Cannot receive TCP packet (family %d)\n",
            net_context_get_family(context));
        // this seems to mean the remote exists but the connection was not made
        zjs_trigger_event(handle->server, "error", NULL, 0, NULL, NULL);
        return;
    }
}

static ZJS_DECL_FUNC(ws_server)
{
    ZJS_VALIDATE_ARGS_OPTCOUNT(optcount, Z_OBJECT, Z_OPTIONAL Z_FUNCTION);

    int ret;
    double port = 0;
    bool backlog = 0;
    double max_payload = 0;

    server_handle_t *handle = zjs_malloc(sizeof(server_handle_t));
    if (!handle) {
        return zjs_error("out of memory");
    }
    memset(handle, 0, sizeof(server_handle_t));

    jerry_value_t server = jerry_create_object();

    zjs_obj_get_double(argv[0], "port", &port);
    zjs_obj_get_boolean(argv[0], "backlog", &backlog);
    zjs_obj_get_boolean(argv[0], "clientTracking", &handle->track);
    zjs_obj_get_double(argv[0], "maxPayload", &max_payload);
    jerry_value_t accept_handler = zjs_get_property(argv[0], "acceptHandler");
    if (jerry_value_is_function(accept_handler)) {
        handle->accept_handler = jerry_acquire_value(accept_handler);
    }

    CHECK(net_context_get(AF_INET6, SOCK_STREAM, IPPROTO_TCP,
                &handle->tcp_sock));

    struct sockaddr_in6 my_addr6 = { 0 };

    my_addr6.sin6_family = AF_INET6;
    my_addr6.sin6_port = htons((int)port);

    CHECK(net_context_bind(handle->tcp_sock,
                           (struct sockaddr *)&my_addr6,
                           sizeof(struct sockaddr_in6)));
    CHECK(net_context_listen(handle->tcp_sock, (int)backlog));
    CHECK(net_context_accept(handle->tcp_sock, tcp_accepted, 0, handle));

    if (optcount) {
        zjs_add_event_listener(handle->server, "listening", argv[1]);
        zjs_trigger_event(handle->server, "listening", NULL, 0, NULL, NULL);
    }

    handle->server = server;
    handle->max_payload = (uint16_t)max_payload;

    zjs_make_event(server, ZJS_UNDEFINED);

    jerry_set_object_native_pointer(server, handle, &server_type_info);

    return server;
}

jerry_value_t zjs_ws_init()
{
    zjs_net_config();

    jerry_value_t ws = jerry_create_object();
    zjs_obj_add_function(ws, ws_server, "Server");

    return ws;
}

void zjs_ws_cleanup()
{
}
