// Copyright (c) 2016, Intel Corporation.

#ifndef ZJS_LINUX_BUILD
#include <zephyr.h>
#include <misc/ring_buffer.h>
#include "zjs_zephyr_queue.h"
#include "zjs_zephyr_time.h"
#else
#include "zjs_linux_queue.h"
#include "zjs_linux_time.h"
#endif
#include <string.h>

#include "zjs_util.h"
#include "zjs_callbacks.h"

#include "jerry-api.h"

#define INITIAL_CALLBACK_SIZE  16
#define CB_CHUNK_SIZE          16

#define CALLBACK_TYPE_JS    0
#define CALLBACK_TYPE_C     1

struct zjs_callback_t {
    int32_t id;
    void* handle;
    zjs_pre_callback_func pre;
    zjs_post_callback_func post;
    jerry_value_t js_func;
};

struct zjs_c_callback_t {
    int32_t id;
    void* handle;
    zjs_c_callback_func function;
    uint32_t args_size;
};

struct zjs_callback_map {
    void* reserved;
    uint8_t type;
    union {
        struct zjs_callback_t* js;
        struct zjs_c_callback_t* c;
    };
};

static uint8_t args_buffer[1024];
SYS_RING_BUF_DECLARE_POW2(ring_buffer, 10);

struct zjs_port_queue service_queue;

static int32_t cb_limit = INITIAL_CALLBACK_SIZE;
static int32_t cb_size = 0;
static struct zjs_callback_map** cb_map = NULL;

static int32_t new_id(void)
{
    int32_t id = 0;
    if (cb_size >= cb_limit) {
        cb_limit += CB_CHUNK_SIZE;
        size_t size = sizeof(struct zjs_callback_map *) * cb_limit;
        struct zjs_callback_map** new_map = zjs_malloc(size);
        if (!new_map) {
            DBG_PRINT("[callbacks] new_id(): Error allocating space for new callback map\n");
            return -1;
        }
        DBG_PRINT("[callbacks] new_id(): Callback list size too small, increasing by %d\n",
                CB_CHUNK_SIZE);
        memset(new_map, 0, size);
        memcpy(new_map, cb_map, sizeof(struct zjs_callback_map *) * cb_size);
        zjs_free(cb_map);
        cb_map = new_map;
    }
    while (cb_map[id] != NULL) {
        id++;
    }
    return id;
}

void zjs_init_callbacks(void)
{
    if (!cb_map) {
        size_t size = sizeof(struct zjs_callback_map *) *
            INITIAL_CALLBACK_SIZE;
        cb_map = (struct zjs_callback_map**)zjs_malloc(size);
        if (!cb_map) {
            DBG_PRINT("[callbacks] zjs_init_callbacks(): Error allocating space for CB map\n");
            return;
        }
        memset(cb_map, 0, size);
    }
    zjs_port_init_queue(&service_queue);
    sys_ring_buf_init(&ring_buffer, 1024, args_buffer);
    return;
}

void zjs_edit_js_func(int32_t id, jerry_value_t func)
{
    if (id != -1) {
        jerry_release_value(cb_map[id]->js->js_func);
        cb_map[id]->js->js_func = jerry_acquire_value(func);
    }
}

int32_t zjs_add_callback(jerry_value_t js_func,
                         void* handle,
                         zjs_pre_callback_func pre,
                         zjs_post_callback_func post)
{
    struct zjs_callback_map* new_cb = zjs_malloc(sizeof(struct zjs_callback_map));
    if (!new_cb) {
        DBG_PRINT("[callbacks] zjs_add_callback(): Error allocating space for new callback\n");
        return -1;
    }
    new_cb->js = zjs_malloc(sizeof(struct zjs_callback_t));
    if (!new_cb->js) {
        DBG_PRINT("[callbacks] zjs_add_callback(): Error allocating space for new callback\n");
        return -1;
    }
    new_cb->type = CALLBACK_TYPE_JS;
    new_cb->js->id = new_id();
    new_cb->js->js_func = jerry_acquire_value(js_func);
    new_cb->js->pre = pre;
    new_cb->js->post = post;
    new_cb->js->handle = handle;

    // Add callback to list
    cb_map[new_cb->js->id] = new_cb;
    cb_size++;

    DBG_PRINT("[callbacks] zjs_add_callback(): Adding new callback id %u\n", new_cb->js->id);

    return new_cb->js->id;
}

