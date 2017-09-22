// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __comms_shell_h__
#define __comms_shell_h__

void comms_set_prompt(const char *prompt);
const char *comms_get_prompt();
void comms_set_echo_mode(bool echo_mode);
bool comms_get_echo_mode();

void term_process_start();

char *ashell_skip_spaces(char *str);
char *ashell_get_token_arg(char *str);
// bool ashell_check_parameter(const char *buf, const char parameter);

u32_t ashell_get_argc(const char *str, u32_t nsize);
const char *ashell_get_next_arg_s(const char *str, u32_t nsize, char *str_arg,
                                  u32_t max_arg_size, u32_t *length);

extern struct terminal_config *term_config;

#endif  // __comms_shell_h__
