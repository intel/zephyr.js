// Copyright (c) 2017, Intel Corporation.

#ifndef __arc_common_h__
#define __arc_common_h__

#include "zjs_common.h"
#include "zjs_ipm.h"

int ipm_send_msg(struct zjs_ipm_message *msg);

int ipm_send_error(struct zjs_ipm_message *msg, u32_t error_code);

#endif  // __arc_common_h__
