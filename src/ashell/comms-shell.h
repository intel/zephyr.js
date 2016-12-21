// Copyright (c) 2016, Intel Corporation.

#ifndef __comms_shell_h__
#define __comms_shell_h__

void comms_set_prompt(const char *prompt);
const char *comms_get_prompt();

/**
 * Callback function when a line arrives
 */
typedef int32_t(*ashell_line_parser_t)(char *buf, uint32_t len);

void ashell_process_start();
void ashell_process_close();

void ashell_register_app_line_handler(ashell_line_parser_t cb);

char *ashell_skip_spaces(char *str);
char *ashell_get_token_arg(char *str);
bool ashell_check_parameter(const char *buf, const char parameter);

uint32_t ashell_get_argc(const char *str, uint32_t nsize);
const char *ashell_get_next_arg_s(const char *str, uint32_t nsize, char *str_arg, uint32_t max_arg_size, uint32_t *length);

#endif  // __comms_shell_h__
