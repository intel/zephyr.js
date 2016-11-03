// Copyright (c) 2016, Intel Corporation.

#ifdef ZJS_POOL_CONFIG
#include <zephyr.h>

#include <string.h>

#include "zjs_common.h"
#include "zjs_util.h"

/*
 * Overhead:
 *
 * pool_map_t:      ptr = 4 bytes
 *                  req_size = 4 bytes
 *                  total = 8 bytes
 *
 * pool_lookup_t:   size = 4 bytes
 *                  pool_id = 4 bytes
 *                  total = 8 bytes
 *
 * lookup:          sizeof(pool_lookup_t) * length(lookup)
 *                  8 * 6
 *                  = 48 bytes
 *
 * pointer_map:     sizeof(pool_map_t) * MAX_CONCURRENT_POINTERS
 *                  8 * 64
 *                  = 512 bytes
 *
 * total overhead:  48 + 512
 *                  = 560 bytes
 *
 *
 * TODO: Find a better way to generate the pool sizes and max concurrent
 *       pointers. There should be a way to choose the values based on a static
 *       analysis of the script, looking at what modules are being used and how
 *       many malloc's each does.
 */

typedef struct pool_map {
    void* ptr;
    uint32_t req_size;
#ifdef DUMP_MEM_STATS
    uint32_t pool_size;
#endif
} pool_map_t;

typedef struct pool_lookup {
    uint32_t size;
    uint32_t pool_id;
} pool_lookup_t;

static pool_lookup_t lookup[] = {
    { 8,    POOL_8 },
    { 16,   POOL_16 },
    { 36,   POOL_36 },
    { 64,   POOL_64 },
    { 128,  POOL_128 },
    { 256,  POOL_256 }
};

#define MAX_CONCURRENT_POINTERS  64

static pool_map_t pointer_map[MAX_CONCURRENT_POINTERS];

#define POOL_SIZE_TOO_SMALL     0xFFFFFFFF

#ifdef DUMP_MEM_STATS
static uint32_t mem_high_water = 0;
static uint32_t mem_in_use = 0;
static uint32_t pointers_used = 0;
static uint32_t max_pointers_used = 0;
static uint32_t max_waste = 0;

void zjs_print_pools(void)
{
    int i;
    uint32_t total_waste = 0;
    ZJS_PRINT("\nDumping pools:\n");
    for (i = 0; i < (sizeof(lookup) / sizeof(lookup[0])); ++i) {
        int j;
        uint32_t cur_use = 0;
        uint32_t cur_waste = 0;
        uint32_t min_size = lookup[i].size;
        uint32_t max_size = lookup[i].size;
        ZJS_PRINT("Pool size: %lu\n", lookup[i].size);
        for (j = 0; j < MAX_CONCURRENT_POINTERS; ++j) {
            if (pointer_map[j].ptr && pointer_map[j].pool_size == lookup[i].size) {
                cur_use++;
                cur_waste += lookup[i].size - pointer_map[j].req_size;
                if (min_size > pointer_map[j].req_size) {
                    min_size = pointer_map[j].req_size;
                }
                if (max_size < pointer_map[j].req_size) {
                    max_size = pointer_map[j].req_size;
                }
            }
        }
        ZJS_PRINT("\tBlocks Used: %lu, Memory Used: %lu, Memory Waste: %lu\n", cur_use, cur_use * lookup[i].size, cur_waste);
        ZJS_PRINT("\tMin Size: %lu, Max Size: %lu\n", min_size, max_size);
        total_waste += cur_waste;
    }
    if (max_waste < total_waste) {
        max_waste = total_waste;
    }
    ZJS_PRINT("Memory Used: %lu, High Water: %lu\n", mem_in_use, mem_high_water);
    ZJS_PRINT("Memory Waste: %lu, Max Waste: %lu\n", total_waste, max_waste);
    ZJS_PRINT("Pointers Used: %lu, Max Used: %lu\n", pointers_used, max_pointers_used);
}
#else
#define zjs_print_pools(void) do {} while (0);
#endif

void zjs_init_mem_pools(void)
{
    int i;
    for (i = 0; i < MAX_CONCURRENT_POINTERS; ++i) {
#ifdef DUMP_MEM_STATS
        pointer_map[i].req_size = 0;
#endif
        pointer_map[i].ptr = NULL;
    }
}

static uint32_t find_pool_id(uint32_t size)
{
    int i;
    for (i = 0; i < (sizeof(lookup) / sizeof(lookup[0])); ++i) {
        if (size <= lookup[i].size) {
            return lookup[i].pool_id;
        }
    }
    DBG_PRINT("no pool ID found\n");
    return 0;
}

