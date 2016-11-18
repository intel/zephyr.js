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
 * @brief Intel Hex handler
 *
 * Reads a program from the uart using Intel HEX format.
 * Designed to be used from Javascript or a ECMAScript object file.
 */

#include <nanokernel.h>

#include <stdio.h>
#include <malloc.h>
#include <misc/printk.h>

#include "jerry-code.h"

#include "acm-uart.h"
#include "acm-shell.h"
#include "ihex/kk_ihex_read.h"

#include "file-utils.h"

#ifndef CONFIG_IHEX_UPLOADER_DEBUG
#define DBG(...) { ; }
#else
#define DBG printk
#endif /* CONFIG_IHEX_UPLOADER_DEBUG */

/*
 * Contains the pointer to the memory where the code will be uploaded
 * using the stub interface at zfile.c
 */
static fs_file_t *zfile = NULL;

const char TEMPORAL_FILENAME[] = "temp.dat";

/****************************** IHEX ****************************************/

static bool marker = false;
static struct ihex_state ihex;

static int8_t upload_state = 0;
#define UPLOAD_START       0
#define UPLOAD_IN_PROGRESS 1
#define UPLOAD_FINISHED    2
#define UPLOAD_ERROR       -1

/* Data received from the buffer */
ihex_bool_t ihex_data_read(struct ihex_state *ihex,
    ihex_record_type_t type,
    ihex_bool_t checksum_error)
{

    if (checksum_error) {
        upload_state = UPLOAD_ERROR;
        acm_println("[ERR] Checksum_error");
        return false;
    };

    if (type == IHEX_DATA_RECORD) {
        upload_state = UPLOAD_IN_PROGRESS;
        unsigned long address = (unsigned long)IHEX_LINEAR_ADDRESS(ihex);
        ihex->data[ihex->length] = 0;

        DBG("%d::%d:: \n%s \n", (int)address, ihex->length, ihex->data);
        acm_write("[ACK]", 5);
        acm_write("\n", 1);

        fs_seek(zfile, address, SEEK_SET);
        size_t written = fs_write(zfile, ihex->data, ihex->length);
        if (written == 0 || written != ihex->length) {
            printf("Failed writting into file \n");
            upload_state = UPLOAD_ERROR;
            return false;
        }
    } else if (type == IHEX_END_OF_FILE_RECORD) {
        upload_state = UPLOAD_FINISHED;
    }
    return true;
}

/**************************** DEVICE **********************************/
/*
 * Negotiate a re-upload
 */
void ihex_process_error(uint32_t error)
{
    printf("[Download Error]\n");
}

/*
 * Capture for the Intel Hex parser
 */
uint32_t ihex_process_init()
{
    upload_state = UPLOAD_START;
    printk("[READY]\n");
    acm_print("\n");
    acm_print("[RDY]\n");

    ihex_begin_read(&ihex);
    zfile = fs_open_alloc(TEMPORAL_FILENAME, "w+");

    /* Error getting an id for our data storage */
    if (!zfile) {
        upload_state = UPLOAD_ERROR;
    }

    return (!zfile);
}

uint32_t ihex_process_data(const char *buf, uint32_t len)
{
    uint32_t processed = 0;
    while (len-- > 0) {
        processed++;
        char byte = *buf++;
#ifdef CONFIG_IHEX_UPLOADER_DEBUG
        acm_write(&byte, 1);
#endif
        if (marker) {
            ihex_read_byte(&ihex, byte);
        }

        switch (byte) {
        case ':':
            DBG("<MK>\n");
            ihex_read_byte(&ihex, byte);
            marker = true;
            break;
        case '\r':
            marker = false;
            DBG("<CR>\n");
            break;
        case '\n':
            marker = false;
            DBG("<IF>\n");
            break;
        }
    }
    return processed;
}

bool ihex_process_is_done()
{
    return (upload_state == UPLOAD_FINISHED || upload_state == UPLOAD_ERROR);
}

uint32_t ihex_process_finish()
{
    if (upload_state == UPLOAD_ERROR) {
        printf("[Error] Callback handle error \n");
        fs_close(zfile);

        ashell_process_start();
        return 1;
    }

    if (upload_state != UPLOAD_FINISHED)
        return 1;

    fs_close(zfile);
    ihex_end_read(&ihex);
    acm_print("[EOF]\n");

    printf("Saved file '%s'\n", TEMPORAL_FILENAME);

    ashell_process_start();
    return 0;
}

void ihex_print_status()
{
}

void ihex_process_start()
{
    struct acm_cfg_data cfg;

    cfg.cb_status = NULL;
    cfg.interface.init_cb = ihex_process_init;
    cfg.interface.error_cb = ihex_process_error;
    cfg.interface.is_done = ihex_process_is_done;
    cfg.interface.close_cb = ihex_process_finish;
    cfg.interface.process_cb = ihex_process_data;
    cfg.print_state = ihex_print_status;

    acm_set_config(&cfg);
}
