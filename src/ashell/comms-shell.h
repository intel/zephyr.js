// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __comms_shell_h__
#define __comms_shell_h__

void comms_set_prompt(const char *prompt);
const char *comms_get_prompt();
void comms_set_echo_mode(bool echo_mode);
bool comms_get_echo_mode();

void terminal_start();

#endif  // __comms_shell_h__