void zjs_remove_callback(int32_t id)
{
    if (id != -1 && cb_map[id]) {
        if (cb_map[id]->type == CALLBACK_TYPE_JS && cb_map[id]->js) {
            jerry_release_value(cb_map[id]->js->js_func);
            zjs_free(cb_map[id]->js);
        } else if (cb_map[id]->c) {
            zjs_free(cb_map[id]->c);
        }
        zjs_free(cb_map[id]);
        cb_map[id] = NULL;
        cb_size--;
        DBG_PRINT("[callbacks] zjs_remove_callback(): Removing callback id %u\n", id);
    }
}

void zjs_signal_callback(int32_t id, void* args)
{
    if (id != -1 && cb_map[id]) {
#ifdef DEBUG_BUILD
        if (cb_map[id]->type == CALLBACK_TYPE_JS) {
            DBG_PRINT("[callbacks] zjs_signal_callback(): Signaling JS callback id %u\n", id);
        } else {
            DBG_PRINT("[callbacks] zjs_signal_callback(): Signaling C callback id %u\n", id);
        }
#endif
        zjs_port_queue_put(&service_queue, cb_map[id]);
        if (cb_map[id]->type == CALLBACK_TYPE_C && args) {
            PRINT("Storing args in ring buffer: %p\n", args);
            if (sys_ring_buf_put(&ring_buffer, 0, 0, args, cb_map[id]->c->args_size) != 0) {
                PRINT("zjs_signal_callback(): Ring buffer error putting\n");
            }
        }
    }
}

int32_t zjs_add_c_callback(void* handle, zjs_c_callback_func callback, uint32_t arg_size)
{
    struct zjs_callback_map* new_cb = zjs_malloc(sizeof(struct zjs_callback_map));
    if (!new_cb) {
        DBG_PRINT("[callbacks] zjs_add_c_callback(): Error allocating space for new callback\n");
        return -1;
    }
    new_cb->c = zjs_malloc(sizeof(struct zjs_c_callback_t));
    if (!new_cb->c) {
        DBG_PRINT("[callbacks] zjs_add_c_callback(): Error allocating space for new callback\n");
        return -1;
    }
    new_cb->type = CALLBACK_TYPE_C;
    new_cb->c->args_size = arg_size;
    new_cb->c->id = new_id();
    new_cb->c->function = callback;
    new_cb->c->handle = handle;

    // Add callback to list
    cb_map[new_cb->c->id] = new_cb;
    PRINT("new_cb=%p\n", new_cb);
    cb_size++;

    DBG_PRINT("[callbacks] zjs_add_callback(): Adding new C callback id %u, type=%u, pointer=%p\n", new_cb->c->id, cb_map[new_cb->c->id]->type, new_cb);

    return new_cb->c->id;
}

void zjs_service_callbacks(void)
{
    struct zjs_callback_map *cb;
    while (1) {
        cb = zjs_port_queue_get(&service_queue, ZJS_TICKS_NONE);
        if (!cb) {
            break;
        }
        if (cb->type == CALLBACK_TYPE_JS && jerry_value_is_function(cb->js->js_func)) {
            uint32_t args_cnt = 0;
            jerry_value_t ret_val;
            jerry_value_t* args = NULL;

            if (cb->js->pre) {
                args = cb->js->pre(cb->js->handle, &args_cnt);
            }

            DBG_PRINT("[callbacks] zjs_service_callbacks(): Calling callback id %u with %u args\n", cb->js->id, args_cnt);
            // TODO: Use 'this' in callback module
            jerry_call_function(cb->js->js_func, ZJS_UNDEFINED, args, args_cnt);
            if (cb->js->post) {
                cb->js->post(cb->js->handle, &ret_val);
            }
        } else if (cb->type == CALLBACK_TYPE_C && cb->c->function) {
            DBG_PRINT("[callbacks] zjs_service_callbacks(): Calling callback id %u\n", cb->c->id);
            uint16_t type;
            uint8_t value;
            uint32_t data[cb->c->args_size];
            uint8_t size;
            if (sys_ring_buf_get(&ring_buffer, &type, &value, data, &size) != 0) {
                PRINT("zjs_service_callbacks(): Ring buffer error getting\n");
            }
            if (size != cb->c->args_size) {
                PRINT("zjs_service_callbacks(): Error, size of ring buffer get did not match: ret size = %u, actual = %u\n", size, cb->c->args_size);
            }
            cb->c->function(cb->c->handle, data);
        }
    }
}
