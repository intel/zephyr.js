// Copyright (c) 2016-2018, Intel Corporation.

// enable for verbose callback detail
#if 0
#define DEBUG_CALLBACKS
#endif

// enable for verbose lock detail
#if 0
#define DEBUG_LOCKS
#endif

// C includes
#include <string.h>

#ifndef ZJS_LINUX_BUILD
// Zephyr includes
#include <misc/ring_buffer.h>
#include <zephyr.h>

// ZJS includes
#include "zjs_zephyr_port.h"
#else
#include "zjs_linux_port.h"
#endif

#include "zjs_callbacks.h"
#include "zjs_util.h"

// JerryScript includes
#include "jerryscript.h"

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
#define CB_LIST_MULTIPLIER     4

// flag bit value for JS callback
#define CALLBACK_TYPE_JS    0
// flag bit value for C callback
#define CALLBACK_TYPE_C     1

// Bits in flags for once, type (C or JS), and JS type (single or list)
#define ONCE_BIT       0
#define TYPE_BIT       1
#define CB_REMOVED_BIT 2
// Macros to set the bits in flags
#define SET_ONCE(f, b)     f |= (b << ONCE_BIT)
#define SET_TYPE(f, b)     f |= (b << TYPE_BIT)
#define SET_CB_REMOVED(f)  f |= (1 << CB_REMOVED_BIT)
// Macros to get the bits in flags
#define GET_ONCE(f)        (f & (1 << ONCE_BIT)) >> ONCE_BIT
#define GET_TYPE(f)        (f & (1 << TYPE_BIT)) >> TYPE_BIT
#define GET_CB_REMOVED(f)  (f & (1 << CB_REMOVED_BIT)) >> CB_REMOVED_BIT

// ring buffer values for flushing pending callbacks
#define CB_FLUSH_ONE 0xfe
#define CB_FLUSH_ALL 0xff

#define MAX_CALLER_CREATOR_LEN 128
typedef struct zjs_callback {
    void *handle;
    zjs_post_callback_func post;
    jerry_value_t this;
    union {
        jerry_value_t js_func;         // Single JS function callback
        zjs_c_callback_func function;  // C callback
    };
    zjs_callback_id id;
    u8_t flags;  // holds once and type bits
    u8_t max_funcs;
    u8_t num_funcs;
#ifdef INSTRUMENT_CALLBACKS
    char creator[MAX_CALLER_CREATOR_LEN];  // file that created this callback
    char caller[MAX_CALLER_CREATOR_LEN];   // file that signalled this callback
#endif
} zjs_callback_t;

#ifdef ZJS_LINUX_BUILD
static u8_t args_buffer[ZJS_CALLBACK_BUF_SIZE];
static struct zjs_port_ring_buf ring_buffer;
#else
SYS_RING_BUF_DECLARE_POW2(ring_buffer, 6);
#endif
static u8_t ring_buf_initialized = 1;

#ifdef ZJS_LINUX_BUILD
#define k_is_preempt_thread() 0
#define RB_LOCK() do {} while (0)
#define RB_UNLOCK() do {} while (0)
#define CB_LOCK() do {} while (0)
#define CB_UNLOCK() do {} while (0)
#else  // !ZJS_LINUX_BUILD
// mutex to ensure only one thread can access ring buffer at a time
static struct k_mutex ring_mutex;

#define RB_LOCK()                                \
{                                                \
    LPRINT("Ringbuf lock...");                   \
    if (k_mutex_lock(&ring_mutex, K_FOREVER)) {  \
        ERR_PRINT("Ringbuf lock error.\n");      \
    } else {                                     \
        LPRINT("Ringbuf locked.");               \
    }                                            \
}

#define RB_UNLOCK()               \
{                                 \
    LPRINT("Ringbuf unlock...");  \
    k_mutex_unlock(&ring_mutex);  \
    LPRINT("Ringbuf unlocked.");  \
}

