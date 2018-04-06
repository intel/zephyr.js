// Copyright (c) 2018, Intel Corporation.

#ifndef __ashell_ide_webusb_h__
#define __ashell_ide_webusb_h__

#include "webusb_driver.h"

typedef void (*webusb_process_cb)(u8_t *buffer, size_t len);
typedef void (*webusb_process_ack)();

void webusb_init(webusb_process_cb cb, webusb_process_ack ack);
void webusb_receive_process();

#endif  // __ashell_ide_webusb_h__
