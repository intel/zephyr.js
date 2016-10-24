// Copyright (c) 2016, Intel Corporation.

#ifndef ZJS_LINUX_BUILD
#include <zephyr.h>
#include <misc/ring_buffer.h>
#include "zjs_zephyr_queue.h"
#include "zjs_zephyr_time.h"
#else
#include "zjs_linux_time.h"
#include "zjs_linux_queue.h"
#endif
#include <string.h>

#include "zjs_util.h"
#include "zjs_callbacks.h"

#include "jerry-api.h"

#define INITIAL_CALLBACK_SIZE  16
#define CB_CHUNK_SIZE          16

#define CALLBACK_TYPE_JS    0
#define CALLBACK_TYPE_C     1

#define CB_LIST_MULTIPLIER  4

struct zjs_callback_t {
    void* handle;
    zjs_pre_callback_func pre;
    zjs_post_callback_func post;
    jerry_value_t js_func;
    jerry_value_t this;
    uint8_t once;
    int max_funcs;
    int num_funcs;
    jerry_value_t* func_list;
};

struct zjs_c_callback_t {
    void* handle;
    zjs_c_callback_func function;
};

struct zjs_callback_map {
    void* reserved;
    int32_t id;
    uint8_t type;
    uint32_t arg_sz;
    uint8_t ring_buffer;
    union {
        struct zjs_callback_t* js;
        struct zjs_c_callback_t* c;
    };
};

static uint8_t args_buffer[1024];
static struct zjs_port_ring_buf ring_buffer;
static uint8_t ring_buf_initialized = 0;

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
            DBG_PRINT("error allocating space for new callback map\n");
            return -1;
        }
        DBG_PRINT("callback list size too small, increasing by %d\n",
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
            DBG_PRINT("error allocating space for CB map\n");
            return;
        }
        memset(cb_map, 0, size);
    }
    zjs_port_ring_buf_init(&ring_buffer, 1024, (uint32_t*)args_buffer);
    ring_buf_initialized = 1;
    return;
}

void zjs_edit_js_func(int32_t id, jerry_value_t func)
{
    if (id != -1) {
        jerry_release_value(cb_map[id]->js->js_func);
        cb_map[id]->js->js_func = jerry_acquire_value(func);
    }
}

void zjs_edit_callback_handle(int32_t id, void* handle)
{
    if (id != -1) {
        if (cb_map[id]->type == CALLBACK_TYPE_JS) {
            if (cb_map[id]->js) {
                cb_map[id]->js->handle = handle;
            }
        } else {
            cb_map[id]->c->handle = handle;
        }
    }
}

bool zjs_remove_callback_list_func(int32_t id, jerry_value_t js_func)
{
    if (id != -1 && cb_map[id] && cb_map[id]->js) {
        int i;
        for (i = 0; i < cb_map[id]->js->num_funcs; ++i) {
            if (js_func == cb_map[id]->js->func_list[i]) {
                int j;
                jerry_release_value(cb_map[id]->js->func_list[i]);
                for (j = i; j < cb_map[id]->js->num_funcs - 1; ++j) {
                    cb_map[id]->js->func_list[j] = cb_map[id]->js->func_list[j + 1];
                }
                cb_map[id]->js->num_funcs--;
                cb_map[id]->js->func_list[cb_map[id]->js->num_funcs] = 0;
                return true;
            }
        }
    }
    return false;
}

int zjs_get_num_callbacks(int32_t id)
{
    if (id != -1) {
        if (cb_map[id] && cb_map[id]->js) {
            return cb_map[id]->js->num_funcs;
        }
    }
    return 0;
}

jerry_value_t* zjs_get_callback_func_list(int32_t id, int* count)
{
    if (id != -1) {
        if (cb_map[id] && cb_map[id]->js) {
            *count = cb_map[id]->js->num_funcs;
            return cb_map[id]->js->func_list;
        }
    }
    return NULL;
}

