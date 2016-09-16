// Copyright (c) 2016, Intel Corporation.

#ifdef ZJS_LINUX_BUILD

#include <stdint.h>
#include <string.h>

#include "zjs_script.h"

void zjs_read_script(char* name, const char** script, uint32_t* length)
{
    if (name) {
        char* s;
        uint32_t size;
        FILE* f = fopen(name, "r");
        if (!f) {
            PRINT("zjs_read_script: Error opening file\n");
            return;
        }
        if (fseek(f, 0L, SEEK_END)) {
            PRINT("zjs_read_script: Error seeking to end of file\n");
            fclose(f);
            return;
        }
        size = ftell(f);
        if (size == -1) {
            PRINT("zjs_read_script: Error, ftell returned -1\n");
            fclose(f);
            return;
        }
        if (fseek(f, 0L, SEEK_SET)) {
            PRINT("zjs_read_script: Error seeking to beginning of file\n");
            fclose(f);
            return;
        }
        s = (char*)zjs_malloc(size);
        if (!s) {
            PRINT("zjs_read_script: Error allocating %u bytes, fatal\n", size);
            fclose(f);
            return;
        }
        if (fread(s, size, 1, f) != 1) {
            PRINT("zjs_read_script: Error reading script file\n");
            fclose(f);
            zjs_free(s);
            return;
        }

        *script = s;
        *length = size;
        fclose(f);
    }

    return;
}

void zjs_free_script(const char* script)
{
    if (script) {
        zjs_free(script);
    }
    return;
}

#endif
