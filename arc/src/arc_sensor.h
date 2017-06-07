// Copyright (c) 2017, Intel Corporation.

#ifndef __arc_sensor_h__
#define __arc_sensor_h__

static u32_t sensor_poll_freq = 20;  // default poll frequency

void arc_handle_sensor(struct zjs_ipm_message *msg);

#ifdef BUILD_MODULE_SENSOR_LIGHT
void arc_fetch_light();
#endif

void arc_fetch_sensor();

#endif  // __arc_sensor_h__
