// Copyright (c) 2017, Intel Corporation.

#ifndef __zjs_sensor_temp_h__
#define __zjs_sensor_temp_h__

/**
 *  Initialize temperature sensor module
 *
 *  @return TemperatureSensor object
 */
sensor_instance_t *zjs_sensor_temp_init();

/**
 * Clean up module
 */
void zjs_sensor_temp_cleanup();

#endif  // __zjs_sensor_temp_h__
