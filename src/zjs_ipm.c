// Copyright (c) 2016, Intel Corporation.
#ifndef QEMU_BUILD
// ipm for ARC communication
#include <ipm/ipm_quark_se.h>

// ZJS includes
#include "zjs_ipm.h"

#include "zjs_common.h"


#ifdef CONFIG_X86
QUARK_SE_IPM_DEFINE(ipm_msg_send, IPM_CHANNEL_X86_TO_ARC, QUARK_SE_IPM_OUTBOUND);
QUARK_SE_IPM_DEFINE(ipm_msg_receive, IPM_CHANNEL_ARC_TO_X86, QUARK_SE_IPM_INBOUND);
#elif CONFIG_ARC
QUARK_SE_IPM_DEFINE(ipm_msg_receive, IPM_CHANNEL_X86_TO_ARC, QUARK_SE_IPM_INBOUND);
QUARK_SE_IPM_DEFINE(ipm_msg_send, IPM_CHANNEL_ARC_TO_X86, QUARK_SE_IPM_OUTBOUND);
#endif

static struct device *ipm_send_dev;
static struct device *ipm_receive_dev;

void zjs_ipm_init() {
    ipm_send_dev = device_get_binding("ipm_msg_send");

    if (!ipm_send_dev) {
        PRINT("Cannot find outbound ipm device!\n" );
    }

    ipm_receive_dev = device_get_binding("ipm_msg_receive");

    if (!ipm_receive_dev) {
        PRINT("Cannot find inbound ipm device!\n" );
    }
}

int zjs_ipm_send(uint32_t id, const void* data, int data_size) {
    if (!ipm_send_dev) {
        PRINT("Cannot find outbound ipm device!\n" );
        return -1;
    }

    return ipm_send(ipm_send_dev, 1, id, data, data_size);
}

void zjs_ipm_register_callback(uint32_t msg_id, ipm_callback_t cb) {
    if (!ipm_receive_dev) {
        PRINT("Cannot find inbound ipm device!\n" );
        return;
    }

    ipm_register_callback(ipm_receive_dev, cb, NULL);
    ipm_set_enabled(ipm_receive_dev, 1);
}

#endif
