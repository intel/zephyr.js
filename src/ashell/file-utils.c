// Copyright (c) 2016, Intel Corporation.

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

#include "../zjs_util.h"

int fs_exist(const char *path)
{
    int res;
    struct fs_dirent entry;
    res = fs_stat(path, &entry);
    return !res;
}

fs_file_t *fs_open_alloc(const char *filename, const char *mode)
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
    else {
        /* Return NULL if trying to read from a nonexistent file */
        if (!fs_exist(filename))
            return NULL;
    }

    fs_file_t *file = (fs_file_t *)zjs_malloc(sizeof(fs_file_t));
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

int fs_close_alloc(fs_file_t *fp)
{
    int res = fs_close(fp);
    zjs_free(fp);
    return res;
}
