/*
 * Copyright (c) 2016, Intel Coporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <zephyr.h>
#include <microkernel/memory_map.h>

#if defined(CONFIG_STDOUT_CONSOLE)
#include <stdio.h>
#define PRINT           printf
#else
#include <misc/printk.h>
#define PRINT           printk
#endif

#define BYTES (44 * 1024 + 800 - 32)

struct nano_lifo mylifo;

DEFINE_MEM_MAP(map1, 1, BYTES)

int main() {
    PRINT("\nMicrokernel Memory Scan\n");
    PRINT("-----------------------\n");

    nano_lifo_init(&mylifo);

    unsigned char *mptr;
    int rval = task_mem_map_alloc(map1, (void **)&mptr, TICKS_UNLIMITED);

    if (rval == RC_OK) {
        PRINT("Allocated %dB successfully!\n", BYTES);

        int i;
        int failCount = 0;

        // test all zeroes
        for (i = 0; i < BYTES; i++) {
            mptr[i] = 0;
        }

        for (i = 0; i < BYTES; i++) {
            if (mptr[i] != 0) {
                failCount++;
            }
        }

        if (failCount) {
            PRINT("Writing zeroes failed %d times.\n", failCount);
        }
        else {
            PRINT("Writing all zeroes succeeded.\n");
        }

        // test all ones
        failCount = 0;
        for (i = 0; i < BYTES; i++) {
            mptr[i] = 0xff;
        }

        for (i = 0; i < BYTES; i++) {
            if (mptr[i] != 0xff) {
                failCount++;
            }
        }

        if (failCount) {
            PRINT("Writing ones failed %d times.\n", failCount);
        }
        else {
            PRINT("Writing all ones succeeded.\n");
        }

        // writing different values
        failCount = 0;
        for (i = 0; i < BYTES; i++) {
            mptr[i] = i % 256;
        }

        for (i = 0; i < BYTES; i++) {
            if (mptr[i] != i % 256) {
                failCount++;
            }
        }

        if (failCount) {
            PRINT("Writing varying values failed %d times.\n", failCount);
        }
        else {
            PRINT("Writing varying values succeeded.\n");
        }
    }

    return 0;
}