// mutex to ensure only one thread can access cb_map at a time
static struct k_mutex cb_mutex;

#define CB_LOCK()                              \
{                                              \
    LPRINT("Callbacks lock...");               \
    if (k_mutex_lock(&cb_mutex, K_FOREVER)) {  \
        ERR_PRINT("Callback lock error.\n");   \
    } else {                                   \
        LPRINT("Callbacks locked.");           \
    }                                          \
}

#define CB_UNLOCK()                 \
{                                   \
    LPRINT("Callbacks unlock...");  \
    k_mutex_unlock(&cb_mutex);      \
    LPRINT("Callbacks unlocked.");  \
}
#endif  // ZJS_LINUX_BUILD

static zjs_callback_id cb_limit = INITIAL_CALLBACK_SIZE;
static zjs_callback_id cb_size = 0;
static zjs_callback_t **cb_map = NULL;

static zjs_callback_id defer_id = -1;

static int zjs_ringbuf_error_count = 0;
static int zjs_ringbuf_error_max = 0;
static int zjs_ringbuf_last_error = 0;

#ifdef INSTRUMENT_CALLBACKS
static void set_info_string(char *str, const char *file, const char *func)
{
    const char *i = file + strlen(file);
    while (i != file) {
        if (*i == '/') {
            i++;
            break;
        }
        i--;
    }
    snprintf(str, MAX_CALLER_CREATOR_LEN, "%s:%s()", i, func);
}
#endif

static zjs_callback_id new_id(void)
{
    zjs_callback_id id = 0;
    // NOTE: We could wait until after we check for a NULL callback slot in
    //   the map before we increase allocation; or else store a count that
    //   can be less than cb_size when callbacks are removed.
    CB_LOCK();
    if (cb_size >= cb_limit) {
        cb_limit += CB_CHUNK_SIZE;
        size_t size = sizeof(zjs_callback_t *) * cb_limit;
        zjs_callback_t **new_map = zjs_malloc(size);
        if (!new_map) {
            DBG_PRINT("error allocating space for new callback map\n");
            return -1;
        }
        DBG_PRINT("callback list size too small, increasing by %d\n",
                  CB_CHUNK_SIZE);
        memset(new_map, 0, size);
        memcpy(new_map, cb_map, sizeof(zjs_callback_t *) * cb_size);
        zjs_free(cb_map);
        cb_map = new_map;
    }
    while (cb_map[id] != NULL) {
        id++;
    }
    if (id >= cb_size) {
        cb_size = id + 1;
    }
    CB_UNLOCK();
    return id;
}

typedef struct deferred_work {
    zjs_deferred_work callback;
    u32_t length;  // length of user data
    char data[0];  // user data
} deferred_work_t;

static void deferred_work_callback(void *handle, const void *args)
{
    const deferred_work_t *deferred = (const deferred_work_t *)args;

#ifdef DEBUG_CALLBACKS
    int len = sizeof(deferred_work_t) + deferred->length;
    DBG_PRINT("process deferred work: %d bytes\ncontents: ", len);
    int count32 = (len + 3) / 4;
    for (int i = 0; i < count32; ++i) {
        ZJS_PRINT("0x%x ", ((u32_t *)args)[i]);
    }
    ZJS_PRINT("\n");
#endif

    if (deferred->callback) {
        deferred->callback(deferred->data, deferred->length);
        return;
    }
    DBG_PRINT("deferred work callback was NULL\n");
}

void zjs_init_callbacks(void)
{
#ifndef ZJS_LINUX_BUILD
    k_mutex_init(&ring_mutex);
    k_mutex_init(&cb_mutex);
#endif

    if (!cb_map) {
        size_t size = sizeof(zjs_callback_t *) * INITIAL_CALLBACK_SIZE;
        cb_map = (zjs_callback_t **)zjs_malloc(size);
        if (!cb_map) {
            DBG_PRINT("error allocating space for CB map\n");
            return;
        }
        memset(cb_map, 0, size);
    }
#ifdef ZJS_LINUX_BUILD
    zjs_port_ring_buf_init(&ring_buffer, ZJS_CALLBACK_BUF_SIZE,
                           (u32_t *)args_buffer);
#endif
    ring_buf_initialized = 1;

    defer_id = zjs_add_c_callback(NULL, deferred_work_callback);
    return;
}