#ifdef DUMP_MEM_STATS
static void add_pointer(void* ptr, uint32_t req_size, uint32_t size, uint32_t pool_size)
#else
static void add_pointer(void* ptr, uint32_t req_size)

#endif
{
    int i;
    for (i = 0; i < MAX_CONCURRENT_POINTERS; ++i) {
        if (pointer_map[i].ptr == NULL) {
            pointer_map[i].ptr = ptr;
            pointer_map[i].req_size = req_size;
#ifdef DUMP_MEM_STATS
            pointer_map[i].req_size = size;
            pointer_map[i].pool_size = pool_size;
            mem_in_use += size;
            if (mem_in_use > mem_high_water) {
                mem_high_water = mem_in_use;
            }
#endif
            return;
        }
    }
}

#ifdef DUMP_MEM_STATS
static uint32_t lookup_pool(uint32_t size, uint32_t* pool_size)
#else
static uint32_t lookup_pool(uint32_t size)

#endif
{
    int i;
    for (i = 0; i < (sizeof(lookup) / sizeof(lookup[0])); ++i) {
        if (size <= lookup[i].size) {
#ifdef DUMP_MEM_STATS
            *pool_size = lookup[i].size;
#endif
            return lookup[i].pool_id;
        }
    }
    return POOL_SIZE_TOO_SMALL;
}

void* pool_malloc(uint32_t size)
{
    int ret;
    struct k_block block;
    uint32_t pool;
#ifdef DUMP_MEM_STATS
    uint32_t pool_size;
    pool = lookup_pool(size, &pool_size);
#else
    pool = lookup_pool(size);
#endif
    if (pool == POOL_SIZE_TOO_SMALL) {
        DBG_PRINT("no pool size big enough for %lu bytes\n", size);
        return NULL;
    }
    ret = task_mem_pool_alloc(&block, pool, size, TICKS_NONE);
    if (ret != RC_OK) {
        DBG_PRINT("task_mem_pool_alloc() returned an error: %u\n", ret);
        return NULL;
    }
#ifdef DUMP_MEM_STATS
    add_pointer(block.pointer_to_data, block.req_size, size, pool_size);
    pointers_used++;
    if (max_pointers_used < pointers_used) {
        max_pointers_used = pointers_used;
    }
#else
    add_pointer(block.pointer_to_data, block.req_size);
#endif
#ifdef ZJS_TRACE_MALLOC
#ifdef DUMP_MEM_STATS
    ZJS_PRINT("\tpool size=%lu\n", pool_size);
#endif
#endif

    zjs_print_pools();

    return block.pointer_to_data;
}

void pool_free(void* ptr)
{
    int i;
    for (i = 0; i < MAX_CONCURRENT_POINTERS; ++i) {
        if (pointer_map[i].ptr == ptr) {
            struct k_block block;
            block.address_in_pool = pointer_map[i].ptr;
            block.pointer_to_data = pointer_map[i].ptr;
            block.req_size = pointer_map[i].req_size;
            block.pool_id = find_pool_id(pointer_map[i].req_size);
#ifdef DUMP_MEM_STATS
            mem_in_use -= pointer_map[i].req_size;
            pointers_used--;
#endif
#ifdef ZJS_TRACE_MALLOC
#ifdef DUMP_MEM_STATS
            ZJS_PRINT("\tpointer size=%lu bytes\n", pointer_map[i].req_size);
#endif
#endif
            task_mem_pool_free(&block);
            pointer_map[i].ptr = NULL;
            zjs_print_pools();
            return;
        }
    }
#ifdef DUMP_MEM_STATS
    zjs_print_pools();
#endif
}
#endif

void zjs_pool_cleanup(void)
{
    int i;
    // Loop through pointer map
    for (i = 0; i < MAX_CONCURRENT_POINTERS; ++i) {
        // if pointer is being used, free it
        if (pointer_map[i].ptr) {
            struct k_block block;
            block.address_in_pool = pointer_map[i].ptr;
            block.pointer_to_data = pointer_map[i].ptr;
            block.req_size = pointer_map[i].req_size;
            block.pool_id = find_pool_id(pointer_map[i].req_size);
            task_mem_pool_free(&block);
            pointer_map[i].ptr = NULL;
        }
    }
#ifdef ZJS_DUMP_MEM_STATS
    mem_high_water = 0;
    mem_in_use = 0;
    pointers_used = 0;
    max_pointers_used = 0;
    max_waste = 0;
#endif
#ifdef ZJS_TRACE_MALLOC
    ZJS_PRINT("zjs_pool_cleanup(): all pools freed\n");
#endif
}
