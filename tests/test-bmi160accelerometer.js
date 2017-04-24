// Copyright (c) 2016-2017, Intel Corporation.

// Testing bmi160 accelerometer APIs
console.log("Testing bmi160 accelerometer APIs");

var genericSensor = require("GenericSensor.js");

var sensor = new Accelerometer({
    includeGravity: false,
    frequency: 50
});

genericSensor.test(sensor, "Accelerometer");
