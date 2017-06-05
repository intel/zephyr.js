// Copyright (c) 2017, Intel Corporation.

#ifndef __zjs_spi_h__
#define __zjs_spi_h__

#include "jerryscript.h"

enum spi_topology {
    ZJS_TOPOLOGY_FULL_DUPLEX,
    ZJS_TOPOLOGY_SINGLE_READ,
    ZJS_TOPOLOGY_SINGLE_WRITE,
    ZJS_TOPOLOGY_MULTIPLEXED,
    ZJS_TOPOLOGY_DAISY_CHAIN
};

enum spi_direction {
    ZJS_SPI_DIR_READ,
    ZJS_SPI_DIR_WRITE,
    ZJS_SPI_DIR_READ_WRITE
};

typedef struct spi_handle {
    struct device *spi_device;
    enum spi_topology topology;
    bool closed;
} spi_handle_t;

/**
 * Initialize the spi module, or reinitialize after cleanup
 *
 * @return SPI API object
 */
jerry_value_t zjs_spi_init();

/** Release resources held by the spi module */
void zjs_spi_cleanup();

#endif  // __zjs_spi_h__