bool zjs_edit_js_func(zjs_callback_id id, jerry_value_t func)
{
    // requires: only run from main thread
    CB_LOCK();
    bool rval = false;
    if (id >= 0 && cb_map[id]) {
        jerry_release_value(cb_map[id]->js_func);
        cb_map[id]->js_func = jerry_acquire_value(func);
        rval = true;
    }
    CB_UNLOCK();
    return rval;
}

zjs_callback_id add_callback_priv(jerry_value_t js_func,
                                  jerry_value_t this,
                                  void *handle,
                                  zjs_post_callback_func post,
                                  u8_t once
#ifdef INSTRUMENT_CALLBACKS
                                  ,
                                  const char *file,
                                  const char *func)
#else
                                  )
#endif
{
    zjs_callback_t *new_cb = zjs_malloc(sizeof(zjs_callback_t));
    if (!new_cb) {
        DBG_PRINT("error allocating space for new callback\n");
        return -1;
    }
    memset(new_cb, 0, sizeof(zjs_callback_t));

    SET_ONCE(new_cb->flags, (once) ? 1 : 0);
    SET_TYPE(new_cb->flags, CALLBACK_TYPE_JS);
    new_cb->id = new_id();
    new_cb->js_func = jerry_acquire_value(js_func);
    new_cb->this = jerry_acquire_value(this);
    new_cb->post = post;
    new_cb->handle = handle;
    new_cb->max_funcs = 1;
    new_cb->num_funcs = 1;

    CB_LOCK();
    // Add callback to list
    cb_map[new_cb->id] = new_cb;

    DBG_PRINT("adding new callback id %d, js_func=%p, once=%u\n", new_cb->id,
              (void *)(uintptr_t)new_cb->js_func, once);

#ifdef INSTRUMENT_CALLBACKS
    set_info_string(cb_map[new_cb->id]->creator, file, func);
#endif
    CB_UNLOCK();
    return new_cb->id;
}

static void zjs_free_callback(zjs_callback_id id)
{
    // effects: frees callback associated with id if it's marked as removed
    CB_LOCK();
    if (id >= 0 && cb_map[id] && GET_CB_REMOVED(cb_map[id]->flags)) {
        zjs_free(cb_map[id]);
        cb_map[id] = NULL;
    }
    CB_UNLOCK();
}

static void zjs_remove_callback_priv(zjs_callback_id id, bool skip_flush)
{
    // effects: removes the callback associated with id; if skip_flush is true,
    //            assumes the callback will be "flushed" elsewhere, that is
    //            freed and the id reclaimed; otherwise, tries to do it here
    CB_LOCK();
    if (id >= 0 && cb_map[id]) {
        // Don't free a callback after its been freed
        if (GET_CB_REMOVED(cb_map[id]->flags)) {
            CB_UNLOCK();
            return;
        }

        if (GET_TYPE(cb_map[id]->flags) == CALLBACK_TYPE_JS) {
            jerry_release_value(cb_map[id]->js_func);
            jerry_release_value(cb_map[id]->this);
        }
        SET_CB_REMOVED(cb_map[id]->flags);
        CB_UNLOCK();
        if (!skip_flush) {
            RB_LOCK();
            int ret = zjs_port_ring_buf_put(&ring_buffer, (u16_t)id,
                                            CB_FLUSH_ONE, NULL, 0);
            RB_UNLOCK();
            if (ret) {
                // couldn't add flush command, so just free now
                DBG_PRINT("no room for flush callback %d command\n", id);
                zjs_free_callback(id);
            }
        }
        DBG_PRINT("removing callback id %d\n", id);
    } else {
        CB_UNLOCK();
    }
}

