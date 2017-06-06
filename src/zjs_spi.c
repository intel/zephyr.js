// Copyright (c) 2017, Intel Corporation.

// Zephyr includes
#ifndef ZJS_LINUX_BUILD
#include <zephyr.h>
#include <spi.h>
#include <misc/util.h>
#endif
#include <string.h>
#include <stdlib.h>

// ZJS includes
#include "zjs_spi.h"
#include "zjs_util.h"
#include "zjs_buffer.h"
#include "zjs_callbacks.h"
#include "zjs_error.h"

#define SPI_BUS "SPI_"
#define MAX_SPI_BUS 127
#define MAX_READ_BUFF 1000
#define MAX_DIR_LEN 13

static jerry_value_t zjs_spi_prototype;

static void zjs_spi_callback_free(void *native)
{
    // requires: handle is the native pointer we registered with
    //             jerry_set_object_native_handle
    //  effects: frees the spi item
    spi_handle_t *handle = (spi_handle_t *)native;
    handle->closed = true;
    zjs_free(handle);
}

static const jerry_object_native_info_t spi_type_info =
{
   .free_cb = zjs_spi_callback_free
};

static ZJS_DECL_FUNC(zjs_spi_transceive)
{
    // requires: Writes and reads from the SPI bus, takes one argument
    //           arg[0] - Buffer to write to the SPI bus
    //  effects: Writes the buffer to the SPI bus and returns the buffer
    //           received on the SPI in reply.

    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_OPTIONAL Z_ARRAY Z_STRING, Z_OPTIONAL Z_STRING);

    // No more than 3 args
    if (argc > 3)
        return ZJS_STD_ERROR(RangeError, "Invalid args");

    ZJS_GET_HANDLE(this, spi_handle_t, handle, spi_type_info);

    if (handle->closed == true) {
        ZJS_PRINT("SPI bus is closed\n");
        return jerry_create_null();
    }

    size_t len;
    jerry_value_t buffer;
    zjs_buffer_t *txBuf;
    zjs_buffer_t *rxBuf;
    jerry_value_t rxBuf_obj = jerry_create_null();
    jerry_value_t txBuf_obj;

    if (spi_slave_select(handle->spi_device, jerry_get_number_value(argv[0]))) {
        return zjs_error("SPI slave select failed\n");
    }

    // If only a slave is given, this must be a single read or its invalid
    if (argc == 1) {
        if (handle->topology != ZJS_TOPOLOGY_SINGLE_READ) {
            return ZJS_STD_ERROR(RangeError, "Missing transmit buffer");
        }
    }

    char dirString[13];
    enum spi_direction dirArg;

    // Set the direction default based on the topology.
    if (handle->topology == ZJS_TOPOLOGY_SINGLE_READ)
        dirArg = ZJS_SPI_DIR_READ;
    else if (handle->topology == ZJS_TOPOLOGY_SINGLE_WRITE)
        dirArg = ZJS_SPI_DIR_WRITE;
    else
        dirArg = ZJS_SPI_DIR_READ_WRITE;

    // If we have a 'direction' arg, get it and validate
    if (argc == 3) {
        uint32_t dirLen = jerry_get_string_size(argv[2]) + 1;
        zjs_copy_jstring(argv[2], dirString, &dirLen);

        if (strncmp(dirString, "read-write", 11) == 0)
            dirArg = ZJS_SPI_DIR_READ_WRITE;
        else if (strncmp(dirString, "read", 5) == 0)
            dirArg = ZJS_SPI_DIR_READ;
        else if (strncmp(dirString, "write", 6) == 0)
            dirArg = ZJS_SPI_DIR_WRITE;
        else
            return ZJS_STD_ERROR(RangeError, "Invalid direction");

        // If topology conflicts with direction given
        if ((handle->topology == ZJS_TOPOLOGY_SINGLE_WRITE &&
            dirArg != ZJS_SPI_DIR_WRITE) ||
            (handle->topology == ZJS_TOPOLOGY_SINGLE_READ &&
            dirArg != ZJS_SPI_DIR_READ)) {
            return ZJS_STD_ERROR(NotSupportedError, "Direction conflicts with topology");
        }
        // If reading only, the 2nd arg should be NULL
        if (dirArg == ZJS_SPI_DIR_READ && !jerry_value_is_null(argv[1])) {
            return ZJS_STD_ERROR(NotSupportedError, "Direction conflicts with topology");
        }
    }

    // If we need to write a buffer
    if (dirArg != ZJS_SPI_DIR_READ) {
        buffer = argv[1];
        // Figure out if the buffer is an array or string, handle accordingly
        if (jerry_value_is_array(argv[1])) {
            len = jerry_get_array_length(buffer);
            txBuf_obj = zjs_buffer_create(len, &txBuf);
            if (txBuf) {
                for (int i = 0; i < len; i++) {
                    ZVAL item = jerry_get_property_by_index(buffer, i);
                    if (jerry_value_is_number(item)) {
                        txBuf->buffer[i] = (uint8_t)jerry_get_number_value(item);
                    } else {
                        ERR_PRINT("non-numeric value in array, treating as 0\n");
                        txBuf->buffer[i] = 0;
                    }
                }
            }
        }
        else {
            txBuf_obj = zjs_buffer_create(jerry_get_string_size(buffer), &txBuf);
            // zjs_copy_jstring adds a null terminator, which we don't want
            // so make a new string instead and remove it.
            char *tmpBuf = zjs_alloc_from_jstring(argv[1], NULL);
            strncpy(txBuf->buffer, tmpBuf, txBuf->bufsize);
            zjs_free(tmpBuf);
        }
        // If this is a read / write
        if (dirArg == ZJS_SPI_DIR_READ_WRITE) {
            rxBuf_obj = zjs_buffer_create(txBuf->bufsize, &rxBuf);
            // Send the data and read from the device
            if (spi_transceive(handle->spi_device, txBuf->buffer , txBuf->bufsize, rxBuf->buffer, rxBuf->bufsize) != 0) {
                return ZJS_STD_ERROR(SystemError, "SPI transceive failed");
            }
        }
        // This is a write only operation, return a NULL buffer
        else {
            if (spi_write(handle->spi_device, txBuf->buffer , txBuf->bufsize) !=0) {
                return ZJS_STD_ERROR(SystemError, "SPI transceive failed");
            }
            rxBuf_obj = jerry_create_null();
        }
    }   // This is a read only operation
    else {
        rxBuf_obj = zjs_buffer_create(MAX_READ_BUFF, &rxBuf);
        // Read the data from the device
        if (spi_read(handle->spi_device, rxBuf->buffer, rxBuf->bufsize) !=0) {
            return ZJS_STD_ERROR(SystemError, "SPI transceive failed");
        }
    }
    return rxBuf_obj;
}

