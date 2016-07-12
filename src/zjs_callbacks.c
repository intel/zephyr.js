#include <zephyr.h>

#include "zjs_util.h"
#include "zjs_callbacks.h"

#include "jerry-api.h"

#define INITIAL_CALLBACK_SIZE	16
#define CB_CHUNK_SIZE			16

#define CALLBACK_TYPE_JS    0
#define CALLBACK_TYPE_C     1

struct zjs_callback_t {
    int32_t id;
    void* handle;
    zjs_pre_callback_func pre;
    zjs_post_callback_func post;
    jerry_object_t *js_func;
};

struct zjs_c_callback_t {
    int32_t id;
    void* handle;
    zjs_c_callback_func function;
};

struct zjs_callback_map {
    uint8_t type;
    uint8_t signal;
    union {
        struct zjs_callback_t* js;
        struct zjs_c_callback_t* c;
    };
};

static int32_t cb_limit = INITIAL_CALLBACK_SIZE;
static int32_t cb_size = 0;
static struct zjs_callback_map** cb_map = NULL;


static int32_t new_id(void)
{
    int32_t id = 0;
    if (cb_size >= cb_limit) {
        struct zjs_callback_map* new_map = task_malloc(sizeof(struct zjs_callback_map*) *
                                                    (cb_limit + CB_CHUNK_SIZE));

        DBG_PRINT("[callbacks] new_id(): Callback list size too small, increasing by %d\n",
                CB_CHUNK_SIZE);
        memset(new_map, 0, sizeof(struct zjs_callback_map*) *
                (cb_limit + CB_CHUNK_SIZE));
        memcpy(new_map, cb_map, sizeof(struct zjs_callback_map*) * cb_size);
        task_free(cb_map);
        cb_map = new_map;
        cb_limit += CB_CHUNK_SIZE;
    }
    while (cb_map[id] != NULL) {
        id++;
    }
    return id;
}

void zjs_init_callbacks(void)
{
    if (!cb_map) {
        cb_map = (struct zjs_callback_map**)task_malloc(sizeof(struct zjs_callback_map*) *
                INITIAL_CALLBACK_SIZE);
        if (!cb_map) {
            DBG_PRINT("[callbacks] zjs_init_callbacks(): Error allocating space for CB map\n");
            return;
        }
        memset(cb_map, 0, sizeof(struct zjs_callback_map*) * INITIAL_CALLBACK_SIZE);
    }
    return;
}

int32_t zjs_add_callback(jerry_object_t *js_func,
                         void* handle,
                         zjs_pre_callback_func pre,
                         zjs_post_callback_func post)
{
    struct zjs_callback_map* new_cb = task_malloc(sizeof(struct zjs_callback_map));
    new_cb->js = task_malloc(sizeof(struct zjs_callback_t));

    if (!new_cb) {
        DBG_PRINT("[callbacks] zjs_add_callback(): Error allocating new callback\n");
        return -1;
    }
    new_cb->type = CALLBACK_TYPE_JS;
    new_cb->signal = 0;
    new_cb->js->id = new_id();
    new_cb->js->js_func = js_func;
    new_cb->js->pre = pre;
    new_cb->js->post = post;
    new_cb->js->handle = handle;

    // Add callback to list
    memcpy(cb_map + new_cb->js->id, &new_cb, sizeof(struct zjs_callback_map*));
    cb_size++;

    DBG_PRINT("[callbacks] zjs_add_callback(): Adding new callback id %u\n", new_cb->js->id);

    return new_cb->js->id;
}

void zjs_remove_callback(int32_t id)
{
    if (id != -1) {
        if (cb_map[id]->type == CALLBACK_TYPE_JS) {
            task_free(cb_map[id]->js);
        } else {
            task_free(cb_map[id]->c);
        }
        task_free(cb_map[id]);
        memset(cb_map + id, 0, sizeof(struct zjs_callback_map*));
        cb_size--;
        DBG_PRINT("[callbacks] zjs_remove_callback(): Removing callback id %u\n", id);
    }
}

void zjs_signal_callback(int32_t id)
{
    if (id != -1 && cb_map[id]) {
#ifdef DEBUG_BUILD
        if (cb_map[id]->type == CALLBACK_TYPE_JS) {
            DBG_PRINT("[callbacks] zjs_signal_calback(): Signaling JS callback id %u\n", id);
        } else {
            DBG_PRINT("[callbacks] zjs_signal_calback(): Signaling C callback id %u\n", id);
        }
#endif
        cb_map[id]->signal = 1;
    }
}

int32_t zjs_add_c_callback(void* handle, zjs_c_callback_func callback)
{
    struct zjs_callback_map* new_cb = task_malloc(sizeof(struct zjs_callback_map));
    new_cb->c = task_malloc(sizeof(struct zjs_c_callback_t));

    if (!new_cb) {
        DBG_PRINT("[callbacks] zjs_add_c_callback(): Error allocating new callback\n");
        return -1;
    }
    new_cb->type = CALLBACK_TYPE_C;
    new_cb->signal = 0;
    new_cb->c->id = new_id();
    new_cb->c->function = callback;
    new_cb->c->handle = handle;

    // Add callback to list
    memcpy(cb_map + new_cb->js->id, &new_cb, sizeof(struct zjs_callback_map*));
    cb_size++;

    DBG_PRINT("[callbacks] zjs_add_callback(): Adding new C callback id %u\n", new_cb->c->id);

    return new_cb->c->id;
}


void zjs_service_callbacks(void)
{
    int i;
    for (i = 0; i < cb_size; i++) {
        if (cb_map[i]->signal) {
            cb_map[i]->signal = 0;
            if (cb_map[i]->type == CALLBACK_TYPE_JS && cb_map[i]->js->js_func) {
                uint32_t args_cnt = 0;
                jerry_value_t ret_val;
                jerry_value_t* args = NULL;

                if (cb_map[i]->js->pre) {
                    args = cb_map[i]->js->pre(cb_map[i]->js->handle, &args_cnt);
                }

                DBG_PRINT("[callbacks] zjs_service_callbacks(): Calling callback id %u with %u args\n", i, args_cnt);
                // TODO: Use 'this' in callback module
                jerry_call_function(cb_map[i]->js->js_func, NULL, args, args_cnt);
                if (cb_map[i]->js->post) {
                    cb_map[i]->js->post(cb_map[i]->js->handle, &ret_val);
                }
            } else if (cb_map[i]->type == CALLBACK_TYPE_C && cb_map[i]->c->function) {
                cb_map[i]->c->function(cb_map[i]->c->handle);
            }
        }
    }
    return;
}
