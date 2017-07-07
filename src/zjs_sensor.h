// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __zjs_sensor_h__
#define __zjs_sensor_h__

// Zephyr includes
#include <sensor.h>

// JerryScript includes
#include "jerryscript.h"

// ZJS includes
#include "zjs_callbacks.h"

#define DEFAULT_SAMPLING_FREQUENCY     20
#define SENSOR_MAX_CONTROLLER_NAME_LEN 16

typedef enum sensor_state {
    SENSOR_STATE_UNCONNECTED,
    SENSOR_STATE_IDLE,
    SENSOR_STATE_ACTIVATING,
    SENSOR_STATE_ACTIVATED,
    SENSOR_STATE_ERRORED,
} sensor_state_t;

typedef struct sensor_controller {
    struct device *dev;
    char name[SENSOR_MAX_CONTROLLER_NAME_LEN + 1];
    u32_t pin;
} sensor_controller_t;

typedef struct sensor_handle {
    sensor_state_t state;
    enum sensor_channel channel;
    sensor_controller_t *controller;
    int frequency;
    jerry_value_t sensor_obj;
    zjs_callback_id onchange_cb_id;
    zjs_callback_id onstart_cb_id;
    zjs_callback_id onstop_cb_id;
    struct sensor_handle *next;
} sensor_handle_t;

typedef struct sensor_instance {
    void *handles;
} sensor_instance_t;

/*
 * Create a new sensor instance struct that stores all the created objects
 *
 * @param name          name of the Sensor subclass that will exist in global
 * @param func          constructor of the Sensor subclass
 *
 * @return              pointer to the sensor instance struct
 */
sensor_instance_t *zjs_sensor_create_instance(const char *name, void *func);

/*
 * Destroys a sensor instance struct and free its resources
 *
 * @param instance      instance object that belongs to the Sensor subclass
 */
void zjs_sensor_free_instance(sensor_instance_t *instance);

/*
 * Creates a Generic W3C Sensor object using the controller information and
 *   initialize it
 *
 * @param instance      instance object that is created by each sensor module's
 *                        init function
 * @param channel       the type of sensor object, maps to the supported Zephyr
 *                        sensor channels
 * @param c_name        the default hardware controller name if not set
 * @param pin           the default pin if not set
 * @param max_frequency the max supported polling frequency
 * @param onchange      the function to be called when sensor reports data has
 *                        changed
 * @param onstart       the function to be called when sensor.start() has been
 *                         called
 * @param onstart       the function to be called when sensor.stop() has been
 *                        called
 *
 * @return              a new Sensor object or Error object if creation failed
 */
jerry_value_t zjs_sensor_create(const jerry_value_t func_obj,
                                const jerry_value_t this,
                                const jerry_value_t argv[],
                                const jerry_length_t argc,
                                sensor_instance_t *instance,
                                enum sensor_channel channel,
                                const char *c_name,
                                u32_t pin,
                                u32_t max_frequency,
                                zjs_c_callback_func onchange,
                                zjs_c_callback_func onstart,
                                zjs_c_callback_func onstop);

/*
 * Starts the sensor, usually in trigger mode or polling mode depending on the
 * controller, this should be called after onstart has been called, any new
 * readings will then be notified
 * in the onchange callback
 *
 * @param obj           the sensor object that was created from
 *                        zjs_sensor_create
 */
jerry_value_t zjs_sensor_start_sensor(jerry_value_t obj);

/*
 * Stops the sensor, usually in trigger mode or polling mode depending on the
 * controller, this should be called after onstop has been called, you will
 * then stop receive any new readings
 *
 * @param obj           the sensor object that was created from
 *                        zjs_sensor_create
 */
jerry_value_t zjs_sensor_stop_sensor(jerry_value_t obj);

/*
 * Get the current state of the sensor
 *
 * @param obj           the sensor object that was created from
 *                        zjs_sensor_create
 *
 * @return              the state from enum sensor_state.
 */
sensor_state_t zjs_sensor_get_state(jerry_value_t obj);

/*
 * Set the current state of the sensor
 *
 * @param obj           the sensor object that was created from
 *                        zjs_sensor_create
 * @param state         the state from enum sensor_state.
 */
void zjs_sensor_set_state(jerry_value_t obj, sensor_state_t state);

/*
 * Trigger a change event so the sensor.onchange callback will be called
 *
 * @param obj           the sensor object that was created from
 *                        zjs_sensor_create
 */
void zjs_sensor_trigger_change(jerry_value_t obj);

/*
 * Trigger a error event so the sensor.onerror callback will be called
 *
 * @param obj           the sensor object that was created from
 *                        zjs_sensor_create
 * @param error_name    the custom error name
 * @param error_message the error message detail
 */
void zjs_sensor_trigger_error(jerry_value_t obj,
                              const char *error_name,
                              const char *error_message);

/** Initialize the sensor module, or reinitialize after cleanup */
void zjs_sensor_init();

/** Release resources held by the sensor module */
void zjs_sensor_cleanup();

#endif  // __zjs_sensor_h__
