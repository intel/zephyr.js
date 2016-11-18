/*
 * Copyright 2015 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __file_wrapper_h__
#define __file_wrapper_h__

#include <fs/fs_interface.h>
#include <ff.h>
#include <fs/fat_fs.h>
#include <fs.h>

#define MAX_FILENAME_SIZE MAX_FILE_NAME + 1

fs_file_t *fs_open_alloc(const char *filename, const char *mode);
int fs_close_alloc(fs_file_t * fp);
int fs_exist(const char *path);
ssize_t fs_size(fs_file_t *file);

#endif // __file_wrapper_h__
