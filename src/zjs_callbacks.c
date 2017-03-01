// Copyright (c) 2016-2017, Intel Corporation.

#ifndef ZJS_LINUX_BUILD
#include <zephyr.h>
#include <misc/ring_buffer.h>
#include "zjs_zephyr_port.h"
#else
#include "zjs_linux_port.h"
#endif
#include <string.h>

#include "zjs_util.h"
#include "zjs_callbacks.h"

#include "jerry-api.h"

// this could be defined with config options in the future
#ifndef ZJS_CALLBACK_BUF_SIZE
#define ZJS_CALLBACK_BUF_SIZE   1024
#endif
// max number of callbacks that can be serviced before continuing execution. If
// this value is reached, any additional callbacks will be serviced on the next
// time around the main loop.
#ifndef ZJS_MAX_CB_LOOP_ITERATION
#define ZJS_MAX_CB_LOOP_ITERATION   12
#endif

#define INITIAL_CALLBACK_SIZE  16
#define CB_CHUNK_SIZE          16
#define CB_LIST_MULTIPLIER  4

// flag bit value for JS callback
#define CALLBACK_TYPE_JS    0
// flag bit value for C callback
#define CALLBACK_TYPE_C     1
// flag bit value for single JS callback
#define JS_TYPE_SINGLE      0
// flag bit value for list JS callback
#define JS_TYPE_LIST        1
// Bits in flags for once, type (C or JS), and JS type (single or list)
#define ONCE_BIT    0
#define TYPE_BIT    1
#define JS_TYPE_BIT 2
// Macros to set the bits in flags
#define SET_ONCE(f, b)      f |= (b << ONCE_BIT)
#define SET_TYPE(f, b)      f |= (b << TYPE_BIT)
#define SET_JS_TYPE(f, b)   f |= (b << JS_TYPE_BIT)
// Macros to get the bits in flags
#define GET_ONCE(f)         (f & (1 << ONCE_BIT)) >> ONCE_BIT
#define GET_TYPE(f)         (f & (1 << TYPE_BIT)) >> TYPE_BIT
#define GET_JS_TYPE(f)      (f & (1 << JS_TYPE_BIT)) >> JS_TYPE_BIT

// FIXME: func_list is really an array :)
struct zjs_callback_t {
    zjs_callback_id id;
    void* handle;
    uint8_t flags;      // holds once and type bits
    zjs_post_callback_func post;
    jerry_value_t this;
    uint8_t max_funcs;
    uint8_t num_funcs;
    union {
        jerry_value_t js_func;          // Single JS function callback
        jerry_value_t* func_list;       // JS callback list
        zjs_c_callback_func function;   // C callback
    };
};

#ifdef ZJS_LINUX_BUILD
static uint8_t args_buffer[ZJS_CALLBACK_BUF_SIZE];
static struct zjs_port_ring_buf ring_buffer;
#else
SYS_RING_BUF_DECLARE_POW2(ring_buffer, 5);
#endif
static uint8_t ring_buf_initialized = 1;

static zjs_callback_id cb_limit = INITIAL_CALLBACK_SIZE;
static zjs_callback_id cb_size = 0;
static struct zjs_callback_t** cb_map = NULL;

static int zjs_ringbuf_error_count = 0;
static int zjs_ringbuf_last_error = 0;

static zjs_callback_id new_id(void)
{
    zjs_callback_id id = 0;
    // NOTE: We could wait until after we check for a NULL callback slot in
    //   the map before we increase allocation; or else store a count that
    //   can be less than cb_size when callbacks are removed.
    if (cb_size >= cb_limit) {
        cb_limit += CB_CHUNK_SIZE;
        size_t size = sizeof(struct zjs_callback_t *) * cb_limit;
        struct zjs_callback_t** new_map = zjs_malloc(size);
        if (!new_map) {
            DBG_PRINT("error allocating space for new callback map\n");
            return -1;
        }
        DBG_PRINT("callback list size too small, increasing by %d\n",
                  CB_CHUNK_SIZE);
        memset(new_map, 0, size);
        memcpy(new_map, cb_map, sizeof(struct zjs_callback_t *) * cb_size);
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
        size_t size = sizeof(struct zjs_callback_t *) *
            INITIAL_CALLBACK_SIZE;
        cb_map = (struct zjs_callback_t**)zjs_malloc(size);
        if (!cb_map) {
            DBG_PRINT("error allocating space for CB map\n");
            return;
        }
        memset(cb_map, 0, size);
    }
#ifdef ZJS_LINUX_BUILD
    zjs_port_ring_buf_init(&ring_buffer, ZJS_CALLBACK_BUF_SIZE,
                           (uint32_t*)args_buffer);
#endif
    ring_buf_initialized = 1;
    return;
}

