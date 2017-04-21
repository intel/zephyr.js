// Copyright (c) 2017, Intel Corporation.

#ifndef __zjs_sensor_magn_h__
#define __zjs_sensor_magn_h__

/**
 *  Initialize magnetometer sensor module
 *
 *  @return Magnetometer object
 */
sensor_instance_t *zjs_sensor_magn_init();

/**
 * Clean up module
 */
void zjs_sensor_magn_cleanup();

#endif  // __zjs_sensor_magnet_h__