static ZJS_DECL_FUNC(zjs_spi_close)
{
    // requires: Closes SPI
    //  effects: Bus is closed

    ZJS_GET_HANDLE(this, spi_handle_t, handle, spi_type_info);
    handle->closed = true;
    return jerry_create_boolean(true);
}

static ZJS_DECL_FUNC(zjs_spi_open)
{
    // requires: This is a SPI object from zjs_spi_open, it has one optional args
    //           arg[0] - config object
    //  effects: Creates the SPI object

    ZJS_VALIDATE_ARGS(Z_OPTIONAL Z_OBJECT);

    jerry_value_t init = 0;
    if (argc >= 1)
        init = argv[0];

    // Default values
    uint32_t bus = 0;
    double speed = 10;
    bool msbFirst = true;
    uint32_t bits = 8;
    uint32_t polarity = 0;
    uint32_t phase = 0;
    char topologyStr[13] = "";
    char busStr[9];
    enum spi_topology topology = ZJS_TOPOLOGY_FULL_DUPLEX;
    uint32_t frameGap = 0;
    struct spi_config config = { 0 };

    // Get any provided optional args
    if (init) {
        zjs_obj_get_uint32(init, "bus", &bus);
        if (bus < MAX_SPI_BUS) {
            snprintf(busStr, 9, "%s%lu", SPI_BUS, bus);
        }
        else
            return ZJS_STD_ERROR(RangeError, "Invalid bus");

        // Bus speed in MHz
        zjs_obj_get_double(init, "speed", &speed);

        // Most significant bit sent first
        zjs_obj_get_boolean(init, "msbFirst", &msbFirst);

        // Number of data bits, valid options are 1, 2, 4, 8, or 16
        zjs_obj_get_uint32(init, "bits", &bits);
        if (bits > 16 || (bits != 1 && bits % 2 != 0))
            return ZJS_STD_ERROR(RangeError, "Invalid bits");

        // Polarity value, valid options are 0 or 2
        zjs_obj_get_uint32(init, "polarity", &polarity);
        if (polarity != 0 && polarity != 2)
            return ZJS_STD_ERROR(TypeError, "Invalid polarity");

        // Clock phase value, valid options are 0 or 1
        zjs_obj_get_uint32(init, "phase", &phase);
        if (phase != 0 && phase != 2)
            return ZJS_STD_ERROR(TypeError, "Invalid phase");

        // Connection type
        zjs_obj_get_string(init, "topology", topologyStr, 13);
        if (strncmp(topologyStr, "full-duplex", 12) == 0)
            topology = ZJS_TOPOLOGY_FULL_DUPLEX;
        else if (strncmp(topologyStr, "single-read", 12) == 0)
            topology = ZJS_TOPOLOGY_SINGLE_READ;
        else if (strncmp(topologyStr, "single-write", 12) == 0)
            topology = ZJS_TOPOLOGY_SINGLE_WRITE;
        else if (strncmp(topologyStr, "multiplexed", 11) == 0)
            topology = ZJS_TOPOLOGY_MULTIPLEXED;
        else if (strncmp(topologyStr, "daisy-chain", 11) == 0)
            topology = ZJS_TOPOLOGY_DAISY_CHAIN;
        else if (strncmp(topologyStr, "", 2) != 0)
            return ZJS_STD_ERROR(RangeError, "Invalid topology");

        zjs_obj_get_uint32(init, "frameGap", &frameGap);

        config.config |= SPI_WORD(bits);

        if (msbFirst)
            config.config |= SPI_TRANSFER_MSB;
        else
            config.config |= SPI_TRANSFER_LSB;

        if (speed > 0)
            config.max_sys_freq = speed;

        // Note: The mode is determined by adding the polarity and phase bits
        // together, this is why polarity is either 0 or 2
        if (polarity == 2)
            config.config |= SPI_MODE_CPOL;

        if (phase == 1)
            config.config |= SPI_MODE_CPHA;
    }

    struct device *spi_device = device_get_binding(busStr);

    if (!spi_device) {
        return zjs_error("Could not find SPI driver\n");
    }

    if (spi_configure(spi_device, &config)) {
        return zjs_error("SPI configuration failed\n");
    }

    // Create the SPI object
    jerry_value_t spi_obj = jerry_create_object();
    jerry_set_prototype(spi_obj, zjs_spi_prototype);

    spi_handle_t *handle = zjs_malloc(sizeof(spi_handle_t));
    handle->spi_device = spi_device;
    handle->closed = false;
    handle->topology = topology;
    jerry_set_object_native_pointer(spi_obj, handle, &spi_type_info);
    return spi_obj;
}

jerry_value_t zjs_spi_init()
{
    // Create SPI pin prototype object
    zjs_native_func_t array[] = {
        { zjs_spi_transceive, "transceive" },
        { zjs_spi_close, "close" },
        { NULL, NULL }
    };
    zjs_spi_prototype = jerry_create_object();
    zjs_obj_add_functions(zjs_spi_prototype, array);

    // Create SPI object
    jerry_value_t spi_obj = jerry_create_object();
    zjs_obj_add_function(spi_obj, zjs_spi_open, "open");

    return spi_obj;
}

void zjs_spi_cleanup()
{
    jerry_release_value(zjs_spi_prototype);
}
