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

/**
* @file
* @brief Simulates the disk access to create a writtable memory section
* to help on the transactions between the UART and the Javascript fs_file_t
* this is a basic stub, do not expect a full implementation.
*/

#include <nanokernel.h>
#include <arch/cpu.h>
#include <fs.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <device.h>
#include <init.h>

#include <misc/printk.h>
#include <malloc.h>

int fs_exist(const char *path)
{
    int res;
    struct fs_dirent entry;
    res = fs_stat(path, &entry);
    return !res;
}

fs_file_t *fs_open_alloc(const char * filename, const char * mode)
{
    int res;

    /* Delete file if exists */
    if (mode[0] == 'w') {
        if (fs_exist(filename)) {
            /* Delete the file and verify checking its status */
            res = fs_unlink(filename);
            if (res) {
                printk("Error deleting file [%d]\n", res);
                return NULL;
            }
        }
    }

    fs_file_t *file = (fs_file_t *) malloc(sizeof(fs_file_t));
    res = fs_open(file, filename);
    if (res) {
        printk("Failed opening file [%d]\n", res);
        return NULL;
    }
    return file;
}

ssize_t fs_size(fs_file_t *file)
{
    if (fs_seek(file, 0, SEEK_END) != 0)
        return -1;

    off_t file_len = fs_tell(file);
    fs_seek(file, 0, SEEK_SET);
    return file_len;
}

int fs_close_alloc(fs_file_t * fp)
{
    int res = fs_close(fp);
    free(fp);
    return res;
}
