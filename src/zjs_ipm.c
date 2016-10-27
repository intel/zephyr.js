// Copyright (c) 2016, Intel Corporation.
#ifndef QEMU_BUILD
// ipm for ARC communication
#include <ipm/ipm_quark_se.h>
#include <string.h>

// ZJS includes
#include "zjs_ipm.h"
#include "zjs_common.h"

#ifdef CONFIG_X86
#include "zjs_util.h"
QUARK_SE_IPM_DEFINE(ipm_msg_send, IPM_CHANNEL_X86_TO_ARC, QUARK_SE_IPM_OUTBOUND);
QUARK_SE_IPM_DEFINE(ipm_msg_receive, IPM_CHANNEL_ARC_TO_X86, QUARK_SE_IPM_INBOUND);
#elif CONFIG_ARC
QUARK_SE_IPM_DEFINE(ipm_msg_receive, IPM_CHANNEL_X86_TO_ARC, QUARK_SE_IPM_INBOUND);
QUARK_SE_IPM_DEFINE(ipm_msg_send, IPM_CHANNEL_ARC_TO_X86, QUARK_SE_IPM_OUTBOUND);
#endif

struct zjs_ipm_callback {
    uint32_t msg_id;
    ipm_callback_t callback;
    struct zjs_ipm_callback *next;
};

static struct device *ipm_send_dev;
static struct device *ipm_receive_dev;

#ifdef CONFIG_X86
static struct zjs_ipm_callback *zjs_ipm_callbacks = NULL;

static void zjs_ipm_msg_callback(void *context, uint32_t id, volatile void *data)
{
    for (struct zjs_ipm_callback *cb = zjs_ipm_callbacks; cb; cb = cb->next) {
        if (cb->msg_id == id) {
            cb->callback(context, id, data);
        }
    }
}
#endif

void zjs_ipm_init()
{
    ipm_send_dev = device_get_binding("ipm_msg_send");

    if (!ipm_send_dev) {
        PRINT("Cannot find outbound ipm device!\n" );
    }

    ipm_receive_dev = device_get_binding("ipm_msg_receive");

    if (!ipm_receive_dev) {
        PRINT("Cannot find inbound ipm device!\n" );
    }

#ifdef CONFIG_X86
    // on x86 side, all ipm is routed through a single callback handler
    ipm_register_callback(ipm_receive_dev, zjs_ipm_msg_callback, NULL);
    ipm_set_enabled(ipm_receive_dev, 1);
#endif
}

int zjs_ipm_send(uint32_t id, zjs_ipm_message_t *data)
{
    if (!ipm_send_dev) {
        PRINT("Cannot find outbound ipm device!\n" );
        return -1;
    }

    // sending pointer to the address of ipm message
    return ipm_send(ipm_send_dev, 1, id, &data, sizeof(void *));
}

void zjs_ipm_register_callback(uint32_t msg_id, ipm_callback_t cb)
{
    if (!ipm_receive_dev) {
        PRINT("Cannot find inbound ipm device!\n" );
        return;
    }

#ifdef CONFIG_X86
    // x86, register for ipm message that matches the MSG_ID
    struct zjs_ipm_callback* callback = zjs_malloc(sizeof(struct zjs_ipm_callback));
    if (!callback) {
        PRINT("zjs_ipm_register_callback: failed to allocate callback\n");
        return;
    }

    memset(callback, 0, sizeof(struct zjs_ipm_callback));
    callback->msg_id = msg_id;
    callback->callback = cb;
    callback->next = zjs_ipm_callbacks;

    zjs_ipm_callbacks = callback;
#elif CONFIG_ARC
    ipm_register_callback(ipm_receive_dev, cb, NULL);
    ipm_set_enabled(ipm_receive_dev, 1);
#endif
}

void zjs_ipm_free_callbacks() {
    #ifdef CONFIG_X86
    struct zjs_ipm_callback **pItem = &zjs_ipm_callbacks;
    while (*pItem) {        
        zjs_free(*pItem);            
        pItem = &(*pItem)->next;
    }
    zjs_free(zjs_ipm_callbacks);
    zjs_ipm_callbacks = NULL;
    #endif
}
#endif // QEMU_BUILD
