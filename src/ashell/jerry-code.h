// Copyright (c) 2016, Intel Corporation.

#ifndef __jerry_code_runner_h__
#define __jerry_code_runner_h__

void javascript_run_code(const char *file_name);
void javascript_eval_code(const char *source_buffer);
int javascript_parse_code(const char *file_name, bool show_lines);
void javascript_stop();

#endif // __jerry_code_runner_h__
