// Copyright (c) 2016-2017, Intel Corporation.

#include <stdint.h>
#include <string.h>
#include "zjs_script.h"

// JerryScript includes
#include "jerryscript.h"

#define SNAPSHOT_BUFFER_SIZE 51200

static uint32_t snapshot_buf[SNAPSHOT_BUFFER_SIZE];

static void generate_snapshot(uint32_t *buf, int buf_size)
{
    for (int i = 0; i < buf_size / sizeof(uint32_t); i++)
    {
        if (i > 0) {
            printf(",");
        }
        printf("0x%08lx", (unsigned long)buf[i]);
    }
}

int main(int argc, char *argv[])
{
    char *script = NULL;
    uint32_t len;

    jerry_init(JERRY_INIT_EMPTY);

    if (argc <= 1) {
        fprintf(stderr, "missing script file\n");
        return 1;
    }

    if (zjs_read_script(argv[1], &script, &len)) {
        fprintf(stderr, "could not read script file %s\n", argv[1]);
        return 1;
    }

    size_t size = jerry_generate_snapshot(NULL,
                                          0,
                                          (const jerry_char_t *)script,
                                          len,
                                          0,
                                          snapshot_buf,
                                          sizeof(snapshot_buf));

    if (script != NULL)
        free(script);

    if (size == 0) {
        fprintf(stderr, "JerryScript: failed to parse JS and create snapshot\n");
        return 1;
    }

    // store the snapshot as byte array in header
    generate_snapshot(snapshot_buf, size);

    return 0;
}