bool zjs_edit_js_func(zjs_callback_id id, jerry_value_t func)
{
    if (id != -1 && cb_map[id]) {
        jerry_release_value(cb_map[id]->js_func);
        cb_map[id]->js_func = jerry_acquire_value(func);
        return true;
    } else {
        return false;
    }
}

bool zjs_edit_callback_handle(zjs_callback_id id, void* handle)
{
    if (id != -1 && cb_map[id]) {
        cb_map[id]->handle = handle;
        return true;
    } else {
        return false;
    }
}

bool zjs_remove_callback_list_func(zjs_callback_id id, jerry_value_t js_func)
{
    if (id != -1 && cb_map[id]) {
        int i;
        for (i = 0; i < cb_map[id]->num_funcs; ++i) {
            if (js_func == cb_map[id]->func_list[i]) {
                int j;
                jerry_release_value(cb_map[id]->func_list[i]);
                for (j = i; j < cb_map[id]->num_funcs - 1; ++j) {
                    cb_map[id]->func_list[j] = cb_map[id]->func_list[j + 1];
                }
                cb_map[id]->num_funcs--;
                cb_map[id]->func_list[cb_map[id]->num_funcs] = 0;
                return true;
            }
        }
    }
    DBG_PRINT("could not remove callback %ld\n", (uint32_t)id);
    return false;
}

int zjs_get_num_callbacks(zjs_callback_id id)
{
    if (id != -1 && cb_map[id]) {
        return cb_map[id]->num_funcs;
    }
    return 0;
}

jerry_value_t* zjs_get_callback_func_list(zjs_callback_id id, int* count)
{
    if (id != -1) {
        if (cb_map[id]) {
            *count = cb_map[id]->num_funcs;
            return cb_map[id]->func_list;
        }
    }
    return NULL;
}

zjs_callback_id zjs_add_callback_list(jerry_value_t js_func,
                                      jerry_value_t this,
                                      void* handle,
                                      zjs_post_callback_func post,
                                      zjs_callback_id id)
{
    if (id != -1) {
        if (cb_map[id] && cb_map[id]->func_list) {
            // The function list is full, allocate more space, copy the existing
            // list, and add the new function
            if (cb_map[id]->num_funcs == cb_map[id]->max_funcs - 1) {
                int i;
                jerry_value_t* new_list = zjs_malloc((sizeof(jerry_value_t) *
                        (cb_map[id]->max_funcs + CB_LIST_MULTIPLIER)));
                for (i = 0; i < cb_map[id]->num_funcs; ++i) {
                    new_list[i] = cb_map[id]->func_list[i];
                }
                new_list[cb_map[id]->num_funcs] = jerry_acquire_value(js_func);

                cb_map[id]->max_funcs += CB_LIST_MULTIPLIER;
                zjs_free(cb_map[id]->func_list);
                cb_map[id]->func_list = new_list;
            } else {
                // Add function to list
                cb_map[id]->func_list[cb_map[id]->num_funcs] =
                        jerry_acquire_value(js_func);
            }
            // If not already set, set the handle/pre/post provided. These will
            // only be set once, when the list is created.
            if (!cb_map[id]->handle) {
                cb_map[id]->handle = handle;
            }
            if (!cb_map[id]->post) {
                cb_map[id]->post = post;
            }
            cb_map[id]->num_funcs++;
            return cb_map[id]->id;
        } else {
            DBG_PRINT("list handle was NULL\n");
            return -1;
        }
    } else {
        struct zjs_callback_t* new_cb = zjs_malloc(sizeof(struct zjs_callback_t));
        if (!new_cb) {
            DBG_PRINT("error allocating space for new callback\n");
            return -1;
        }
        memset(new_cb, 0, sizeof(struct zjs_callback_t));

        SET_ONCE(new_cb->flags, 0);
        SET_TYPE(new_cb->flags, CALLBACK_TYPE_JS);
        SET_JS_TYPE(new_cb->flags, JS_TYPE_LIST);
        new_cb->id = new_id();
        new_cb->this = jerry_acquire_value(this);
        new_cb->post = post;
        new_cb->handle = handle;
        new_cb->max_funcs = CB_LIST_MULTIPLIER;
        new_cb->num_funcs = 1;
        new_cb->func_list = zjs_malloc(sizeof(jerry_value_t) * CB_LIST_MULTIPLIER);
        if (!new_cb->func_list) {
            DBG_PRINT("could not allocate function list\n");
            return -1;
        }
        new_cb->func_list[0] = jerry_acquire_value(js_func);
        cb_map[new_cb->id] = new_cb;
        if (new_cb->id >= cb_size - 1) {
            cb_size++;
        }
        return new_cb->id;
    }
}

