// Copyright (c) 2017, Intel Corporation.

// Testing bmi160 temperature APIs
console.log("Testing bmi160 temperature APIs");

var genericSensor = require("GenericSensor.js");

var sensor = new TemperatureSensor({
    frequency: 20
});

genericSensor.test(sensor, "Temperature");
