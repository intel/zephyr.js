// Copyright (c) 2016-2017, Intel Corporation.

#ifdef ZJS_LINUX_BUILD

// C includes
#include <stdint.h>
#include <string.h>

// ZJS includes
#include "zjs_script.h"

uint8_t zjs_read_script(char *name, char **script, uint32_t *length)
{
    if (name) {
        char *s;
        uint32_t size;
        FILE *f = fopen(name, "r");
        if (!f) {
            ERR_PRINT("error opening file '%s'\n", name);
            return 1;
        }
        if (fseek(f, 0L, SEEK_END)) {
            ERR_PRINT("error seeking to end of file\n");
            fclose(f);
            return 1;
        }
        size = ftell(f);
        if (size == -1) {
            ERR_PRINT("ftell returned -1\n");
            fclose(f);
            return 1;
        }
        if (fseek(f, 0L, SEEK_SET)) {
            ERR_PRINT("error seeking to beginning of file\n");
            fclose(f);
            return 1;
        }
        s = (char *)malloc(size + 1);
        if (!s) {
            ERR_PRINT("error allocating %u bytes, fatal\n", size);
            fclose(f);
            return 1;
        }
        if (fread(s, size, 1, f) != 1) {
            ERR_PRINT("error reading script file\n");
            fclose(f);
            free(s);
            return 1;
        }

        s[size] = '\0';
        *script = s;
        *length = size;
        fclose(f);
    }

    return 0;
}

void zjs_free_script(char *script)
{
    if (script) {
        free(script);
    }
    return;
}

#endif