int32_t zjs_add_callback_list(jerry_value_t js_func,
                              jerry_value_t this,
                              void* handle,
                              zjs_pre_callback_func pre,
                              zjs_post_callback_func post,
                              int32_t id)
{
    if (id != -1) {
        if (cb_map[id] && cb_map[id]->js && cb_map[id]->js->func_list) {
            // The function list is full, allocate more space, copy the existing
            // list, and add the new function
            if (cb_map[id]->js->num_funcs == cb_map[id]->js->max_funcs - 1) {
                int i;
                jerry_value_t* new_list = zjs_malloc((sizeof(jerry_value_t) *
                        (cb_map[id]->js->max_funcs + CB_LIST_MULTIPLIER)));
                for (i = 0; i < cb_map[id]->js->num_funcs; ++i) {
                    new_list[i] = cb_map[id]->js->func_list[i];
                }
                new_list[cb_map[id]->js->num_funcs] = jerry_acquire_value(js_func);

                cb_map[id]->js->max_funcs += CB_LIST_MULTIPLIER;
                zjs_free(cb_map[id]->js->func_list);
                cb_map[id]->js->func_list = new_list;
            } else {
                // Add function to list
                cb_map[id]->js->func_list[cb_map[id]->js->num_funcs] =
                        jerry_acquire_value(js_func);
            }
            // If not already set, set the handle/pre/post provided. These will
            // only be set once, when the list is created.
            if (!cb_map[id]->js->handle) {
                cb_map[id]->js->handle = handle;
            }
            if (!cb_map[id]->js->pre) {
                cb_map[id]->js->pre = pre;
            }
            if (!cb_map[id]->js->post) {
                cb_map[id]->js->post = post;
            }
            cb_map[id]->js->num_funcs++;
            return cb_map[id]->id;
        } else {
            DBG_PRINT("list handle was NULL\n");
            return -1;
        }
    } else {
        struct zjs_callback_map* new_cb = zjs_malloc(sizeof(struct zjs_callback_map));
        if (!new_cb) {
            DBG_PRINT("error allocating space for new callback\n");
            return -1;
        }
        new_cb->js = zjs_malloc(sizeof(struct zjs_callback_t));
        if (!new_cb->js) {
            DBG_PRINT("error allocating space for new callback\n");
            zjs_free(new_cb);
            return -1;
        }
        new_cb->type = CALLBACK_TYPE_JS;
        new_cb->id = new_id();
        new_cb->arg_sz = 0;
        new_cb->js->this = jerry_acquire_value(this);
        new_cb->js->pre = pre;
        new_cb->js->post = post;
        new_cb->js->handle = handle;
        new_cb->js->max_funcs = CB_LIST_MULTIPLIER;
        new_cb->js->num_funcs = 1;
        new_cb->js->func_list = zjs_malloc(sizeof(jerry_value_t) * CB_LIST_MULTIPLIER);
        if (!new_cb->js->func_list) {
            DBG_PRINT("could not allocate function list\n");
            return -1;
        }
        new_cb->js->func_list[0] = jerry_acquire_value(js_func);
        cb_map[new_cb->id] = new_cb;
        if (new_cb->id >= cb_size - 1) {
            cb_size++;
        }
        return new_cb->id;
    }
}

int32_t add_callback(jerry_value_t js_func,
                     jerry_value_t this,
                     void* handle,
                     zjs_pre_callback_func pre,
                     zjs_post_callback_func post,
                     uint8_t once)
{
    struct zjs_callback_map* new_cb = zjs_malloc(sizeof(struct zjs_callback_map));
    if (!new_cb) {
        DBG_PRINT("error allocating space for new callback\n");
        return -1;
    }
    new_cb->js = zjs_malloc(sizeof(struct zjs_callback_t));
    if (!new_cb->js) {
        DBG_PRINT("error allocating space for new callback\n");
        zjs_free(new_cb);
        return -1;
    }
    new_cb->type = CALLBACK_TYPE_JS;
    new_cb->id = new_id();
    new_cb->arg_sz = 0;
    new_cb->js->js_func = jerry_acquire_value(js_func);
    new_cb->js->this = jerry_acquire_value(this);
    new_cb->js->pre = pre;
    new_cb->js->post = post;
    new_cb->js->handle = handle;
    new_cb->js->func_list = NULL;
    new_cb->js->max_funcs = 0;
    new_cb->js->num_funcs = 0;
    new_cb->js->once = once;

    // Add callback to list
    cb_map[new_cb->id] = new_cb;
    if (new_cb->id >= cb_size - 1) {
        cb_size++;
    }
    DBG_PRINT("adding new callback id %ld, js_func=%lu, once=%u\n",
              new_cb->id, new_cb->js->js_func, once);

    return new_cb->id;
}

int32_t zjs_add_callback(jerry_value_t js_func,
                         jerry_value_t this,
                         void* handle,
                         zjs_pre_callback_func pre,
                         zjs_post_callback_func post)
{
    return add_callback(js_func, this, handle, pre, post, 0);
}

int32_t zjs_add_callback_once(jerry_value_t js_func,
                              jerry_value_t this,
                              void* handle,
                              zjs_pre_callback_func pre,
                              zjs_post_callback_func post)
{
    return add_callback(js_func, this, handle, pre, post, 1);
}

