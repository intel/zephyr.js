// Copyright (c) 2016-2018, Intel Corporation.

/**
 * @file
 * @brief Intel Hex handler
 *
 * Reads a program from the uart using Intel HEX format.
 * Designed to be used from Javascript or a ECMAScript object file.
 */

// C includes
#include <stdio.h>

// Zephyr includes
#include <misc/printk.h>
#include <zephyr.h>

#include "jerry-code.h"

// ZJS includes
#include "ihex/kk_ihex_read.h"
#include "term-cmd.h"
#include "term-uart.h"

#include "../zjs_file_utils.h"

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

static s8_t upload_state = 0;
#define UPLOAD_START       0
#define UPLOAD_IN_PROGRESS 1
#define UPLOAD_FINISHED    2
#define UPLOAD_ERROR      -1

void ihex_send(const char *buf, int len)
{
    uart_write_buf(buf, len);
}

/* Data received from the buffer */
ihex_bool_t ihex_data_read(struct ihex_state *ihex, ihex_record_type_t type,
                           ihex_bool_t checksum_error)
{

    if (checksum_error) {
        upload_state = UPLOAD_ERROR;
        comms_print("[ERR] Checksum_error\r\n");
        return false;
    };

    if (type == IHEX_DATA_RECORD) {
        upload_state = UPLOAD_IN_PROGRESS;
        unsigned long address = (unsigned long)IHEX_LINEAR_ADDRESS(ihex);
        ihex->data[ihex->length] = 0;

        DBG("%d::%d:: \n%s \n", (int)address, ihex->length, ihex->data);
        ihex_send("[ACK]", 5);
        ihex_send("\n", 1);

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
void ihex_process_error(u32_t error)
{
    printf("[Download Error]\n");
}

/*
 * Capture for the Intel Hex parser
 */
u32_t ihex_process_init()
{
    upload_state = UPLOAD_START;
    printk("[READY]\n");
    comms_print("\n");
    comms_print("[RDY]\n");

    ihex_begin_read(&ihex);
    zfile = fs_open_alloc(TEMPORAL_FILENAME, "w+");

    /* Error getting an id for our data storage */
    if (!zfile) {
        upload_state = UPLOAD_ERROR;
    }

    return (!zfile);
}

u32_t ihex_process_data(const char *buf, u32_t len)
{
    u32_t processed = 0;
    while (len-- > 0) {
        processed++;
        char byte = *buf++;
#ifdef CONFIG_IHEX_UPLOADER_DEBUG
        ihex_send(&byte, 1);
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

u32_t ihex_process_finish()
{
    if (upload_state == UPLOAD_ERROR) {
        printf("[Error] Callback handle error \n");
        fs_close_alloc(zfile);

        terminal_start();
        return 1;
    }

    if (upload_state != UPLOAD_FINISHED)
        return 1;

    fs_close_alloc(zfile);
    ihex_end_read(&ihex);
    comms_print("[EOF]\n");

#ifdef CONFIG_IHEX_UPLOADER_DEBUG
    printf("Saved file '%s'\n", TEMPORAL_FILENAME);
#endif

    terminal_start();
    return 0;
}

static struct terminal_config ihex_cfg = {
    .init = ihex_process_init,
    .error = ihex_process_error,
    .done = ihex_process_is_done,
    .close = ihex_process_finish,
    .process = ihex_process_data,
    .send = ihex_send
};

void ihex_process_start()
{
    terminal = &ihex_cfg;
    DBG("[Init]\n");
    ihex_process_init();
}
