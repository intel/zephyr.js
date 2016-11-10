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
            ERR_PRINT("error opening file\n");
            return;
        }
        if (fseek(f, 0L, SEEK_END)) {
            ERR_PRINT("error seeking to end of file\n");
            fclose(f);
            return;
        }
        size = ftell(f);
        if (size == -1) {
            ERR_PRINT("ftell returned -1\n");
            fclose(f);
            return;
        }
        if (fseek(f, 0L, SEEK_SET)) {
            ERR_PRINT("error seeking to beginning of file\n");
            fclose(f);
            return;
        }
        s = (char*)zjs_malloc(size);
        if (!s) {
            ERR_PRINT("error allocating %u bytes, fatal\n", size);
            fclose(f);
            return;
        }
        if (fread(s, size, 1, f) != 1) {
            ERR_PRINT("error reading script file\n");
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
