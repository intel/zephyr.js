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
slightly different, but we try to follow the latest spec as closely as possible.

The currently supported hardware are Arduino 101s with its built-in BMI160
accelerometer and gyroscope, also ambient light sensor that can be connected
using analog pin.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

Sensor Interface
```javascript
interface Sensor {
    readonly attribute SensorState state;   // The current state of Sensor object
    void start();                           // Starts the sensor
    void stop();                            // Stops the sensor
    attribute ChangeCallback onchange;      // Callback handler for change events
    attribute ActivateCallback onactivate;  // Callback handler for activate events
    attribute ErrorCallback onerror;        // Callback handler for error events
};

dictionary SensorOptions {
    double frequency;  // The requested polling frequency, default is 20 if unset
};

enum SensorState {
    "unconnected",
    "activating",
    "activated",
    "idle",
    "errored"
};

interface SensorErrorEvent {
    attribute Error error;
};

callback ChangeCallback = void(SensorReading value);
callback ActivateCallback = void();
callback ErrorCallback = void(SensorErrorEvent error);
```
Accelerometer Interface
```javascript
[Constructor(optional AccelerometerOptions accelerometerOptions)]
interface **Accelerometer** : Sensor {
    attribute AccelerometerReading reading;
    attribute boolean includesGravity;
};

dictionary AccelerometerOptions : SensorOptions  {
   boolean includeGravity = true;  // not supported, will throw an error if set
};

interface AccelerometerrReading : SensorReading {
    readonly attribute double x;
    readonly attribute double y;
    readonly attribute double z;
};

callback ChangeCallback = void(AccelerometerReading value);
```

GyroscopeSensor Interface
```javascript
[Constructor(optional SensorOptions sensorOptions)]
interface **GyroscopeSensor** : Sensor {
    attribute GyroscopeSensorReading reading;
};

interface GyroscopeSensorReading : SensorReading {
    readonly attribute double x;
    readonly attribute double y;
    readonly attribute double z;
};

callback ChangeCallback = void(GyroscopeReading value);
```
AmbientLightSensor Interface
```javascript
[Constructor(optional SensorOptions sensorOptions)]
interface **AmbientLightSensor** : Sensor {
    attribute AmbientLightSensorReading reading;
};

dictionary AmbientLightSensorOptions : SensorOptions  {
    unsigned long pin;  // analog pin where the light is connected
};

interface AmbientLightSensorReading : SensorReading {
    readonly attribute double illuminance;
};

callback ChangeCallback = void(AmbientLightSensorReading value);
```

API Documentation
-----------------

### onchange
`Sensor.onchange`

The onchange attribute is an EventHandler which is called whenever a new reading is available.

### onactivate
`Sensor.onactivate`

The onactivate attribute is an EventHandler which is called when this.[[state]] transitions from "activating" to "activated".

### onerror
`Sensor.onerror`

The onactivate attribute is an EventHandler which is called whenever an exception cannot be handled synchronously.

### start
`void Sensor.start()`

Starts the sensor instance, the sensor will get callback on onchange whenever there's a new reading available.

### stop
`void Sensor.stop()`

Stop the sensor instance, the sensor will stop reporting new readings.

Sample Apps
-----------
* [Accelerometer sample](../samples/Accelerometer.js)
* [Gyroscope sample](../samples/Gyroscope.js)
* [Ambient Light sample](../samples/AmbientLight.js)
