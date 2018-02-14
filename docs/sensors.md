ZJS API for W3C Generic Sensors
==============================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
ZJS Generic Sensor API implements the W3C Sensor API, and it's intended to
provide a consistent API that allows apps to communicate with sensors like
accelerometer and gyroscope. Since the W3C Sensor API is still a draft spec,
our implementation only provide a subset of the API and the API could be
slightly different, but we try to follow the latest spec as closely as
possible.

Note: The currently supported hardware is Arduino 101 with its built-in
BMI160 chip with accelerometer, gyroscope, and temperature.  The supported
ambient light sensor is the Grove light sensor that comes with the
Grove starter kit, that can be connected using an analog pin.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

####Sensor Interface
```javascript
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
};

dictionary SensorOptions {
    double frequency;  // desire frequency, default is 20 if unset
};

interface SensorErrorEvent {
    attribute Error error;
};

callback ChangeCallback = void();
callback ActivateCallback = void();
callback ErrorCallback = void(SensorErrorEvent error);
```
####Accelerometer Interface
```javascript
[Constructor(optional AccelerometerOptions accelerometerOptions)]
interface Accelerometer : Sensor {
    readonly attribute double x;
    readonly attribute double y;
    readonly attribute double z;
};

dictionary AccelerometerOptions : SensorOptions  {
    string controller;       // controller name, default to "bmi160"
};
```
####GyroscopeSensor Interface
```javascript
[Constructor(optional SensorOptions sensorOptions)]
interface GyroscopeSensor : Sensor {
    readonly attribute double x;
    readonly attribute double y;
    readonly attribute double z;
};

dictionary GyroscopeOptions : SensorOptions  {
    string controller;  // controller name, default to "bmi160"
};
```
####AmbientLightSensor Interface
```javascript
[Constructor(optional SensorOptions sensorOptions)]
interface AmbientLightSensor : Sensor {
    readonly attribute unsigned long pin;
    readonly attribute double illuminance;
};

dictionary AmbientLightSensorOptions : SensorOptions  {
    string controller;  // controller name, default to "ADC_0"
    unsigned long pin;  // analog pin where the light is connected
};
```
####TemperatureSensor Interface
```javascript
[Constructor(optional SensorOptions sensorOptions)]
interface TemperatureSensor : Sensor {
    readonly attribute double celsius;
};

dictionary TemperatureSensorOptions : SensorOptions  {
    string controller;  // controller name, default to "bmi160"
};
```

API Documentation
-----------------

### onreading
`Sensor.onreading`

The onreading attribute is an EventHandler, which is called whenever a new reading is available.

### onactivate
`Sensor.onactivate`

The onactivate attribute is an EventHandler which is called when the sensor is activated after calling start().

### onerror
`Sensor.onerror`

The onactivate attribute is an EventHandler which is called whenever an exception cannot be handled synchronously.

### start
`void Sensor.start()`

Starts the sensor instance, the sensor will get callback on onreading whenever there's a new reading available.

### stop
`void Sensor.stop()`

Stop the sensor instance, the sensor will stop reporting new readings.

Sample Apps
-----------
* [Accelerometer sample](../samples/BMI160Accelerometer.js)
* [Gyroscope sample](../samples/BMI160Gyroscope.js)
* [Ambient Light sample](../samples/AmbientLight.js)
* [Temperature sample](../samples/BMI160Temperature.js)
