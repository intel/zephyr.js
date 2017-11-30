// Copyright (c) 2016-2017, Intel Corporation.

/**
* @file
* @brief Simulates the disk access to create a writtable memory section
* to help on the transactions between the UART and the Javascript fs_file_t
* this is a basic stub, do not expect a full implementation.
*/

// C includes
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Zephyr includes
#include <arch/cpu.h>
#include <fs.h>

#include <device.h>
#include <init.h>

#include <misc/printk.h>

// ZJS includes
#include "file-utils.h"
#include "../zjs_util.h"

#define FILE_ERR ERR_PRINT("Read file failed\n")

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
    } else {
        /* Return NULL if trying to read from a nonexistent file */
        if (!fs_exist(filename)) {
            return NULL;
        }
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

bool fs_valid_filename(char *filename)
{
    if (filename == NULL) {
        printf("No filename given\n");
        return false;
    }

    char *ptr1 = filename;
    char *ptr2 = NULL;
    ssize_t namelen;
    ssize_t extlen;
    ssize_t size = strlen(filename);

    ptr1 = strchr(ptr1, '.');

    if (ptr1 != NULL) {
        // Check there aren't multiple periods
        ptr2 = strchr(ptr1 + 1, '.');

        if (ptr2 != NULL) {
            printf("Invalid file extension format\n");
            return false;
        }

        namelen = ptr1 - filename;
        extlen = size - namelen - 1;
    } else {
        // Filename has no extension
        namelen = size;
        extlen = 0;
    }

    if (namelen == 0) {
        printf("Filename length is zero\n");
        return false;
    } else if (namelen > 8 || extlen > 3) {
        printf("Filename must be 8.3 format\n");
        return false;
    }
    return true;
}

int fs_get_boot_cfg_filename(const char *timestamp, char *filename)
{
    fs_file_t *file;
    size_t count;
    file = fs_open_alloc("boot.cfg", "r");

    if (!file) {
        DBG_PRINT("failed to open boot.cfg\n");
        return -1;
    }

    size_t tssize;
    if (timestamp) {
        tssize = strlen(timestamp);
    } else {
        tssize = strlen(__DATE__ " " __TIME__ "\n");
    }
    ssize_t size = fs_size(file);

    // Check that there is something after the timestamp
    if (size > tssize) {
        char ts[tssize];
        count = fs_read(file, ts, tssize);
        if (count == tssize) {
            // if there's timestamp, check against timestamp
            if (!timestamp ||
                (timestamp && strncmp(ts, timestamp, tssize) == 0)) {
                size_t filenamesize = size - tssize;
                count = fs_read(file, filename, filenamesize);
                filename[filenamesize] = '\0';
                if (count > 0) {
                    fs_close_alloc(file);
                    return 0;
                }
            }
        }
    }

    // boot.cfg is invalid, remove it
    if (fs_unlink("boot.cfg") != 0) {
        DBG_PRINT("cannot remove boot.cfg\n");
    }

    fs_close_alloc(file);
    return -1;
}

char *read_file_alloc(const char *file_name, ssize_t *size)
{
    char *file_buf = NULL;
    fs_file_t *fp = fs_open_alloc(file_name, "r");

    if (fp == NULL)
        return NULL;

    *size = fs_size(fp);
    if (*size == 0) {
        DBG_PRINT("Empty file (%s)\n", file_name);
        FILE_ERR;
        fs_close_alloc(fp);
        return NULL;
    }

    file_buf = (char *)zjs_malloc(*size);
    if (file_buf == NULL) {
        DBG_PRINT("Not enough memory for (%s)\n", file_name);
        FILE_ERR;
        fs_close_alloc(fp);
        return NULL;
    }

    ssize_t brw = fs_read(fp, file_buf, *size);

    if (brw != *size) {
        DBG_PRINT("Read failed for (%s)\n", file_name);
        FILE_ERR;
        fs_close_alloc(fp);
        zjs_free(file_buf);
        return NULL;
    }

    fs_close_alloc(fp);
    return file_buf;
}
