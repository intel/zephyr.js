// Copyright (c) 2017, Intel Corporation.

#ifndef __zjs_sensor_light_h__
#define __zjs_sensor_light_h__

/**
 *  Initialize ambient light sensor module
 *
 *  @return AmbientLightSensor object
 */
sensor_instance_t *zjs_sensor_light_init();

/**
 * Clean up module
 */
void zjs_sensor_light_cleanup();

#endif  // __zjs_sensor_light_h__