void zjs_remove_callback(int32_t id)
{
    if (id != -1 && cb_map[id]) {
        if (cb_map[id]->type == CALLBACK_TYPE_JS && cb_map[id]->js) {
            if (cb_map[id]->js->func_list) {
                int i;
                for (i = 0; i < cb_map[id]->js->num_funcs; ++i) {
                    jerry_release_value(cb_map[id]->js->func_list[i]);
                }
                zjs_free(cb_map[id]->js->func_list);
            } else {
                jerry_release_value(cb_map[id]->js->js_func);
            }
            jerry_release_value(cb_map[id]->js->this);
            zjs_free(cb_map[id]->js);
        } else if (cb_map[id]->c) {
            zjs_free(cb_map[id]->c);
        }
        zjs_free(cb_map[id]);
        cb_map[id] = NULL;
        DBG_PRINT("removing callback id %ld\n", id);
    }
}

void zjs_signal_callback_args(int32_t id, void* args, uint32_t size)
{
    if (args && size) {
        // signal with args
        PRINT("Putting callback %ld into ring buffer, args=%p, size=%lu\n", id, args, size);
        int ret = zjs_port_ring_buf_put(&ring_buffer, (uint16_t)id, 0, (uint32_t*)args, (uint8_t)size / 4);
        if (ret != 0) {
            DBG_PRINT("error putting into ring buffer, ret=%u\n", ret);
        }
    } else {
        // signal without args
        PRINT("Putting callback %ld into ring buffer (no args), args=%p, size=%lu\n", id, args, size);
        uint32_t dummy = 0;
        int ret = zjs_port_ring_buf_put(&ring_buffer, (uint16_t)id, 0, (uint32_t*)&dummy, (uint8_t)1);
        if (ret != 0) {
            DBG_PRINT("error putting into ring buffer, ret=%u\n", ret);
        }
    }

    cb_map[id]->arg_sz = size;
    cb_map[id]->ring_buffer = 1;
}

void zjs_signal_callback(int32_t id)
{
    if (id != -1 && cb_map[id]) {
#ifdef DEBUG_BUILD
        if (cb_map[id]->type == CALLBACK_TYPE_JS) {
            DBG_PRINT("signaling JS callback id %ld\n", id);
        } else {
            DBG_PRINT("signaling C callback id %ld\n", id);
        }
#endif
        zjs_signal_callback_args(id, NULL, 0);
    }
}

int32_t zjs_add_c_callback(void* handle, zjs_c_callback_func callback)
{
    struct zjs_callback_map* new_cb = zjs_malloc(sizeof(struct zjs_callback_map));
    if (!new_cb) {
        DBG_PRINT("error allocating space for new callback\n");
        return -1;
    }
    new_cb->c = zjs_malloc(sizeof(struct zjs_c_callback_t));
    if (!new_cb->c) {
        DBG_PRINT("error allocating space for new callback\n");
        zjs_free(new_cb);
        return -1;
    }
    new_cb->type = CALLBACK_TYPE_C;
    new_cb->id = new_id();
    new_cb->c->function = callback;
    new_cb->c->handle = handle;

    // Add callback to list
    cb_map[new_cb->id] = new_cb;
    if (new_cb->id >= cb_size - 1) {
        cb_size++;
    }
    DBG_PRINT("adding new C callback id %ld\n", new_cb->id);

    return new_cb->id;
}

#ifdef DEBUG_BUILD
void print_callbacks(void)
{
    int i;
    for (i = 0; i < cb_size; i++) {
        if (cb_map[i]) {
            if (cb_map[i]->type == CALLBACK_TYPE_JS) {
                PRINT("[%u] JS Callback:\n\tType: ", i);
                if (cb_map[i]->js->func_list == NULL &&
                    jerry_value_is_function(cb_map[i]->js->js_func)) {
                    PRINT("Single Function\n");
                    PRINT("\tjs_func: %lu\n", cb_map[i]->js->js_func);
                    PRINT("\tonce: %u\n", cb_map[i]->js->once);
                } else {
                    PRINT("List\n");
                    PRINT("\tmax_funcs: %u\n", cb_map[i]->js->max_funcs);
                    PRINT("\tmax_funcs: %u\n", cb_map[i]->js->num_funcs);
                }
            }
        } else {
            PRINT("[%u] Empty\n", i);
        }
    }
}
#else
#define print_callbacks() do {} while (0)
#endif

