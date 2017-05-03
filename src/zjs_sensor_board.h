// Copyright (c) 2017, Intel Corporation.

#ifndef __zjs_sensor_board_h__
#define __zjs_sensor_board_h__

#include "zjs_sensor.h"

/*
 * Starts the sensor with the underlying board driver
 *
 * @param handle        the sensor object that was created from zjs_sensor_create
 */
int zjs_sensor_board_start(sensor_handle_t *handle);

/*
 * Stops the sensor with the underlying board driver
 *
 * @param handle        the sensor object that was created from zjs_sensor_create
 */
int zjs_sensor_board_stop(sensor_handle_t *handle);


/*
 * Create and initialize the sensor with the underlying driver
 *
 * @param handle        the sensor object that was created from zjs_sensor_create
 */
int zjs_sensor_board_create(sensor_handle_t *handle);

/*
 * Initialize the sensor board module
 */
void zjs_sensor_board_init();

/*
 * Release resources held by the sensor board module
 */
void zjs_sensor_board_cleanup();

#endif  // __zjs_sensor_board_h__