zjs_callback_id add_callback(jerry_value_t js_func,
                             jerry_value_t this,
                             void* handle,
                             zjs_post_callback_func post,
                             uint8_t once)
{
    struct zjs_callback_t* new_cb = zjs_malloc(sizeof(struct zjs_callback_t));
    if (!new_cb) {
        DBG_PRINT("error allocating space for new callback\n");
        return -1;
    }
    memset(new_cb, 0, sizeof(struct zjs_callback_t));

    SET_ONCE(new_cb->flags, (once) ? 1 : 0);
    SET_TYPE(new_cb->flags, CALLBACK_TYPE_JS);
    SET_JS_TYPE(new_cb->flags, JS_TYPE_SINGLE);
    new_cb->id = new_id();
    new_cb->js_func = jerry_acquire_value(js_func);
    new_cb->this = jerry_acquire_value(this);
    new_cb->post = post;
    new_cb->handle = handle;
    new_cb->max_funcs = 1;
    new_cb->num_funcs = 1;

    // Add callback to list
    cb_map[new_cb->id] = new_cb;
    if (new_cb->id >= cb_size - 1) {
        cb_size++;
    }

    DBG_PRINT("adding new callback id %d, js_func=%lu, once=%u\n",
              new_cb->id, new_cb->js_func, once);

    return new_cb->id;
}

zjs_callback_id zjs_add_callback(jerry_value_t js_func,
                                 jerry_value_t this,
                                 void* handle,
                                 zjs_post_callback_func post)
{
    return add_callback(js_func, this, handle, post, 0);
}

zjs_callback_id zjs_add_callback_once(jerry_value_t js_func,
                                      jerry_value_t this,
                                      void* handle,
                                      zjs_post_callback_func post)
{
    return add_callback(js_func, this, handle, post, 1);
}

void zjs_remove_callback(zjs_callback_id id)
{
    if (id != -1 && cb_map[id]) {
        if (GET_TYPE(cb_map[id]->flags) == CALLBACK_TYPE_JS) {
            if (GET_JS_TYPE(cb_map[id]->flags) == JS_TYPE_SINGLE) {
                jerry_release_value(cb_map[id]->js_func);
            } else if (GET_JS_TYPE(cb_map[id]->flags) == JS_TYPE_LIST &&
                       cb_map[id]->func_list) {
                int i;
                for (i = 0; i < cb_map[id]->num_funcs; ++i) {
                    jerry_release_value(cb_map[id]->func_list[i]);
                }
                zjs_free(cb_map[id]->func_list);
            }
            jerry_release_value(cb_map[id]->this);
        }
        zjs_free(cb_map[id]);
        cb_map[id] = NULL;
        DBG_PRINT("removing callback id %d\n", id);
    }
}

void zjs_remove_all_callbacks()
{
   for (int i = 0; i < cb_size; i++) {
        if (cb_map[i]) {
            zjs_remove_callback(i);
        }
    }
}