#if 0
void zjs_call_callback(int32_t i)
{
    if (cb_map[i]) {
        if (cb_map[i]->type == CALLBACK_TYPE_JS) {
            if (cb_map[i]->js->func_list == NULL && jerry_value_is_function(cb_map[i]->js->js_func)) {
                if (cb_map[i]->ring_buffer) {
                    uint16_t type;
                    uint8_t value;
                    uint8_t size = (cb_map[i]->arg_sz == 0) ? 1 : (cb_map[i]->arg_sz / 4);
                    jerry_value_t data[size];

                    int ret = zjs_port_ring_buf_get(&ring_buffer,
                                                    &type,
                                                    &value,
                                                    (uint32_t*)data,
                                                    &size);
                    if (ret != 0) {
                        DBG_PRINT("(JS) error pulling from ring buffer: ret = %u\n", ret);
                    }
                    if (type != i) {
                        DBG_PRINT("(JS) type != callback ID : type=%u, id=%ld\n", type, i);
                    }
                    if (cb_map[i]->arg_sz) {
                        jerry_call_function(cb_map[i]->js->js_func, cb_map[i]->js->this, data, cb_map[i]->arg_sz / 4);
                    } else {
                        jerry_call_function(cb_map[i]->js->js_func, cb_map[i]->js->this, NULL, 0);
                    }
                } else {
                    // JS callback with pre/post callback args
                    uint32_t argc = 0;
                    jerry_value_t ret_val;
                    jerry_value_t* args = NULL;

                    if (cb_map[i]->js->pre) {
                        args = cb_map[i]->js->pre(cb_map[i]->js->handle, &argc);
                    }

                    DBG_PRINT("calling callback id %ld with %lu args\n", cb_map[i]->id, argc);
                    // TODO: Use 'this' in callback module
                    jerry_call_function(cb_map[i]->js->js_func, cb_map[i]->js->this, args, argc);
                    if (cb_map[i]->js->post) {
                        cb_map[i]->js->post(cb_map[i]->js->handle, &ret_val);
                    }
                }
                if (cb_map[i]->js->once) {
                    zjs_remove_callback(i);
                }
            } else {
                int j;
                uint32_t argc = 0;
                jerry_value_t ret_val;
                jerry_value_t* args = NULL;

                if (cb_map[i]->js->pre) {
                    args = cb_map[i]->js->pre(cb_map[i]->js->handle, &argc);
                }

                DBG_PRINT("calling callback list id %ld with %lu args\n", cb_map[i]->id, argc);

                for (j = 0; j < cb_map[i]->js->num_funcs; ++j) {
                    ret_val = jerry_call_function(cb_map[i]->js->func_list[j], cb_map[i]->js->this, args, argc);
                    if (jerry_value_has_error_flag(ret_val)) {
                        DBG_PRINT("callback %ld returned an error for function[%i]\n", i, j);
                    }
                }
                if (cb_map[i]->js->post) {
                    cb_map[i]->js->post(cb_map[i]->js->handle, &ret_val);
                }
            }
        } else if (cb_map[i]->type == CALLBACK_TYPE_C && cb_map[i]->c->function) {
            if (cb_map[i]->ring_buffer) {
                uint16_t type;
                uint8_t value;
                uint8_t size = (cb_map[i]->arg_sz == 0) ? 1 : (cb_map[i]->arg_sz / 4);
                jerry_value_t data[size];

                int ret = zjs_port_ring_buf_get(&ring_buffer,
                                                &type,
                                                &value,
                                                (uint32_t*)data,
                                                &size);
                if (ret != 0) {
                    DBG_PRINT("(JS) error pulling from ring buffer: ret = %u\n", ret);
                }
                if (type != i) {
                    DBG_PRINT("(JS) type != callback ID : type=%u, id=%ld\n", type, i);
                }
                if (cb_map[i]->arg_sz) {
                    cb_map[i]->c->function(cb_map[i]->c->handle, data);
                } else {
                    cb_map[i]->c->function(cb_map[i]->c->handle, NULL);
                }
            } else {
                DBG_PRINT("calling callback id %ld, NULL args\n", cb_map[i]->id);
                cb_map[i]->c->function(cb_map[i]->c->handle, NULL);
            }
        }
    }
}
#endif

