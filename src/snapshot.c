// Copyright (c) 2016, Intel Corporation.
#include <string.h>
#include "zjs_script.h"

// JerryScript includes
#include "jerry-api.h"

#define SNAPSHOT_BUFFER_SIZE 51200
#define SNAPSHOT_SOURCE_FILE "src/zjs_snapshot_gen.c"

static uint8_t snapshot_buf[SNAPSHOT_BUFFER_SIZE];

static int generate_snapshot(const char *file_name, uint8_t *buf, int buf_size)
{
    // create or overwite the existing the src file that
    // initialize the array to stores the byte code
    // to be executed by jerryscript
    FILE* f = fopen(file_name, "w+");
    if (!f) {
        ERR_PRINT("error opening file\n");
        return 1;
    }

    fwrite("/* This file was auto-generated */\n\n", 1, 36, f);
    fwrite("#include \"zjs_common.h\"\n\n", 1, 25, f);
    fwrite("const uint8_t snapshot_bytecode[] = {\n", 1, 38, f);

    for (int i = 0; i < buf_size; i++)
    {
        if (i > 0) {
            fwrite(",", 1, 1, f);
        }
        char byte[5];
        snprintf(byte, 5, "0x%02x", snapshot_buf[i]);
        fwrite(byte, 1, 4, f);
        DBG_PRINT("%s,", byte);
    }

    fwrite("\n};\n\n", 1, 5, f);
    fwrite("const int snapshot_len = sizeof(snapshot_bytecode) / ", 1, 53, f);
    fwrite("sizeof(snapshot_bytecode[0]);\n", 1, 30, f);
    fclose(f);

    return 0;
}

int main(int argc, char *argv[])
{
    const char *script = NULL;
    uint32_t len;

    jerry_init(JERRY_INIT_EMPTY);

    if (argc <= 1) {
        ERR_PRINT("missing script file\n");
        return 1;
    }

    if (zjs_read_script(argv[1], &script, &len)) {
        ERR_PRINT("could not read script file %s\n", argv[1]);
        return 1;
    }

    size_t size = jerry_parse_and_save_snapshot((jerry_char_t *)script,
                                                strlen(script),
                                                true,
                                                false,
                                                snapshot_buf,
                                                sizeof(snapshot_buf));

    zjs_free_script(script);

    if (size == 0) {
        ERR_PRINT("JerryScript: failed to parse JS and create snapshot\n");
        return 1;
    }

    // store the snapshot as byte array in header
    if (generate_snapshot(SNAPSHOT_SOURCE_FILE, snapshot_buf, size) != 0) {
        ERR_PRINT("failed to generate %s\n", SNAPSHOT_SOURCE_FILE);
        return 1;
    }

    ZJS_PRINT("Source code %lu bytes\n", len);
    ZJS_PRINT("Byte code %d bytes\n", size);

    return 0;
}
