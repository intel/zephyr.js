// Copyright (c) 2016, Intel Corporation.

#include <stdint.h>
#include <string.h>

#include "zjs_script.h"

extern const char script_gen[];
static uint8_t cmd_line = 0;

// this is arbitrary but okay for now; added to avoid plain strlen below
#define MAX_SCRIPT_SIZE 4096

void zjs_read_script(char* name, char** script, uint32_t* length)
{
    if (name == NULL) {
        *script = (char*)script_gen;
        *length = strnlen(script_gen, MAX_SCRIPT_SIZE);
        return;
    } else {
#ifdef ZJS_LINUX_BUILD
        char* s;
        uint32_t size;
        FILE* f = fopen(name, "r");
        if (!f) {
            PRINT("zjs_read_script: Error opening file\n");
            return;
        }
        if (fseek(f, 0L, SEEK_END)) {
            PRINT("zjs_read_script: Error seeking to end of file\n");
            return;
        }
        size = ftell(f);
        if (size == -1) {
            PRINT("zjs_read_script: Error, ftell returned -1\n");
            return;
        }
        if (fseek(f, 0L, SEEK_SET)) {
            PRINT("zjs_read_script: Error seeking to beginning of file\n");
            return;
        }
        s = (char*)zjs_malloc(size);
        if (!s) {
            PRINT("zjs_read_script: Error allocating %u bytes, fatal\n", size);
            return;
        }
        if (fread(s, size, 1, f) != 1) {
            PRINT("zjs_read_script: Error reading script file\n");
            return;
        }

        *script = s;
        *length = size;
        cmd_line = 1;
#endif
        return;
    }
}

void zjs_free_script(char* script)
{
    if (cmd_line) {
        zjs_free(script);
    }
    return;
}
