// Copyright (c) 2017, Intel Corporation.

#ifndef __zjs_sensor_gyro_h__
#define __zjs_sensor_gyro_h__

/**
 *  Initialize gyroscope sensor module
 *
 *  @return Gyroscope object
 */
sensor_instance_t *zjs_sensor_gyro_init();

/**
 * Clean up module
 */
void zjs_sensor_gyro_cleanup();

#endif  // __zjs_sensor_gyro_h__
