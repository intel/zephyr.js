// Copyright (c) 2016, Intel Corporation.

#ifndef SRC_ZJS_POOL_H_
#define SRC_ZJS_POOL_H_

#ifdef ZJS_POOL_CONFIG
void zjs_init_mem_pools(void);

void* pool_malloc(uint32_t size);

void pool_free(void* ptr);

void zjs_print_pools(void);
#endif // ZJS_POOL_CONFIG

#endif /* SRC_ZJS_POOL_H_ */
