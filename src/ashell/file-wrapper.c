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
 * to help on the transactions between the UART and the Javascript ZFILE
 * this is a basic stub, do not expect a full implementation.
 */

#include <nanokernel.h>
#include <arch/cpu.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <device.h>
#include <init.h>

#include <misc/printk.h>
#include <malloc.h>
#include "file-wrapper.h"

int csexist(const char *path)
{
    int res;
    struct zfs_dirent entry;
    res = fs_stat(path, &entry);
    return !res;
}

ZFILE *csopen(const char * filename, const char * mode)
{
    printk("[OPEN] %s\n", filename);
    int res;

    /* Delete file if exists */
    if (mode[0] == 'w') {
        if (csexist(filename)) {
            /* Delete the file and verify checking its status */
            res = fs_unlink(filename);
            if (res) {
                printk("Error deleting file [%d]\n", res);
                return NULL;
            }
        }
    }

    ZFILE *file = (ZFILE *)malloc(sizeof(ZFILE));
    res = fs_open(file, filename);
    if (res) {
        printk("Failed opening file [%d]\n", res);
        return NULL;
    }
    return file;
}

int csseek(ZFILE *fp, long int offset, int whence)
{
    int res = fs_seek(fp, offset, whence);
    if (res) {
        printk("fs_seek failed [%d]\n", res);
        fs_close(fp);
        return res;
    }

    return 0;
}

ssize_t cssize(ZFILE *file)
{
    if (csseek(file, 0, SEEK_END) != 0)
        return -1;

    off_t file_len = fs_tell(file);
    csseek(file, 0, SEEK_SET);
    return file_len;
}

ssize_t cswrite(const char * ptr, size_t size, size_t count, ZFILE * fp)
{
    ssize_t brw;
    size *= count;

    brw = fs_write(fp, (const char *)ptr, size);
    if (brw < 0) {
        printk("Failed writing to file [%d]\n", (int) brw);
        fs_close(fp);
        return 0;
    }

    return brw;
}

ssize_t csread(char * ptr, size_t size, size_t count, ZFILE * fp)
{
    ssize_t brw = fs_read(fp, ptr, size);
    if (brw < 0) {
        printk("Failed reading file [%d]\n", (int) brw);
        fs_close(fp);
        return -1;
    }
    return brw;
}

int csclose(ZFILE * fp)
{
    printk("[CLOSE]\n");
    int res = fs_close(fp);
    free(fp);
    return res;
}

#ifdef CONFIG_ZFILE_MEMORY_TESTING
void main()
{
    ZFILE *myfile;

    myfile = csopen("test.js", "w+");
    printk(" Getting memory %p \n", myfile);

    cswrite("01234567890123456789\0", 21, sizeof(char), myfile);
    printk("[%s] %i \n", myfile->data, myfile->curoff);

    csseek(myfile, 10, SEEK_SET);
    cswrite("ABCDEFGHIK\0", 11, sizeof(char), myfile);
    printk("[%s] %i \n", myfile->data, myfile->curoff);

    csseek(myfile, 5, SEEK_SET);
    cswrite("01234", 5, sizeof(char), myfile);
    printk("[%s] %i \n", myfile->data, myfile->curoff);

    cswrite("01234\0", 6, sizeof(char), myfile);
    printk("[%s] %i \n", myfile->data, myfile->curoff);

    csseek(myfile, -10, SEEK_END);
    cswrite("012345", 5, sizeof(char), myfile);
    printk("[%s] %i \n", myfile->data, myfile->curoff);

    printk(" End \n");
}
#endif
