// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __file_wrapper_h__
#define __file_wrapper_h__

#include <fs/fs_interface.h>
#include <ff.h>
#include <fs/fat_fs.h>
#include <fs.h>

#define MAX_FILENAME_SIZE        MAX_FILE_NAME + 1
#define MAX_ASHELL_JS_MODULE_LEN 12

fs_file_t *fs_open_alloc(const char *filename, const char *mode);
int fs_close_alloc(fs_file_t *fp);
int fs_exist(const char *path);
ssize_t fs_size(fs_file_t *file);
bool fs_valid_filename(char *filename);

#endif // __file_wrapper_h__
