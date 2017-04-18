// Copyright (c) 2017, Intel Corporation.

#ifndef __zjs_sensor_accel_h__
#define __zjs_sensor_accel_h__

/**
 *  Initialize accelerometer sensor module
 *
 *  @return Accelerometer object
 */
sensor_instance_t *zjs_sensor_accel_init();

/**
 * Clean up module
 */
void zjs_sensor_accel_cleanup();

#endif  // __zjs_sensor_accel_h__
