// Copyright (c) 2017, Intel Corporation.
#ifdef BUILD_MODULE_MATH

// ZJS includes
#include "zjs_util.h"

#define RANDOM_MAX 32767

u32_t random_generator()
{
    // pseudorandom number generator using Xorshift
    // https://en.wikipedia.org/wiki/Xorshift
    // https://de.wikipedia.org/wiki/Xorshift
    static uint32_t x = 123456789;
    static uint32_t y = 362436069;
    static uint32_t z = 521288629;
    static uint32_t w = 88675123;
    uint32_t t;
    t = x ^ (x << 11);
    x = y; y = z; z = w;
    return w = w ^ (w >> 19) ^ (t ^ (t >> 8));
}

static ZJS_DECL_FUNC(math_random)
{
    u32_t next = random_generator() % RANDOM_MAX;
    return jerry_create_number(next);
}

void zjs_math_init(void)
{
    ZVAL math = zjs_create_object();
    zjs_obj_add_function(math, "random", math_random);

    // initialize the Math object
    ZVAL global_obj = jerry_get_global_object();
    zjs_set_property(global_obj, "MathStubs", math);
}

void zjs_math_cleanup()
{
}

#endif  // BUILD_MODULE_MATH
