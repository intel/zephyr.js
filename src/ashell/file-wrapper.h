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

#define MAX_FILENAME_SIZE 13

#include <fs/fs_interface.h>
#include <ff.h>
#include <fs/fat_fs.h>
#include <fs.h>

ZFILE *csopen(const char *filename, const char *mode);
int csexist(const char *path);
int csseek(ZFILE *stream, long int offset, int whence);
ssize_t cswrite(const char *ptr, size_t size, size_t count, ZFILE *stream);
ssize_t csread(char *ptr, size_t size, size_t count, ZFILE *stream);
int csclose(ZFILE * stream);
ssize_t cssize(ZFILE *file);

#endif // __file_wrapper_h__