void zjs_remove_callback(zjs_callback_id id)
{
    zjs_remove_callback_priv(id, false);
}

void zjs_remove_all_callbacks()
{
    // try posting a command to flush all removed callbacks
    RB_LOCK();
    int ret = zjs_port_ring_buf_put(&ring_buffer, 0, CB_FLUSH_ALL, NULL, 0);
    RB_UNLOCK();
    bool skip_flush = ret ? false : true;
    for (int i = 0; i < cb_size; i++) {
        CB_LOCK();
        if (cb_map[i]) {
            zjs_remove_callback_priv(i, skip_flush);
        }
        CB_UNLOCK();
    }
}

// INTERRUPT SAFE FUNCTION: No JerryScript VM, allocs, or release prints!
void signal_callback_priv(zjs_callback_id id,
                          const void *args,
                          u32_t size
#ifdef INSTRUMENT_CALLBACKS
                          ,
                          const char *file,
                          const char *func)
#else
                          )
#endif
{
#ifdef DEBUG_CALLBACKS
    DBG_PRINT("pushing item to ring buffer. id=%d, args=%p, size=%u\n", id,
              args, size);
#endif
    int in_thread = k_is_preempt_thread();  // versus ISR or co-op thread
#ifndef ZJS_LINUX_BUILD
    int key = 0;
#endif
    if (in_thread) CB_LOCK();
    if (id < 0 || id >= cb_size || !cb_map[id]) {
        DBG_PRINT("callback ID %d does not exist\n", id);
        if (in_thread) CB_UNLOCK();
        return;
    }
    if (GET_CB_REMOVED(cb_map[id]->flags)) {
        DBG_PRINT("callback already removed\n");
        if (in_thread) CB_UNLOCK();
        return;
    }
    if (GET_TYPE(cb_map[id]->flags) == CALLBACK_TYPE_JS) {
        // for JS, acquire values and release them after servicing callback
        int argc = size / sizeof(jerry_value_t);
        jerry_value_t *values = (jerry_value_t *)args;
        for (int i = 0; i < argc; i++) {
            jerry_acquire_value(values[i]);
        }
    }
#ifdef INSTRUMENT_CALLBACKS
    set_info_string(cb_map[id]->caller, file, func);
#endif
#ifndef ZJS_LINUX_BUILD
    if (in_thread) {
        RB_LOCK();
        key = irq_lock();
    }
#endif
    int ret = zjs_port_ring_buf_put(&ring_buffer,
                                    (u16_t)id,
                                    0,  // we use value for CB_FLUSH_ONE/ALL
                                    (u32_t *)args,
                                    (u8_t)((size + 3) / 4));
    // Need to unlock here or callback may be blocked when it gets called.
    // NOTE: this is a temporary fix, we should implement a lock ID system
    // rather than locking everything, as we are only trying to prevent a
    // callback
    // from being edited and called at the same time.
#ifndef ZJS_LINUX_BUILD
    if (in_thread) {
        irq_unlock(key);
        RB_UNLOCK();
    }
    zjs_loop_unblock();
#endif
    if (ret != 0) {
        if (GET_TYPE(cb_map[id]->flags) == CALLBACK_TYPE_JS) {
            // for JS, acquire values and release them after servicing callback
            int argc = size / sizeof(jerry_value_t);
            jerry_value_t *values = (jerry_value_t *)args;
            for (int i = 0; i < argc; i++) {
                jerry_release_value(values[i]);
            }
        }

        zjs_ringbuf_error_count++;
        zjs_ringbuf_last_error = ret;
    }
    if (in_thread) CB_UNLOCK();
}