void zjs_call_callback(int32_t id, void* data, uint32_t sz)
{
    if(cb_map[id]) {
        if (cb_map[id]->type == CALLBACK_TYPE_JS) {
            if (cb_map[id]->js->func_list == NULL && jerry_value_is_function(cb_map[id]->js->js_func)) {
                jerry_value_t ret_val;
                // Single function callback
                if (sz) {
                    ret_val = jerry_call_function(cb_map[id]->js->js_func, cb_map[id]->js->this, data, sz);
                } else {
                    ret_val = jerry_call_function(cb_map[id]->js->js_func, cb_map[id]->js->this, NULL, 0);
                }
                if (cb_map[id]->js->post) {
                    cb_map[id]->js->post(cb_map[id]->js->handle, &ret_val);
                }
                if (cb_map[id]->js->once) {
                    zjs_remove_callback(id);
                }
            } else {
                // Function list callback
                int i;
                jerry_value_t ret_val;

                for (i = 0; i < cb_map[id]->js->num_funcs; ++i) {
                    if (sz) {
                        ret_val = jerry_call_function(cb_map[id]->js->func_list[i], cb_map[id]->js->this, data, sz);
                    } else {
                        ret_val = jerry_call_function(cb_map[id]->js->js_func, cb_map[id]->js->this, NULL, 0);
                    }
                    if (jerry_value_has_error_flag(ret_val)) {
                        DBG_PRINT("callback %d returned an error for function[%i]\n", id, i);
                    }
                }
                if (cb_map[id]->js->post) {
                    cb_map[id]->js->post(cb_map[id]->js->handle, &ret_val);
                }
            }
        } else if (cb_map[id]->type == CALLBACK_TYPE_C && cb_map[id]->c->function) {
            if (sz) {
                cb_map[id]->c->function(cb_map[id]->c->handle, data);
            } else {
                cb_map[id]->c->function(cb_map[id]->c->handle, NULL);
            }
        }
    } else {
        PRINT("cb_map[%ld]=%p\n", id, cb_map[id]);
    }
}

void zjs_service_callbacks(void)
{
    if (ring_buf_initialized) {
        uint16_t type;
        uint8_t value;
        uint8_t size = 0;
        uint32_t dummy;
        while (zjs_port_ring_buf_get(&ring_buffer,
                                     &type,
                                     &value,
                                     &dummy,
                                     &size) == -EMSGSIZE) {
            if (size == 0) {
                break;
            } else {
                // Pull from ring buffer
                uint16_t id;
                uint8_t value1;
                uint8_t sz = size;
                jerry_value_t data[sz];

                int ret = zjs_port_ring_buf_get(&ring_buffer,
                                                &id,
                                                &value1,
                                                (uint32_t*)data,
                                                &sz);
                if (ret != 0) {
                    DBG_PRINT("(JS) error pulling from ring buffer: ret = %u\n", ret);
                    break;
                }
                PRINT("Calling callback, id=%d, sz=%u\n", id, sz);
                zjs_call_callback(id, data, sz);
#if 0
                if(cb_map[id]) {
                    if (cb_map[id]->type == CALLBACK_TYPE_JS) {
                        if (cb_map[id]->js->func_list == NULL && jerry_value_is_function(cb_map[id]->js->js_func)) {
                            jerry_value_t ret_val;
                            // Single function callback
                            if (sz) {
                                ret_val = jerry_call_function(cb_map[id]->js->js_func, cb_map[id]->js->this, data, sz);
                            } else {
                                ret_val = jerry_call_function(cb_map[id]->js->js_func, cb_map[id]->js->this, NULL, 0);
                            }
                            if (cb_map[id]->js->post) {
                                cb_map[id]->js->post(cb_map[id]->js->handle, &ret_val);
                            }
                            if (cb_map[id]->js->once) {
                                zjs_remove_callback(id);
                            }
                        } else {
                            // Function list callback
                            int i;
                            jerry_value_t ret_val;

                            for (i = 0; i < cb_map[id]->js->num_funcs; ++i) {
                                if (sz) {
                                    ret_val = jerry_call_function(cb_map[id]->js->func_list[i], cb_map[id]->js->this, data, sz);
                                } else {
                                    ret_val = jerry_call_function(cb_map[id]->js->js_func, cb_map[id]->js->this, NULL, 0);
                                }
                                if (jerry_value_has_error_flag(ret_val)) {
                                    DBG_PRINT("callback %d returned an error for function[%i]\n", id, i);
                                }
                            }
                            if (cb_map[id]->js->post) {
                                cb_map[id]->js->post(cb_map[id]->js->handle, &ret_val);
                            }
                        }
                    } else if (cb_map[id]->type == CALLBACK_TYPE_C && cb_map[id]->c->function) {
                        if (sz) {
                            cb_map[id]->c->function(cb_map[id]->c->handle, data);
                        } else {
                            cb_map[id]->c->function(cb_map[id]->c->handle, NULL);
                        }
                    }
                } else {
                    PRINT("cb_map[%u]=%p\n", id, cb_map[id]);
                }
#endif
            }
        }
    }
}

