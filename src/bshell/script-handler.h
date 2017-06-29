// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __jerry_code_runner_h__
#define __jerry_code_runner_h__

#define MAX_BUFFER_SIZE 4096

void javascript_run(const char *file_name);
void javascript_eval(const char *source_buffer, size_t size);
bool javascript_parse(const char *file_name);
void javascript_stop();

#endif // __jerry_code_runner_h__
