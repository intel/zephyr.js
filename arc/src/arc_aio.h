// Copyright (c) 2017, Intel Corporation.

#ifndef __arc_aio_h__
#define __arc_aio_h__

void arc_aio_init();

void arc_aio_cleanup();

u32_t arc_pin_read(u8_t pin);

void arc_process_aio_updates();

void arc_handle_aio(struct zjs_ipm_message *msg);

#endif  // __arc_aio_h__