// INTERRUPT SAFE FUNCTION: No JerryScript VM, allocs, or release prints!
void zjs_signal_callback(zjs_callback_id id, const void *args, uint32_t size)
{
    DBG_PRINT("pushing item to ring buffer. id=%d, args=%p, size=%lu\n", id,
              args, size);

    if (GET_TYPE(cb_map[id]->flags) == CALLBACK_TYPE_JS) {
        // for JS, acquire values and release them after servicing callback
        int argc = size / sizeof(jerry_value_t);
        jerry_value_t *values = (jerry_value_t *)args;
        for (int i=0; i<argc; i++) {
            jerry_acquire_value(values[i]);
        }
    }
    int ret = zjs_port_ring_buf_put(&ring_buffer,
                                    (uint16_t)id,
                                    0,
                                    (uint32_t*)args,
                                    (uint8_t)((size + 3) / 4));
    if (ret != 0) {
        if (GET_TYPE(cb_map[id]->flags) == CALLBACK_TYPE_JS) {
            // for JS, acquire values and release them after servicing callback
            int argc = size / sizeof(jerry_value_t);
            jerry_value_t *values = (jerry_value_t *)args;
            for (int i=0; i<argc; i++) {
                jerry_release_value(values[i]);
            }
        }

        zjs_ringbuf_error_count++;
        zjs_ringbuf_last_error = ret;
    }
}

zjs_callback_id zjs_add_c_callback(void* handle, zjs_c_callback_func callback)
{
    struct zjs_callback_t* new_cb = zjs_malloc(sizeof(struct zjs_callback_t));
    if (!new_cb) {
        DBG_PRINT("error allocating space for new callback\n");
        return -1;
    }
    memset(new_cb, 0, sizeof(struct zjs_callback_t));

    SET_ONCE(new_cb->flags, 0);
    SET_TYPE(new_cb->flags, CALLBACK_TYPE_C);
    new_cb->id = new_id();
    new_cb->function = callback;
    new_cb->handle = handle;

    // Add callback to list
    cb_map[new_cb->id] = new_cb;
    if (new_cb->id >= cb_size - 1) {
        cb_size++;
    }
    DBG_PRINT("adding new C callback id %d\n", new_cb->id);

    return new_cb->id;
}

#ifdef DEBUG_BUILD
void print_callbacks(void)
{
    int i;
    for (i = 0; i < cb_size; i++) {
        if (cb_map[i]) {
            if (GET_TYPE(cb_map[i]->flags) == CALLBACK_TYPE_JS) {
                ZJS_PRINT("[%u] JS Callback:\n\tType: ", i);
                if (cb_map[i]->func_list == NULL &&
                    jerry_value_is_function(cb_map[i]->js_func)) {
                    ZJS_PRINT("Single Function\n");
                    ZJS_PRINT("\tjs_func: %lu\n", cb_map[i]->js_func);
                    ZJS_PRINT("\tonce: %u\n", GET_ONCE(cb_map[i]->flags));
                } else {
                    ZJS_PRINT("List\n");
                    ZJS_PRINT("\tmax_funcs: %u\n", cb_map[i]->max_funcs);
                    ZJS_PRINT("\tmax_funcs: %u\n", cb_map[i]->num_funcs);
                }
            }
        } else {
            ZJS_PRINT("[%u] Empty\n", i);
        }
    }
}
#else
#define print_callbacks() do {} while (0)
#endif

void zjs_call_callback(zjs_callback_id id, void* data, uint32_t sz)
{
    if (id <= cb_size && cb_map[id]) {
        if (GET_TYPE(cb_map[id]->flags) == CALLBACK_TYPE_JS) {
            // Function list callback
            int i;
            jerry_value_t *values = (jerry_value_t *)data;
            jerry_value_t ret_val;
            if (GET_JS_TYPE(cb_map[id]->flags) == JS_TYPE_SINGLE) {
                ret_val = jerry_call_function(cb_map[id]->js_func,
                                              cb_map[id]->this, values, sz);
                if (jerry_value_has_error_flag(ret_val)) {
                    zjs_print_error_message(ret_val);
                }
                jerry_release_value(ret_val);
            } else if (GET_JS_TYPE(cb_map[id]->flags) == JS_TYPE_LIST) {
                for (i = 0; i < cb_map[id]->num_funcs; ++i) {
                    ret_val = jerry_call_function(cb_map[id]->func_list[i],
                                                  cb_map[id]->this, values, sz);
                    if (jerry_value_has_error_flag(ret_val)) {
                        zjs_print_error_message(ret_val);
                    }
                    jerry_release_value(ret_val);
                }
            }

            // ensure the callback wasn't deleted by the previous calls
            if (cb_map[id]) {
                if (cb_map[id]->post) {
                    cb_map[id]->post(cb_map[id]->handle, &ret_val);
                }
                if (GET_ONCE(cb_map[id]->flags)) {
                    zjs_remove_callback(id);
                }
            }
        } else if (GET_TYPE(cb_map[id]->flags) == CALLBACK_TYPE_C &&
                   cb_map[id]->function) {
            cb_map[id]->function(cb_map[id]->handle, data);
        }
    } else {
        // NOTE: This can happen if a callback is removed while one is pending,
        //   and that's not really an error.
        // FIXME: But... new_id() could assign the same callback ID out again
        //   in the meantime, in which case we might call the new callback
        //   handler above, possibly disastrously expecting a different number
        //   of arguments. One solution might be to always increase new_id but
        //   map it to a slot id. (This would be a contender for my recent idea
        //   about storing key/value map type data within a JS object to keep
        //   from using malloc for that.)
        ERR_PRINT("callback does not exist: %d\n", id);
    }
}

