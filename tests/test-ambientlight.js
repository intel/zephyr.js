// Copyright (c) 2016-2017, Intel Corporation.

// Hardware Requirements:
//   - A Grove Light sensor
// Wiring:
//   - Wire the sensor's power to Arduino 3.3V or 5V and ground to GND
//   - Wire the signal pin to Arduino A2

// Testing ambientlight APIs
console.log("Testing ambientlight APIs");

var genericSensor = require("GenericSensor.js");

var sensor = new AmbientLightSensor({
    pin: 'A2',
    frequency: 50
});

genericSensor.test(sensor, "AmbientLight");
