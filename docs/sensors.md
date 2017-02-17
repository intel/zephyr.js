ZJS API for W3C Generic Sensors
==============================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
ZJS Generic Sensor API implements the W3C Sensor API, and it's intended to
provide a consistent API that allow apps to communicate with sensors like
accelerometer and gyroscope. Since the W3C Sensor API is still a draft spec,
our implementation only provide a subset of the API and the API could be
slightly different, but we try to follow the latest spec as close as possible.

The currently supported hardware are Arduino 101s with its built-in BMI160
accelerometer and gyroscope, also ambient light sensor that can be connected
using analog pin.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

Sensor Interface
```javascript
interface Sensor : EventTarget {
  readonly attribute SensorState state; // The current state of Sensor object
  void start();                         // Starts the sensor
  void stop();                          // Stops the sensor
  attribute EventHandler onchange;      // Event handler for change events
  attribute EventHandler onactivate;    // Event handler for activate events
  attribute EventHandler onerror;       // Event handler for error events
};

dictionary SensorOptions {
  double frequency;                    // The requested polling frequency, default is 20 if unset
};

enum SensorState {
  "unconnected",
  "activating",
  "activated",
  "idle",
  "errored"
};

interface SensorErrorEvent : Event {
  readonly attribute Error error;
};

dictionary SensorErrorEventInit : EventInit {
  required Error error;
};
```
AccelerometerSensor Interface
```javascript
[Constructor(optional AccelerometerSensorOptions accelerometerSensorOptions)]
interface AccelerometerSensor : Sensor {
  readonly attribute AccelerometerSensorReading reading;
  readonly attribute boolean includesGravity;
};

dictionary AccelerometerSensorOptions : SensorOptions  {
  boolean includeGravity = true; // not supported, will throw an error if set
};

[Constructor(AccelerometerSensorReadingInit AccelerometerSensorReadingInit)]
interface AccelerometerSensorReading : SensorReading {
    readonly attribute double x;
    readonly attribute double y;
    readonly attribute double z;
};

dictionary AccelerometerSensorReadingInit {
    double x = 0;
    double y = 0;
    double z = 0;
};
```

GyroscopeSensor Interface
```javascript
[Constructor(optional SensorOptions sensorOptions)]
interface GyroscopeSensor : Sensor {
  readonly attribute GyroscopeSensorReading reading;
};

[Constructor(GyroscopeSensorReadingInit GyroscopeSensorReadingInit)]
interface GyroscopeSensorReading : SensorReading {
    readonly attribute unrestricted double x;
    readonly attribute unrestricted double y;
    readonly attribute unrestricted double z;
};

dictionary GyroscopeSensorReadingInit {
    unrestricted double x = 0;
    unrestricted double y = 0;
    unrestricted double z = 0;
};
```
AmbientLightSensor Interface
```javascript
[Constructor(optional SensorOptions sensorOptions)]
interface AmbientLightSensor : Sensor {
  readonly attribute AmbientLightSensorReading reading;
};

dictionary AmbientLightSensorOptions : SensorOptions  {
    unsigned long pin; // analog pin where the light is connected
};

[Constructor(AmbientLightSensorReadingInit ambientLightSensorReadingInit)]
interface AmbientLightSensorReading : SensorReading {
    readonly attribute unrestricted double illuminance;
};

dictionary AmbientLightSensorReadingInit {
  unrestricted double illuminance;
};
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

The onactivate attribute is an EventHandler which is called when this.[[state]] transitions from "activating" to "activated".

### start
`void Sensor.start()`

Starts the sensor instace, the senseor will get callback on onchange whenever there's a new reading available.

### stop
`void Sensor.stop()`

Stop the sensor instace, the sensor will stop reporting new readings.

Sample Apps
-----------
* [Accelerometer sample](../samples/Accelerometer.js)
* [Gyroscope sample](../samples/Gyroscope.js)
* [Ambient Light sample](../samples/AmbientLight.js)
