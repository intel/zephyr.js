// Copyright (c) 2016-2017, Intel Corporation.

// Testing bmi160 gyroscope APIs
console.log("Testing bmi160 gyroscope APIs");

var genericSensor = require("GenericSensor.js");

var sensor = new Gyroscope({
    frequency: 50
});

genericSensor.test(sensor, "Gyroscope");