uint8_t zjs_service_callbacks(void)
{
    if (zjs_ringbuf_error_count > 0) {
        ERR_PRINT("%d errors putting into ring buffer (last rval=%d)\n",
                  zjs_ringbuf_error_count, zjs_ringbuf_last_error);
        zjs_ringbuf_error_count = 0;
    }

    uint8_t serviced = 0;
    if (ring_buf_initialized) {
#ifdef ZJS_PRINT_CALLBACK_STATS
        uint8_t header_printed = 0;
        uint32_t num_callbacks = 0;
#endif
        uint16_t count = 0;
        while (count++ < ZJS_MAX_CB_LOOP_ITERATION) {
            int ret;
            uint16_t id;
            uint8_t value;
            uint8_t size = 0;

            // set size = 0 to check if there is an item in the ring buffer
            ret = zjs_port_ring_buf_get(&ring_buffer,
                                        &id,
                                        &value,
                                        NULL,
                                        &size);
            if (ret == -EMSGSIZE || ret == 0) {
                serviced = 1;
                // item in ring buffer with size > 0, has args
                // pull from ring buffer
                uint8_t sz = size;
                uint32_t data[sz];
                if (ret == -EMSGSIZE) {
                    ret = zjs_port_ring_buf_get(&ring_buffer,
                                                &id,
                                                &value,
                                                data,
                                                &sz);
                    if (ret != 0) {
                        ERR_PRINT("pulling from ring buffer: ret = %u\n", ret);
                        break;
                    }
                    DBG_PRINT("calling callback with args. id=%u, args=%p, sz=%u, ret=%i\n", id, data, sz, ret);
                    bool is_js = GET_TYPE(cb_map[id]->flags) == CALLBACK_TYPE_JS;
                    zjs_call_callback(id, data, sz);
                    if (is_js) {
                        for (int i = 0; i < sz; i++)
                            jerry_release_value(data[i]);
                    }
                } else if (ret == 0) {
                    // item in ring buffer with size == 0, no args
                    DBG_PRINT("calling callback with no args, original vals id=%u, size=%u, ret=%i\n", id, size, ret);
                    zjs_call_callback(id, NULL, 0);
                }
#ifdef ZJS_PRINT_CALLBACK_STATS
                if (!header_printed) {
                    PRINT("\n--------- Callback Stats ------------\n");
                    header_printed = 1;
                }
                if (cb_map[id]) {
                    PRINT("[cb stats] Callback[%u]: type=%s, arg_sz=%u\n",
                            id,
                            (cb_map[id]->type == CALLBACK_TYPE_JS) ? "JS" : "C",
                            size);
                }
                num_callbacks++;
#endif
            } else {
                // no more items in ring buffer
                break;
            }
        }
#ifdef ZJS_PRINT_CALLBACK_STATS
        if (num_callbacks) {
            PRINT("[cb stats] Number of Callbacks (this service): %lu\n",
                  num_callbacks);
            PRINT("[cb stats] Max Callbacks Per Service: %u\n",
                  ZJS_MAX_CB_LOOP_ITERATION);
            PRINT("------------- End ----------------\n");
        }
#endif
    }
    return serviced;
}
