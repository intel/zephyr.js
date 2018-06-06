ZJS API for W3C Generic Sensors
==============================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [Class: Sensor](#sensor-api)
  * [onreading](#onreading)
  * [onactivate](#onactivate)
  * [onerror](#onerror)
  * [sensor.start()](#sensorstart)
  * [sensor.stop()](#sensorstop)
* [Sample Apps](#sample-apps)

Introduction
------------
ZJS Generic Sensor API implements the W3C Sensor API, and it's intended to
provide a consistent API that allows apps to communicate with sensors like
an accelerometer or gyroscope. Since the W3C Sensor API is still a draft spec,
our implementation only provides a subset of the API -- and this API could be
slightly different even though we try to follow the latest spec as closely as
possible.

Note: The currently supported hardware is Arduino 101 that has a
built-in BMI160 chip with accelerometer, gyroscope, and temperature
sensors.  The supported ambient light sensor is the Grove light sensor
that comes with the Grove starter kit.  The light sensor can be
connected using an analog pin.

Web IDL
-------

This IDL provides an overview of the interface; see below for
documentation of specific API functions.  We have a short document
explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).

<details>
<summary>Click to show WebIDL</summary>
<pre>
interface Sensor {
    readonly attribute boolean activated;   // whether the sensor is activated or not
    readonly attribute boolean hasReading;  // whether the sensor has readings available
    readonly attribute double timestamp;    // timestamp of the latest reading in milliseconds
    attribute double frequency;             // sampling frequency in hertz
    void start();                           // starts the sensor
    void stop();                            // stops the sensor
    attribute ChangeCallback onreading;     // callback handler for change events
    attribute ActivateCallback onactivate;  // callback handler for activate events
    attribute ErrorCallback onerror;        // callback handler for error events
};<p>dictionary SensorOptions {
    double frequency;  // desired frequency, default is 20 if unset
};<p>interface SensorErrorEvent {
    attribute Error error;
};<p>callback ChangeCallback = void();
callback ActivateCallback = void();
callback ErrorCallback = void(SensorErrorEvent error);<p>[Constructor(optional AccelerometerOptions accelerometerOptions)]
interface Accelerometer : Sensor {
    readonly attribute double x;
    readonly attribute double y;
    readonly attribute double z;
};<p>dictionary AccelerometerOptions : SensorOptions  {
    string controller;       // controller name, default to "bmi160"
};<p>[Constructor(optional SensorOptions sensorOptions)]
interface GyroscopeSensor : Sensor {
    readonly attribute double x;
    readonly attribute double y;
    readonly attribute double z;
};<p>
dictionary GyroscopeOptions : SensorOptions  {
    string controller;  // controller name, default to "bmi160"
};<p>
[Constructor(optional SensorOptions sensorOptions)]
interface AmbientLightSensor : Sensor {
    readonly attribute unsigned long pin;
    readonly attribute double illuminance;
};<p>dictionary AmbientLightSensorOptions : SensorOptions  {
    string controller;  // controller name, default to "ADC_0"
    unsigned long pin;  // analog pin where the light is connected
};<p>[Constructor(optional SensorOptions sensorOptions)]
interface TemperatureSensor : Sensor {
    readonly attribute double celsius;
};<p>dictionary TemperatureSensorOptions : SensorOptions  {
    string controller;  // controller name, default to "bmi160"
};</pre></details>

Sensor API
----------

### onreading
`Sensor.onreading`

The onreading attribute is an EventHandler, which is called whenever a new reading is available.

### onactivate
`Sensor.onactivate`

The onactivate attribute is an EventHandler which is called when the sensor is activated after calling start().

### onerror
`Sensor.onerror`

The onactivate attribute is an EventHandler which is called whenever an exception cannot be handled synchronously.

### sensor.start()

Starts the sensor instance, the sensor will get callback on onreading whenever there's a new reading available.

### sensor.stop()

Stop the sensor instance, the sensor will stop reporting new readings.

Sample Apps
-----------
* [Accelerometer sample](../samples/BMI160Accelerometer.js)
* [Gyroscope sample](../samples/BMI160Gyroscope.js)
* [Ambient Light sample](../samples/AmbientLight.js)
* [Temperature sample](../samples/BMI160Temperature.js)