zjs_callback_id zjs_add_c_callback(void *handle, zjs_c_callback_func callback)
{
    zjs_callback_t *new_cb = zjs_malloc(sizeof(zjs_callback_t));
    if (!new_cb) {
        DBG_PRINT("error allocating space for new callback\n");
        return -1;
    }
    memset(new_cb, 0, sizeof(zjs_callback_t));

    SET_ONCE(new_cb->flags, 0);
    SET_TYPE(new_cb->flags, CALLBACK_TYPE_C);
    CB_LOCK();
    new_cb->id = new_id();
    new_cb->function = callback;
    new_cb->handle = handle;

    // Add callback to list
    cb_map[new_cb->id] = new_cb;

    DBG_PRINT("adding new C callback id %d\n", new_cb->id);
    CB_UNLOCK();
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
                if (jerry_value_is_function(cb_map[i]->js_func)) {
                    ZJS_PRINT("Single Function\n");
                    ZJS_PRINT("\tjs_func: %p\n",
                              (void *)(uintptr_t)cb_map[i]->js_func);
                    ZJS_PRINT("\tonce: %u\n", GET_ONCE(cb_map[i]->flags));
                } else {
                    ZJS_PRINT("js_func is not a function\n");
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

void zjs_call_callback(zjs_callback_id id, const void *data, u32_t sz)
{
    CB_LOCK();
    if (id == -1 || id >= cb_size || !cb_map[id]) {
        ERR_PRINT("callback %d does not exist\n", id);
    } else if (GET_CB_REMOVED(cb_map[id]->flags)) {
        DBG_PRINT("callback %d has already been removed\n", id);
    } else {
        if (GET_TYPE(cb_map[id]->flags) == CALLBACK_TYPE_JS) {
            jerry_value_t *values = (jerry_value_t *)data;
            ZVAL_MUTABLE rval;
            if (!jerry_value_is_undefined(cb_map[id]->js_func)) {
                rval = jerry_call_function(cb_map[id]->js_func,
                                           cb_map[id]->this, values, sz);
                if (jerry_value_has_error_flag(rval)) {
#ifdef INSTRUMENT_CALLBACKS
                    DBG_PRINT("callback %d had error; creator: %s, "
                              "caller: %s\n",
                              id, cb_map[id]->creator, cb_map[id]->caller);
#endif
                    zjs_print_error_message(rval, cb_map[id]->js_func);
                }
            }

            // ensure the callback wasn't deleted by the previous calls
            if (cb_map[id]) {
                if (cb_map[id]->post) {
                    cb_map[id]->post(cb_map[id]->handle, rval);
                }
                if (GET_ONCE(cb_map[id]->flags)) {
                    zjs_remove_callback_priv(id, false);
                }
            }
        } else if (GET_TYPE(cb_map[id]->flags) == CALLBACK_TYPE_C &&
                   cb_map[id]->function) {
            cb_map[id]->function(cb_map[id]->handle, data);
        }
    }
    CB_UNLOCK();
}

u8_t zjs_service_callbacks(void)
{
    if (zjs_ringbuf_error_count > zjs_ringbuf_error_max) {
        ERR_PRINT("%d ringbuf put errors (last rval=%d)\n",
                  zjs_ringbuf_error_count, zjs_ringbuf_last_error);
        zjs_ringbuf_error_max = zjs_ringbuf_error_count * 2;
        zjs_ringbuf_error_count = 0;
    }

    u8_t serviced = 0;
    if (ring_buf_initialized) {
#ifdef ZJS_PRINT_CALLBACK_STATS
        u8_t header_printed = 0;
        u32_t num_callbacks = 0;
#endif
        u16_t count = 0;
        while (count++ < ZJS_MAX_CB_LOOP_ITERATION) {
            int ret;
            u16_t id;
            u8_t value;
            u8_t size = 0;
            // set size = 0 to check if there is an item in the ring buffer
            RB_LOCK();
            ret = zjs_port_ring_buf_get(&ring_buffer, &id, &value, NULL, &size);
            RB_UNLOCK();
            if (ret == -EMSGSIZE || ret == 0) {
                serviced = 1;
                // item in ring buffer with size > 0, has args
                // pull from ring buffer
                u8_t sz = size;
                u32_t data[sz];
                if (ret == -EMSGSIZE) {
                    RB_LOCK();
                    ret = zjs_port_ring_buf_get(&ring_buffer, &id, &value, data,
                                                &sz);
                    RB_UNLOCK();
                    if (ret != 0) {
                        ERR_PRINT("pulling from ring buffer: ret = %u\n", ret);
                        break;
                    }
#ifdef DEBUG_CALLBACKS
                    DBG_PRINT("calling callback with args. id=%u, args=%p, "
                              "sz=%u, ret=%i\n", id, data, sz, ret);
#endif
                    bool is_js = GET_TYPE(cb_map[id]->flags) ==
                        CALLBACK_TYPE_JS;
                    zjs_call_callback(id, data, sz);
                    if (is_js) {
                        for (int i = 0; i < sz; i++)
                            jerry_release_value((jerry_value_t)data[i]);
                    }
                } else if (ret == 0) {
                    // check for flush commands
                    switch (value) {
                    case CB_FLUSH_ONE:
                        DBG_PRINT("flushed callback %d, freeing\n", id);
                        zjs_free_callback(id);
                        break;

                    case CB_FLUSH_ALL:
                        DBG_PRINT("flushed all callbacks, freeing\n");
                        for (int i = 0; i < cb_size; i++)
                            zjs_free_callback(i);
                        break;

                    default:
                        // item in ring buffer with size == 0, no args
#ifdef DEBUG_CALLBACKS
                        DBG_PRINT("calling callback with no args, "
                                  "original vals id=%u, size=%u, ret=%i\n",
                                  id, size, ret);
#endif
                        zjs_call_callback(id, NULL, 0);
                    }
                }
#ifdef ZJS_PRINT_CALLBACK_STATS
                if (!header_printed) {
                    ZJS_PRINT("\n--------- Callback Stats ------------\n");
                    header_printed = 1;
                }
                if (cb_map[id]) {
                    ZJS_PRINT("[cb stats] Callback[%u]: type=%s, arg_sz=%u\n", id,
                              (GET_TYPE(cb_map[id]->flags) == CALLBACK_TYPE_JS) ? "JS" : "C",
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
            ZJS_PRINT("[cb stats] Number of Callbacks (this service): %u\n",
                      num_callbacks);
            ZJS_PRINT("[cb stats] Max Callbacks Per Service: %u\n",
                      ZJS_MAX_CB_LOOP_ITERATION);
            ZJS_PRINT("------------- End ----------------\n");
        }
#endif
    }
    return serviced;
}

void zjs_defer_work(zjs_deferred_work callback, const void *buffer, u32_t bytes)
{
    DBG_PRINT("deferring work: %d bytes\n", bytes);
    int len = sizeof(deferred_work_t) + bytes;
    char buf[len];
    deferred_work_t *defer = (deferred_work_t *)buf;
    defer->callback = callback;
    defer->length = bytes;
    if (buffer && bytes) {
        memcpy(defer->data, buffer, bytes);
    }
#ifdef DEBUG_CALLBACKS
    DBG_PRINT("full packet: %d bytes\ncontents: ", len);
    int count32 = (len + 3) / 4;
    for (int i = 0; i < count32; ++i) {
        ZJS_PRINT("0x%08x ", ((u32_t *)buf)[i]);
    }
    ZJS_PRINT("\nbuffer: ");
    count32 = (bytes + 3) / 4;
    for (int i = 0; i < count32; ++i) {
        ZJS_PRINT("0x%08x ", ((u32_t *)buffer)[i]);
    }
    ZJS_PRINT("\n");
#endif
    // assert: if buffer is null, bytes should be 0, and vice versa
    zjs_signal_callback(defer_id, buf, len);
}
