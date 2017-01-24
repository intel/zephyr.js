// Copyright (c) 2016, Intel Corporation.

// Testing accelerometer APIs
console.log("Testing accelerometer APIs");

var genericSensor = require("GenericSensor.js");

var sensor = new Accelerometer({
    includeGravity: false,
    frequency: 50
});

genericSensor.test(sensor, "Accelerometer");
